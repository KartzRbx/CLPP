#include "core/binder/binder.hpp"

#include "clpp/stdlib.hpp"
#include "clpp/numeric.hpp"
#include "core/typechecker/solver.hpp"
#include "core/compiler/module_types.hpp"

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace clpp::binder {

namespace {

struct Local {
  std::string name;
  std::uint16_t slot{0};
  std::string type_name;
  bool constant{false};
  std::string value{};
  bool observable{false};
  std::string optional_origin{};
  bool global{false};
};

struct FuncRef {
  std::string name;
  std::uint16_t arity{0};
  std::uint16_t index{0};
  bool is_async{false};
  bool is_extern{false};
  bool is_private{false};
  bool is_abstract{false};
  bool is_override{false};
  bool is_final{false};
  std::uint16_t extern_id{0};
  std::string return_type{};
  std::vector<std::string> type_params{};
  std::size_t owner_type_param_count{0};
};

// The functions of the program being bound, so `make().field` can find make's return type.
thread_local const std::vector<FuncRef>* g_bound_functions = nullptr;
thread_local const parser::Function* active_function = nullptr;
thread_local std::vector<Local>* module_globals = nullptr;

struct StructRef {
  std::string name;
  std::vector<std::string> fields;
  std::vector<std::string> field_owners;
  std::vector<std::string> field_types;
  std::vector<std::string> type_params{};
  std::vector<std::shared_ptr<parser::Expr>> field_defaults;
  std::vector<char> private_fields;
  std::vector<std::string> method_names;
  std::vector<std::uint16_t> vtable;
  std::uint16_t index{0};
  bool is_abstract{false};
};

struct EnumRef {
  bool scoped{false};
  std::string name;
  std::vector<std::string> variants;
};


thread_local std::vector<std::vector<std::uint16_t>>* active_listeners = nullptr;
thread_local const std::string* active_owner = nullptr;
thread_local parser::Program* active_program = nullptr;
thread_local std::vector<const parser::Expr*> pending_defaults;
thread_local std::vector<const parser::Expr*> bound_defaults;

[[nodiscard]] parser::Expr clone_expr(const parser::Expr& expr) {
  parser::Expr copy;
  copy.kind = expr.kind;
  copy.value = expr.value;
  copy.default_scope = expr.default_scope;
  copy.number = expr.number;
  copy.integer = expr.integer; copy.exact_integer = expr.exact_integer;
  copy.numeric_promotion = expr.numeric_promotion;
  copy.float_literal = expr.float_literal;
  copy.bool_literal = expr.bool_literal;
  copy.null_literal = expr.null_literal;
  copy.slot = expr.slot;
  copy.global = expr.global;
  copy.span = expr.span;
  copy.location = expr.location;
  copy.enum_literal = expr.enum_literal;
  copy.named_initializer = expr.named_initializer;
  copy.scoped_member = expr.scoped_member;
  copy.scoped_type_candidate = expr.scoped_type_candidate;
  copy.async_call = expr.async_call;
  copy.signal_call = expr.signal_call;
  copy.thread_call = expr.thread_call;
  copy.extern_call = expr.extern_call;
  copy.int_specialized = expr.int_specialized;
  copy.bool_left = expr.bool_left;
  copy.bool_right = expr.bool_right;
  copy.virtual_call = expr.virtual_call;
  copy.missing = expr.missing;
  copy.runtime_op = expr.runtime_op;
  copy.targets = expr.targets;
  copy.arg_names = expr.arg_names;
  copy.postfix = expr.postfix;

  if (expr.left != nullptr) {
    copy.left = std::make_unique<parser::Expr>(clone_expr(*expr.left));
  }
  if (expr.right != nullptr) {
    copy.right = std::make_unique<parser::Expr>(clone_expr(*expr.right));
  }
  for (const parser::Expr& arg : expr.args) {
    copy.args.push_back(clone_expr(arg));
  }
  return copy;
}

void narrow_optional(const parser::Expr& condition, const bool truth, std::vector<Local>& locals) {
  if (condition.kind == parser::Expr::Kind::Not && condition.left != nullptr) {
    narrow_optional(*condition.left, !truth, locals);
    return;
  }
  if (((condition.kind == parser::Expr::Kind::And && truth) ||
       (condition.kind == parser::Expr::Kind::Or && !truth)) && condition.left != nullptr && condition.right != nullptr) {
    narrow_optional(*condition.left, truth, locals);
    narrow_optional(*condition.right, truth, locals);
    return;
  }
  if (condition.kind == parser::Expr::Kind::TypeTest && condition.left &&
      condition.left->kind == parser::Expr::Kind::Name && truth) {
    for (auto local = locals.rbegin(); local != locals.rend(); ++local) {
      if (local->name != condition.left->value || (condition.left->global && !local->global)) continue;
      if (local->optional_origin.empty()) local->optional_origin = local->type_name;
      local->type_name = condition.value;
      break;
    }
    return;
  }
  if (condition.kind == parser::Expr::Kind::Name && truth) {
    for (auto local = locals.rbegin(); local != locals.rend(); ++local) {
      if (local->name != condition.value) continue;
      if (local->type_name.starts_with("optional<") && local->type_name.back() == '>') {
        local->optional_origin = local->type_name;
        local->type_name = local->type_name.substr(9, local->type_name.size() - 10);
      }
      break;
    }
    return;
  }
  if (condition.kind != parser::Expr::Kind::Eq && condition.kind != parser::Expr::Kind::NotEq) return;
  if (condition.left == nullptr || condition.right == nullptr) return;
  const parser::Expr* name = condition.left.get();
  const parser::Expr* value = condition.right.get();
  if (name->null_literal) std::swap(name, value);
  if (name->kind != parser::Expr::Kind::Name || !value->null_literal ||
      ((condition.kind == parser::Expr::Kind::NotEq) != truth)) return;
  for (auto local = locals.rbegin(); local != locals.rend(); ++local) {
    if (local->name != name->value) continue;
    if (local->type_name.starts_with("optional<") && local->type_name.back() == '>') {
      local->optional_origin = local->type_name;
      local->type_name = local->type_name.substr(9, local->type_name.size() - 10);
    }
    break;
  }
}

[[nodiscard]] std::string bare_type(std::string_view name) {
  const std::size_t open = name.find('<');
  if (open == std::string_view::npos) {
    return std::string(name);
  }
  return std::string(name.substr(0, open));
}

[[nodiscard]] bool declared_since(const std::vector<Local>& locals, const std::size_t watermark, const std::string_view name) {
  for (std::size_t index = watermark; index < locals.size(); ++index) {
    if (locals[index].name == name) {
      return true;
    }
  }
  return false;
}


[[nodiscard]] int find_local(const std::vector<Local>& locals, const std::string_view name) {
  for (std::size_t index = locals.size(); index > 0; --index) {
    if (locals[index - 1].name == name) {
      return static_cast<int>(locals[index - 1].slot);
    }
  }
  return -1;
}

[[nodiscard]] const Local* find_local_entry(const std::vector<Local>& locals, const std::string_view name) {
  for (std::size_t index = locals.size(); index > 0; --index) {
    if (locals[index - 1].name == name) {
      return &locals[index - 1];
    }
  }
  return nullptr;
}

[[nodiscard]] const Local* expression_local(const std::vector<Local>& locals, const parser::Expr& expr) {
  if (expr.global) {
    for (auto local = locals.rbegin(); local != locals.rend(); ++local)
      if (local->global && local->name == expr.value) return &*local;
    return nullptr;
  }
  return find_local_entry(locals, expr.value);
}

[[nodiscard]] int native_id(const std::string_view name, const std::size_t arity) {
  if (const int axiom = stdlib::axiom_native(name, arity); axiom >= 0) {
    return axiom;
  }
  if (const int host = stdlib::host_native(name, arity); host >= 0) {
    return host;
  }
  if (name == "Vector3" && arity == 3) {
    return 0;
  }
  if (name == "Vector2" && arity == 2) {
    return 7;
  }
  if (name == "Vector4" && arity == 4) {
    return 8;
  }
  if (name == "buffer::create" && arity == 1) {
    return 1;
  }
  if (name == "buffer::write_string" && arity == 3) {
    return 2;
  }
  if (name == "buffer::size" && arity == 1) {
    return 3;
  }
  if (name == "fs::size" && arity == 1) {
    return 4;
  }
  if (name == "os::env" && arity == 1) {
    return 5;
  }
  if (name == "http::host" && arity == 1) {
    return 6;
  }
  if (name == "string::trim" && arity == 1) {
    return 9;
  }
  if (name == "string::lower" && arity == 1) {
    return 10;
  }
  if (name == "string::upper" && arity == 1) {
    return 11;
  }
  if (name == "string::contains" && arity == 2) {
    return 12;
  }
  if (name == "fs::read" && arity == 1) {
    return 13;
  }
  if (name == "fs::list" && arity == 1) {
    return 14;
  }
  if (name == "args" && arity == 0) {
    return 15;
  }
  // text helpers behind @clpp.text (std ids 7..14)
  if (name == "string::split" && arity == 2) {
    return 16;
  }
  if (name == "string::replace" && arity == 3) {
    return 17;
  }
  if (name == "string::starts_with" && arity == 2) {
    return 18;
  }
  if (name == "string::ends_with" && arity == 2) {
    return 19;
  }
  if (name == "string::repeat" && arity == 2) {
    return 20;
  }
  if (name == "string::to_number" && arity == 1) {
    return 21;
  }
  if (name == "string::index_of" && arity == 2) {
    return 22;
  }
  if (name == "string::join_list" && arity == 2) {
    return 23;
  }
  return -1;
}

[[nodiscard]] int find_func(const std::vector<FuncRef>& functions, const std::string_view name) {
  for (const FuncRef& function : functions) {
    if (function.name == name) {
      return static_cast<int>(function.index);
    }
  }
  return -1;
}

[[nodiscard]] const StructRef* find_struct(const std::vector<StructRef>& structs, const std::string_view name) {
  for (const StructRef& decl : structs) {
    if (decl.name == name) {
      return &decl;
    }
  }
  return nullptr;
}

[[nodiscard]] const StructRef* struct_at(const std::vector<StructRef>& structs, const std::uint16_t index) {
  for (const StructRef& decl : structs) {
    if (decl.index == index) {
      return &decl;
    }
  }
  return nullptr;
}

[[nodiscard]] const EnumRef* find_enum(const std::vector<EnumRef>& enums, const std::string_view name) {
  for (const EnumRef& decl : enums) {
    if (decl.name == name) {
      return &decl;
    }
  }
  return nullptr;
}

[[nodiscard]] int find_field(const StructRef& decl, const std::string_view name) {
  for (std::size_t index = 0; index < decl.fields.size(); ++index) {
    if (decl.fields[index] == name) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

[[nodiscard]] int find_variant(const EnumRef& decl, const std::string_view name) {
  for (std::size_t index = 0; index < decl.variants.size(); ++index) {
    if (decl.variants[index] == name) {
      return static_cast<int>(index);
    }
  }
  return -1;
}

void bind_stmts(std::vector<parser::Stmt>&, std::vector<Local>&, const std::vector<FuncRef>&,
                const std::vector<StructRef>&, const std::vector<EnumRef>&, std::size_t&, std::vector<Diagnostic>&);


// Establish type ownership before inference queries build anonymous signatures.
void record_lexical_stmts(const std::vector<parser::Stmt>& body, parser::Program& program,
                          const std::vector<FuncRef>& functions, std::uint16_t owner);
void record_lexical_expr(const parser::Expr& expr, parser::Program& program,
                         const std::vector<FuncRef>& functions, const std::uint16_t owner) {
  if (expr.kind == parser::Expr::Kind::Closure) {
    const auto index = find_func(functions, expr.value);
    if (index >= 0 && static_cast<std::size_t>(index) < program.functions.size()) {
      auto& nested = program.functions[index];
      if (nested.anonymous) {
        nested.lexical_parent = owner;
        record_lexical_stmts(nested.body, program, functions, nested.index);
      }
    }
  }
  if (expr.left) record_lexical_expr(*expr.left, program, functions, owner);
  if (expr.right) record_lexical_expr(*expr.right, program, functions, owner);
  for (const auto& arg : expr.args) record_lexical_expr(arg, program, functions, owner);
}
void record_lexical_stmts(const std::vector<parser::Stmt>& body, parser::Program& program,
                          const std::vector<FuncRef>& functions, const std::uint16_t owner) {
  for (const auto& stmt : body) {
    record_lexical_expr(stmt.expr, program, functions, owner);
    record_lexical_expr(stmt.place, program, functions, owner);
    record_lexical_expr(stmt.type_expr, program, functions, owner);
    record_lexical_stmts(stmt.init, program, functions, owner);
    record_lexical_stmts(stmt.then_body, program, functions, owner);
    record_lexical_stmts(stmt.else_body, program, functions, owner);
    record_lexical_stmts(stmt.step, program, functions, owner);
    for (const auto& arm : stmt.arms) {
      record_lexical_expr(arm.pattern, program, functions, owner);
      record_lexical_expr(arm.guard, program, functions, owner);
      record_lexical_stmts(arm.body, program, functions, owner);
    }
  }
}

// Find lexical free names before introducing hidden capture parameters. A shadow only
// takes effect after its initializer; nested lambdas inherit the surrounding scope.
bool closure_uses(const std::vector<parser::Stmt>& body, const std::string& name,
                  std::vector<std::string> shadows);
bool closure_expr_uses(const parser::Expr& expr, const std::string& name,
                       const std::vector<std::string>& shadows) {
  const bool hidden = std::find(shadows.begin(), shadows.end(), name) != shadows.end();
  if (!hidden && ((expr.kind == parser::Expr::Kind::SelfField && name == "@this") || (expr.kind == parser::Expr::Kind::Name && expr.value == name) ||
      (expr.kind == parser::Expr::Kind::Call && (expr.value == name || expr.value.starts_with(name + "."))))) return true;
  if (expr.kind == parser::Expr::Kind::Closure && active_program) {
    for (const auto& nested : active_program->functions) if (nested.name == expr.value) {
      auto scope = shadows; scope.insert(scope.end(), nested.params.begin(), nested.params.end());
      if (closure_uses(nested.body, name, std::move(scope))) return true;
    }
  }
  if (expr.left && closure_expr_uses(*expr.left, name, shadows)) return true;
  if (expr.right && closure_expr_uses(*expr.right, name, shadows)) return true;
  for (const auto& arg : expr.args) if (closure_expr_uses(arg, name, shadows)) return true;
  return false;
}
bool closure_uses(const std::vector<parser::Stmt>& body, const std::string& name,
                  std::vector<std::string> shadows) {
  for (const auto& stmt : body) {
    if (stmt.kind == parser::Stmt::Kind::SelfAssign && name == "@this" &&
        std::find(shadows.begin(), shadows.end(), name) == shadows.end()) return true;
    if ((stmt.kind == parser::Stmt::Kind::Assign || stmt.kind == parser::Stmt::Kind::PlaceAssign) &&
        stmt.name == name && std::find(shadows.begin(), shadows.end(), name) == shadows.end()) return true;
    if (closure_expr_uses(stmt.expr, name, shadows) || closure_expr_uses(stmt.place, name, shadows) ||
        closure_expr_uses(stmt.type_expr, name, shadows)) return true;
    auto nested_scope = shadows;
    for (const auto& init : stmt.init) {
      if (closure_expr_uses(init.expr, name, nested_scope)) return true;
      if (init.kind == parser::Stmt::Kind::VariableDecl) nested_scope.push_back(init.name);
    }
    if (stmt.kind == parser::Stmt::Kind::For || stmt.kind == parser::Stmt::Kind::ForIn) nested_scope.push_back(stmt.name);
    if (closure_uses(stmt.then_body, name, nested_scope) || closure_uses(stmt.step, name, nested_scope) ||
        closure_uses(stmt.else_body, name, shadows)) return true;
    for (const auto& arm : stmt.arms) {
      auto arm_scope = shadows; if (!arm.bind_name.empty()) arm_scope.push_back(arm.bind_name);
      if (closure_expr_uses(arm.pattern, name, shadows) || closure_expr_uses(arm.guard, name, arm_scope) ||
          closure_uses(arm.body, name, arm_scope)) return true;
    }
    if (stmt.kind == parser::Stmt::Kind::VariableDecl) shadows.push_back(stmt.name);
  }
  return false;
}

void bind_expr(parser::Expr& expr, const std::vector<Local>& locals, const std::vector<FuncRef>& functions,
               const std::vector<StructRef>& structs, const std::vector<EnumRef>& enums,
               std::vector<Diagnostic>& diagnostics, const std::string& expected = {});

// Defaults resolve once in the declaration scope, never in a caller's locals.
[[nodiscard]] bool bind_default(parser::Expr& expr, const std::vector<FuncRef>& functions,
                                 const std::vector<StructRef>& structs, const std::vector<EnumRef>& enums,
                                 std::vector<Diagnostic>& diagnostics, const std::string& expected,
                                 const std::string& owner) {
  if (std::find(bound_defaults.begin(), bound_defaults.end(), &expr) != bound_defaults.end()) return true;
  if (std::find(pending_defaults.begin(), pending_defaults.end(), &expr) != pending_defaults.end()) {
    diagnostics.push_back(Diagnostic{expr.location, expr.span, "recursive field default"});
    return false;
  }
  pending_defaults.push_back(&expr);
  const auto* previous = active_function;
  const auto* previous_owner = active_owner;
  parser::Function context; context.index = 65535; context.type_owner = owner;
  for (const auto& data : structs) if (data.name == owner) context.type_params = data.type_params;
  active_function = &context; active_owner = &owner;
  const std::vector<Local> definition_scope = module_globals ? *module_globals : std::vector<Local>{};
  bind_expr(expr, definition_scope, functions, structs, enums, diagnostics, expected);
  active_function = previous; active_owner = previous_owner;
  pending_defaults.pop_back();
  bound_defaults.push_back(&expr);
  return true;
}

[[nodiscard]] std::string inferred_type(const parser::Expr& expr, const std::vector<Local>& locals,
                                        const std::vector<StructRef>& structs);

[[nodiscard]] std::string member_owner_spelling(const std::string& type, const std::vector<StructRef>& structs,
                                               const std::string& member = {}) {
  const parser::Function* owner = active_function;
  for (std::size_t depth = 0; owner && owner->anonymous && active_program &&
       owner->lexical_parent < active_program->functions.size() && depth < active_program->functions.size(); ++depth)
    owner = &active_program->functions[owner->lexical_parent];
  const bool formal = owner != nullptr &&
      std::find(owner->type_params.begin(), owner->type_params.end(), type) != owner->type_params.end();
  if ((!formal && find_struct(structs, bare_type(type)) != nullptr) || active_program == nullptr) return type;
  return typechecker::member_owner_type(*active_program, type, active_function, member).value_or(type);
}

[[nodiscard]] int struct_index_of(const parser::Expr& expr, const std::vector<Local>& locals,
                                  const std::vector<StructRef>& structs, const std::string& member = {}) {
  if (expr.kind == parser::Expr::Kind::Construct) {
    return static_cast<int>(expr.slot);
  }
  if (expr.kind == parser::Expr::Kind::Ascribe) {
    const StructRef* decl = find_struct(structs, bare_type(member_owner_spelling(expr.value, structs, member)));
    return decl == nullptr ? -1 : static_cast<int>(decl->index);
  }
  if (expr.kind == parser::Expr::Kind::Name) {
    const Local* local = expression_local(locals, expr);
    if (local == nullptr) {
      return -1;
    }
    const StructRef* decl = find_struct(structs, bare_type(member_owner_spelling(local->type_name, structs, member)));
    if (decl == nullptr) {
      return -1;
    }
    return static_cast<int>(decl->index);
  }
  if ((expr.kind == parser::Expr::Kind::Call || expr.kind == parser::Expr::Kind::Invoke || expr.kind == parser::Expr::Kind::Await) && g_bound_functions != nullptr) {
    const StructRef* decl = find_struct(structs, bare_type(member_owner_spelling(inferred_type(expr, locals, structs), structs, member)));
    return decl == nullptr ? -1 : static_cast<int>(decl->index);
  }
  if (expr.kind == parser::Expr::Kind::Member && expr.left != nullptr) {
    const StructRef* decl = find_struct(structs, bare_type(member_owner_spelling(inferred_type(expr, locals, structs), structs, member)));
    return decl == nullptr ? -1 : static_cast<int>(decl->index);
  }
  if (expr.kind == parser::Expr::Kind::Index) {
    const StructRef* decl = find_struct(structs, bare_type(member_owner_spelling(inferred_type(expr, locals, structs), structs, member)));
    return decl == nullptr ? -1 : static_cast<int>(decl->index);
  }
  return -1;
}

// Method receiver bindings are named @this internally, which cannot collide with
// a plain source identifier. Legacy @field handling remains a separate cleanup.
[[nodiscard]] bool self_has_field(const std::vector<Local>& locals, const std::vector<StructRef>& structs,
                                  const std::string& field) {
  const Local* self = find_local_entry(locals, "@this");
  if (self == nullptr) {
    return false;
  }
  const StructRef* decl = find_struct(structs, bare_type(self->type_name));
  return decl != nullptr && find_field(*decl, field) >= 0;
}

[[nodiscard]] parser::Expr self_member(const std::string& field, const std::string_view span, const SourceLocation location) {
  parser::Expr receiver;
  receiver.kind = parser::Expr::Kind::Name;
  receiver.value = "@this";
  receiver.span = span;
  receiver.location = location;
  parser::Expr member;
  member.kind = parser::Expr::Kind::Member;
  member.value = field;
  member.span = span;
  member.location = location;
  member.left = std::make_unique<parser::Expr>(std::move(receiver));
  return member;
}

// Bind evidence first so an earlier lambda can use a later ordinary argument's type.
void bind_arguments(parser::Expr& expr, const std::vector<Local>& locals, const std::vector<FuncRef>& functions,
                    const std::vector<StructRef>& structs, const std::vector<EnumRef>& enums,
                    std::vector<Diagnostic>& diagnostics) {
  std::vector<bool> deferred(expr.args.size(), false);
  for (std::size_t index = 0; index < expr.args.size(); ++index) {
    deferred[index] = expr.args[index].kind == parser::Expr::Kind::Closure ||
        (expr.args[index].kind == parser::Expr::Kind::MakeList && expr.args[index].value == "$array_initializer");
    if (!deferred[index]) bind_expr(expr.args[index], locals, functions, structs, enums, diagnostics);
  }
  std::vector<std::string> expected;
  if (active_program && std::any_of(deferred.begin(), deferred.end(), [](bool value) { return value; })) {
    std::vector<typechecker::LocalType> types;
    for (const auto& local : locals) types.push_back({local.name, local.type_name, local.global});
    expected = typechecker::argument_types(*active_program, expr, types, active_function);
  }
  for (std::size_t index = 0; index < expr.args.size(); ++index)
    if (deferred[index]) bind_expr(expr.args[index], locals, functions, structs, enums, diagnostics,
                                index < expected.size() ? expected[index] : std::string{});
}

// Resolve methods from the receiver's declared type, independent of its expression shape.
void bind_method(parser::Expr& expr, parser::Expr receiver, const std::vector<Local>& locals,
                 const std::vector<FuncRef>& functions, const std::vector<StructRef>& structs,
                 const std::vector<EnumRef>& enums, std::vector<Diagnostic>& diagnostics) {
  if (expr.thread_call) { diagnostics.push_back(Diagnostic{expr.location, expr.span, "extract a method callback before using thread"}); return; }
  bind_expr(receiver, locals, functions, structs, enums, diagnostics);
  const int type_index = struct_index_of(receiver, locals, structs, bare_type(expr.value));
  const StructRef* type = type_index < 0 ? nullptr : struct_at(structs, static_cast<std::uint16_t>(type_index));
  if (type == nullptr) {
    diagnostics.push_back(Diagnostic{expr.location, expr.span, "unknown receiver type"});
    return;
  }
  const auto generic = expr.value.find('<');
  const std::string supplied = generic == std::string::npos ? std::string{} : expr.value.substr(generic);
  const auto method = std::find(type->method_names.begin(), type->method_names.end(), bare_type(expr.value));
  const std::size_t slot = static_cast<std::size_t>(method - type->method_names.begin());
  const FuncRef* target = nullptr;
  if (method != type->method_names.end() && slot < type->vtable.size()) {
    for (const FuncRef& symbol : functions) {
      if (symbol.index == type->vtable[slot]) { target = &symbol; break; }
    }
  }
  if (target == nullptr) {
    diagnostics.push_back(Diagnostic{expr.location, expr.span, "undefined function"});
    return;
  }
  const std::string owner = target->name.substr(0, target->name.rfind('.'));
  if (target->is_private && (active_owner == nullptr || *active_owner != owner)) {
    diagnostics.push_back(Diagnostic{expr.location, expr.span, "private member"});
    return;
  }
  if (expr.args.size() + 1 != target->arity) {
    diagnostics.push_back(Diagnostic{expr.location, expr.span, "wrong number of arguments"});
    return;
  }
  expr.args.insert(expr.args.begin(), std::move(receiver));
  expr.virtual_call = true;
  expr.slot = static_cast<std::uint16_t>(slot);
  expr.value = target->name + supplied;
  expr.async_call = target->is_async;
  bind_arguments(expr, locals, functions, structs, enums, diagnostics);
}

void bind_expr(parser::Expr& expr, const std::vector<Local>& locals, const std::vector<FuncRef>& functions,
               const std::vector<StructRef>& structs, const std::vector<EnumRef>& enums,
               std::vector<Diagnostic>& diagnostics, const std::string& expected) {
  if (expr.kind == parser::Expr::Kind::Call && expr.scoped_type_candidate && !expr.left &&
      find_local(locals, expr.value) < 0 && active_program) {
    const auto root = bare_type(expr.value);
    const bool alias = std::any_of(active_program->aliases.begin(), active_program->aliases.end(),
                                  [&](const auto& declaration) { return declaration.name == root; });
    const bool data = find_struct(structs, root) != nullptr || find_enum(enums, root) != nullptr;
    if ((alias || data) && !expr.named_initializer && expr.args.size() == 1) {
      expr.kind = parser::Expr::Kind::Ascribe;
      expr.left = std::make_unique<parser::Expr>(std::move(expr.args[0]));
      expr.args.clear();
    } else if (expr.named_initializer && expr.args.empty() &&
               typechecker::array_initializer_type(*active_program, expr.value, active_function)) {
      expr.kind = parser::Expr::Kind::Ascribe;
      expr.left = std::make_unique<parser::Expr>();
      expr.left->kind = parser::Expr::Kind::MakeList;
      expr.left->value = "$array_initializer";
      expr.left->location = expr.location; expr.left->span = expr.span;
    }
  }
  if (expr.kind == parser::Expr::Kind::TypeTest) {
    if (expr.left) bind_expr(*expr.left, locals, functions, structs, enums, diagnostics);
    return;
  }
  if (expr.kind == parser::Expr::Kind::Ascribe) {
    if (expr.left) bind_expr(*expr.left, locals, functions, structs, enums, diagnostics, expr.value);
    return;
  }
  if (expr.kind == parser::Expr::Kind::MakeList && expr.value == "$array_initializer") {
    const auto type = active_program ? typechecker::array_initializer_type(*active_program, expected, active_function) : std::nullopt;
    if (!type) {
      diagnostics.push_back(Diagnostic{expr.location, expr.span, "array initializer requires an explicit array type context"});
      return;
    }
    expr.value = *type;
    bind_arguments(expr, locals, functions, structs, enums, diagnostics);
    return;
  }
  if ((expr.kind == parser::Expr::Kind::And || expr.kind == parser::Expr::Kind::Or) &&
      expr.left != nullptr && expr.right != nullptr) {
    bind_expr(*expr.left, locals, functions, structs, enums, diagnostics);
    auto right_locals = locals;
    narrow_optional(*expr.left, expr.kind == parser::Expr::Kind::And, right_locals);
    bind_expr(*expr.right, right_locals, functions, structs, enums, diagnostics);
    return;
  }
  if (expr.kind == parser::Expr::Kind::SelfField && self_has_field(locals, structs, expr.value)) {
    expr = self_member(expr.value, expr.span, expr.location);
  }
  if (expr.kind == parser::Expr::Kind::Update) {
    if (!expr.left || expr.left->kind != parser::Expr::Kind::Name) {
      diagnostics.push_back(Diagnostic{expr.location, expr.span, "update requires a mutable binding"}); return;
    }
    if (expr.left != nullptr) {
      const auto* binding = expr.left->kind == parser::Expr::Kind::Name ? find_local_entry(locals, expr.left->value) : nullptr;
      if (binding && binding->constant) diagnostics.push_back(Diagnostic{expr.location, expr.span, "cannot assign to immutable binding"});
      bind_expr(*expr.left, locals, functions, structs, enums, diagnostics);
      expr.slot = expr.left->slot;
      expr.global = expr.left->global;
      expr.value = expr.left->value;
    }
    return;
  }
  if (expr.kind == parser::Expr::Kind::Closure) {
    const int index = find_func(functions, bare_type(expr.value));
    if (expr.virtual_call && expr.left) { bind_expr(*expr.left, locals, functions, structs, enums, diagnostics); return; }
    if (index < 0 || active_program == nullptr) {
      diagnostics.push_back(Diagnostic{expr.location, expr.span, "undefined callback"}); return;
    }
    expr.slot = static_cast<std::uint16_t>(index);
    auto& function = active_program->functions[index];
    if (!function.anonymous || function.closure_bound) return;
    if (const auto parameters = typechecker::callback_parameters(*active_program, expected, active_function);
        parameters && parameters->size() == function.params.size()) {
      for (std::size_t param = 0; param < function.params.size(); ++param)
        if (function.param_types[param].empty()) function.param_types[param] = (*parameters)[param];
    }
    function.closure_bound = true;
    function.lexical_parent = active_function ? active_function->index : 65535;
    if (active_function) function.type_owner = active_function->type_owner;
    std::vector<Local> scope = module_globals ? *module_globals : std::vector<Local>{};
    std::size_t next = function.params.size();
    for (std::size_t i = 0; i < function.params.size(); ++i) {
      scope.push_back(Local{function.params[i], static_cast<std::uint16_t>(i), function.param_types[i]});
    }
    for (auto it = locals.rbegin(); it != locals.rend(); ++it) {
      if (it->global || (find_local_entry(scope, it->name) && !find_local_entry(scope, it->name)->global) || !closure_uses(function.body, it->name, function.params)) continue;
      const auto capture_type = !it->constant && !it->optional_origin.empty() ? it->optional_origin : it->type_name;
      parser::Capture capture{it->name, capture_type, it->slot, it->constant};
      capture.source_scope = active_function ? active_function->index + 1u : 0u;
      capture.origin_slot = it->slot;
      if (active_function && it->slot >= active_function->params.size() &&
          it->slot < active_function->params.size() + active_function->captures.size()) {
        const auto& origin = active_function->captures[it->slot - active_function->params.size()];
        capture.source_scope = origin.source_scope; capture.origin_slot = origin.origin_slot;
      }
      function.captures.push_back(std::move(capture));
      auto captured = *it; captured.type_name = capture_type; captured.slot = static_cast<std::uint16_t>(next++); scope.push_back(std::move(captured));
      auto& boxed = active_function ? const_cast<parser::Function*>(active_function)->boxed_slots : active_program->boxed_slots;
      if (std::find(boxed.begin(), boxed.end(), it->slot) == boxed.end()) boxed.push_back(it->slot);
      function.boxed_slots.push_back(static_cast<std::uint16_t>(next - 1));
      expr.targets.push_back(it->slot);
    }
    const auto* previous = active_function;
    active_function = &function;
    bind_stmts(function.body, scope, functions, structs, enums, next, diagnostics);
    active_function = previous;
    function.local_count = static_cast<std::uint16_t>(next);
    return;
  }
  if (expr.kind == parser::Expr::Kind::Invoke) {
    if (expr.left) bind_expr(*expr.left, locals, functions, structs, enums, diagnostics);
    bind_arguments(expr, locals, functions, structs, enums, diagnostics);
    return;
  }
  if (expr.kind == parser::Expr::Kind::Event) { bind_arguments(expr, locals, functions, structs, enums, diagnostics); return; }
  if (expr.kind == parser::Expr::Kind::Name) {
    if (expr.global) return;  // Definition defaults retain their lexical binding when cloned.
    const auto dot = expr.value.find('.');
    if (dot != std::string::npos && find_local(locals, expr.value.substr(0, dot)) >= 0) {
      auto receiver = std::make_unique<parser::Expr>(); receiver->kind = parser::Expr::Kind::Name;
      receiver->value = expr.value.substr(0, dot); receiver->span = expr.span; receiver->location = expr.location;
      expr.value = expr.value.substr(dot + 1); expr.kind = parser::Expr::Kind::Member; expr.left = std::move(receiver);
      bind_expr(expr, locals, functions, structs, enums, diagnostics); return;
    }
    const int slot = find_local(locals, expr.value);
    if (slot < 0) {
      const auto name = bare_type(expr.value);
      const int function = find_func(functions, name);
      if (function >= 0) {
        const auto count = std::count_if(functions.begin(), functions.end(), [&](const FuncRef& candidate) { return candidate.name == name; });
        if (count != 1 || functions[function].is_extern ||
            functions[function].is_private || functions[function].is_abstract || name.find('.') != std::string::npos ||
            (!functions[function].type_params.empty() && expr.value.find('<') == std::string::npos)) {
          diagnostics.push_back(Diagnostic{expr.location, expr.span, "callback requires an unambiguous explicitly specialized function"}); return;
        }
        expr.kind = parser::Expr::Kind::Closure; expr.slot = static_cast<std::uint16_t>(function); return;
      }
      diagnostics.push_back(Diagnostic{expr.location, expr.span, "undefined name"});
      return;
    }
    expr.slot = static_cast<std::uint16_t>(slot);
    expr.global = find_local_entry(locals, expr.value)->global;
    return;
  }
  if (expr.kind == parser::Expr::Kind::String || expr.kind == parser::Expr::Kind::Number ||
      expr.kind == parser::Expr::Kind::SelfField) {
    return;
  }
  if (expr.kind == parser::Expr::Kind::Parallel) {
    for (parser::Expr& arg : expr.args) {
      bind_expr(arg, locals, functions, structs, enums, diagnostics);
    }
    return;
  }
  if (expr.kind == parser::Expr::Kind::MoveFrom) {
    if (expr.left == nullptr || expr.left->kind != parser::Expr::Kind::Name) {
      diagnostics.push_back(Diagnostic{expr.location, expr.span, "cannot move"});
      return;
    }
    const Local* local = find_local_entry(locals, expr.left->value);
    if (local == nullptr) {
      diagnostics.push_back(Diagnostic{expr.left->location, expr.left->span, "undefined name"});
    } else if (local->constant || local->global) {
      diagnostics.push_back(Diagnostic{expr.left->location, expr.left->span, local->global && !local->constant ? "cannot move from shared module binding" : "cannot move"});
    } else {
      expr.slot = local->slot;
      expr.value = local->name;
    }
    return;
  }
  if ((expr.kind == parser::Expr::Kind::Add || expr.kind == parser::Expr::Kind::Sub) && expr.left != nullptr &&
      expr.right != nullptr) {
    bind_expr(*expr.left, locals, functions, structs, enums, diagnostics);
    bind_expr(*expr.right, locals, functions, structs, enums, diagnostics);
    // Either side is a struct value: a variable, a constructor call (Money(150)) or a field (a.wallet).
    const auto struct_name = [&](const parser::Expr& side) -> std::string {
      const int index = struct_index_of(side, locals, structs);
      if (index >= 0) {
        for (const StructRef& candidate : structs) {
          if (candidate.index == static_cast<std::uint16_t>(index)) {
            return candidate.name;
          }
        }
      }
      return {};
    };
    if (!struct_name(*expr.left).empty() && !struct_name(*expr.right).empty()) {
      const char* operator_name = expr.kind == parser::Expr::Kind::Sub ? "operator-" : "operator+";
      const int function = find_func(functions, operator_name);
      std::uint16_t arity = 0;
      for (const FuncRef& candidate : functions) {
        if (candidate.name == operator_name && static_cast<int>(candidate.index) == function) {
          arity = candidate.arity;
        }
      }
      if (function < 0) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "undefined function"});
      } else if (arity != 2) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "wrong number of arguments"});
      } else {
        expr.kind = parser::Expr::Kind::Call;
        expr.value = operator_name;
        expr.slot = static_cast<std::uint16_t>(function);
        expr.args.clear();
        expr.args.push_back(std::move(*expr.left));
        expr.args.push_back(std::move(*expr.right));
        expr.left.reset();
        expr.right.reset();
      }
    }
    return;
  }
  if (expr.kind == parser::Expr::Kind::Join) {
    if (expr.left != nullptr) {
      bind_expr(*expr.left, locals, functions, structs, enums, diagnostics);
    }
    return;
  }
  if (expr.kind == parser::Expr::Kind::Call) {
    if (bare_type(expr.value) == "signal" && !expr.left) {
      expr.kind = parser::Expr::Kind::Event; expr.runtime_op = 0;
      bind_arguments(expr, locals, functions, structs, enums, diagnostics); return;
    }
    if (!expr.left) {
      const auto separator = expr.value.find("::");
      if (separator != std::string::npos && find_local(locals, expr.value.substr(0, separator)) >= 0) {
        auto receiver = std::make_unique<parser::Expr>(); receiver->kind = parser::Expr::Kind::Name;
        receiver->value = expr.value.substr(0, separator); receiver->span = expr.span; receiver->location = expr.location;
        expr.value = expr.value.substr(separator + 2); expr.left = std::move(receiver);
      }
    }
    if (expr.left) {
      const Local* observable = expr.left->kind == parser::Expr::Kind::Name ? find_local_entry(locals, expr.left->value) : nullptr;
      if (observable && observable->observable && expr.value == "OnChange") {
        if (expr.args.size() != 1) {
          diagnostics.push_back(Diagnostic{expr.location, expr.span, "wrong number of arguments"}); return;
        }
        bind_expr(expr.args[0], locals, functions, structs, enums, diagnostics, "(" + observable->type_name + ")->void");
        expr.kind = parser::Expr::Kind::Observe; expr.slot = observable->slot; expr.global = observable->global;
        expr.value = observable->type_name; expr.left.reset(); return;
      }
      auto receiver_type = inferred_type(*expr.left, locals, structs);
      if (active_program) receiver_type = typechecker::resource_type(*active_program, receiver_type, active_function).value_or(receiver_type);
      const bool signal = receiver_type.starts_with("signal<");
      const bool connection = receiver_type == "connection";
      std::uint8_t operation = 255;
      if (signal && expr.value == "Connect") operation = 1;
      if (signal && expr.value == "Once") operation = 5;
      if (signal && expr.value == "Fire") operation = 2;
      if (signal && expr.value == "$ManagedConnect") operation = 9;
      if (signal && expr.value == "Clear") operation = 10;
      if (signal && expr.value == "Wait") operation = 11;
      if (connection && expr.value == "Disconnect") operation = 3;
      if (connection && expr.value == "Connected") operation = 4;
      if (operation != 255) {
        bind_expr(*expr.left, locals, functions, structs, enums, diagnostics);
        expr.args.insert(expr.args.begin(), std::move(*expr.left)); expr.left.reset();
        expr.kind = parser::Expr::Kind::Event; expr.runtime_op = operation;
        bind_arguments(expr, locals, functions, structs, enums, diagnostics); return;
      }
      if (signal || connection || expr.value == "$ManagedConnect") {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "unknown event or connection operation"}); return;
      }
    }
    if (!expr.named_initializer && expr.left == nullptr && find_local(locals, expr.value) >= 0) {
      auto callee = std::make_unique<parser::Expr>();
      callee->kind = parser::Expr::Kind::Name; callee->value = expr.value;
      callee->span = expr.span; callee->location = expr.location;
      expr.kind = parser::Expr::Kind::Invoke; expr.left = std::move(callee);
      bind_expr(expr, locals, functions, structs, enums, diagnostics); return;
    }
    if (expr.named_initializer && find_struct(structs, bare_type(expr.value)) == nullptr && active_program) {
      if (const auto data = typechecker::data_initializer_type(*active_program, expr.value, active_function))
        expr.value = *data;
    }
    if (expr.named_initializer && find_struct(structs, bare_type(expr.value)) == nullptr) {
      diagnostics.push_back(Diagnostic{expr.location, expr.span, "named initializer requires a data type"});
      return;
    }
    if (expr.left != nullptr) {
      const int object = struct_index_of(*expr.left, locals, structs, expr.value);
      if (object >= 0) {
        const auto& data = structs[object];
        const auto field = std::find(data.fields.begin(), data.fields.end(), expr.value);
        if (field != data.fields.end()) {
          auto member = std::make_unique<parser::Expr>(); member->kind = parser::Expr::Kind::Member;
          member->value = expr.value; member->span = expr.span; member->location = expr.location;
          member->left = std::move(expr.left); expr.left = std::move(member); expr.kind = parser::Expr::Kind::Invoke;
          bind_expr(expr, locals, functions, structs, enums, diagnostics); return;
        }
      }
      parser::Expr receiver = std::move(*expr.left);
      expr.left.reset();
      bind_method(expr, std::move(receiver), locals, functions, structs, enums, diagnostics);
      return;
    }
    const auto take_function = [&](const std::size_t argument, const std::uint16_t expected_arity) -> int {
      if (argument >= expr.args.size() || expr.args[argument].kind != parser::Expr::Kind::Name) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "undefined function"});
        return -1;
      }
      const int index = find_func(functions, expr.args[argument].value);
      if (index < 0) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "undefined function"});
        return -1;
      }
      for (const FuncRef& symbol : functions) {
        if (symbol.index == static_cast<std::uint16_t>(index) && symbol.is_abstract) {
          diagnostics.push_back(Diagnostic{expr.location, expr.span, "abstract method requires virtual dispatch"});
          return -1;
        }
        if (symbol.index == static_cast<std::uint16_t>(index) && symbol.is_private &&
            (active_owner == nullptr || *active_owner != symbol.name.substr(0, symbol.name.rfind('.')))) {
          diagnostics.push_back(Diagnostic{expr.location, expr.span, "private member"});
          return -1;
        }
        if (symbol.index == static_cast<std::uint16_t>(index) && symbol.arity != expected_arity) {
          diagnostics.push_back(Diagnostic{expr.location, expr.span, "wrong number of arguments"});
          return -1;
        }
      }
      return index;
    };
    const std::string on_change = ".OnChange";
    if (expr.value.size() > on_change.size() &&
        expr.value.compare(expr.value.size() - on_change.size(), on_change.size(), on_change) == 0) {
      const std::string owner = expr.value.substr(0, expr.value.size() - on_change.size());
      const Local* local = find_local_entry(locals, owner);
      if (local != nullptr && local->observable) {
        if (expr.args.size() != 1) {
          diagnostics.push_back(Diagnostic{expr.location, expr.span, "wrong number of arguments"}); return;
        }
        bind_expr(expr.args[0], locals, functions, structs, enums, diagnostics, "(" + local->type_name + ")->void");
        expr.kind = parser::Expr::Kind::Observe; expr.slot = local->slot; expr.global = local->global;
        expr.value = local->type_name;
        return;
      }
    }
    if (expr.value == "coroutine.create" || expr.value == "task.defer") {
      if (expr.args.size() != 1) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "wrong number of arguments"});
        return;
      }
      const int index = take_function(0, 0);
      if (index < 0) {
        return;
      }
      expr.runtime_op = expr.value == "coroutine.create" ? 1 : 4;
      expr.slot = static_cast<std::uint16_t>(index);
      expr.args.clear();
      return;
    }
    if (expr.value == "coroutine.resume" || expr.value == "coroutine.yield") {
      if (expr.args.size() != 1) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "wrong number of arguments"});
        return;
      }
      expr.runtime_op = expr.value == "coroutine.resume" ? 2 : 3;
      for (parser::Expr& arg : expr.args) {
        bind_expr(arg, locals, functions, structs, enums, diagnostics);
      }
      return;
    }
    if (expr.value == "actor") {
      if (expr.args.size() != 2) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "wrong number of arguments"});
        return;
      }
      const int index = take_function(0, 1);
      if (index < 0) {
        return;
      }
      expr.runtime_op = 5;
      for (const auto& function : functions) if (function.index == static_cast<std::uint16_t>(index) && function.is_async) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "actor requires a synchronous entry function; await async tasks inside it"});
        return;
      }
      expr.slot = static_cast<std::uint16_t>(index);
      expr.args.erase(expr.args.begin());
      bind_expr(expr.args[0], locals, functions, structs, enums, diagnostics);
      return;
    }
    if (bare_type(expr.value) == "array" || bare_type(expr.value) == "dictionary") {
      if (bare_type(expr.value) == "dictionary") {
        expr.slot = 65535;
      }
      expr.kind = parser::Expr::Kind::MakeList;
      bind_arguments(expr, locals, functions, structs, enums, diagnostics);
      return;
    }
    const std::size_t dot = expr.value.find('.');
    if (dot != std::string::npos) {
      const std::string owner = expr.value.substr(0, dot);
      const std::string method = expr.value.substr(dot + 1);
      const Local* local = find_local_entry(locals, owner);
      const StructRef* type = local == nullptr ? nullptr : find_struct(structs, bare_type(member_owner_spelling(local->type_name, structs, bare_type(method))));
      if (type != nullptr && method.find('.') == std::string::npos) {
        parser::Expr receiver;
        receiver.kind = parser::Expr::Kind::Name;
        receiver.value = owner;
        receiver.span = expr.span;
        receiver.location = expr.location;
        expr.value = method;
        expr.left = std::make_unique<parser::Expr>(std::move(receiver));
        bind_expr(expr, locals, functions, structs, enums, diagnostics);
        return;
      }
    }
    if (expr.value == "list") {
      expr.kind = parser::Expr::Kind::MakeList;
      for (parser::Expr& arg : expr.args) {
        bind_expr(arg, locals, functions, structs, enums, diagnostics);
      }
      return;
    }
    if (expr.value == "sort" && expr.args.size() == 1) {
      expr.kind = parser::Expr::Kind::Sort;
      bind_expr(expr.args[0], locals, functions, structs, enums, diagnostics);
      return;
    }
    if (expr.value == "push" || expr.value == "pop" || expr.value == "insert" || expr.value == "remove") {
      const int op = expr.value == "push" ? 0 : expr.value == "pop" ? 1 : expr.value == "insert" ? 2 : 3;
      const std::size_t wanted = op == 0 ? 2 : op == 1 ? 1 : op == 2 ? 3 : 2;
      if (expr.args.size() != wanted) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "wrong number of arguments"});
        return;
      }
      const parser::Expr* root = &expr.args[0];
      while ((root->kind == parser::Expr::Kind::Member || root->kind == parser::Expr::Kind::Index) && root->left) root = root->left.get();
      const Local* target = root->kind == parser::Expr::Kind::Name ? find_local_entry(locals, root->value) : nullptr;
      if (target != nullptr && target->constant) {
        diagnostics.push_back(Diagnostic{expr.args[0].location, expr.args[0].span,
                                         "cannot assign to immutable binding (declare it with auto or a type)"});
        return;
      }
      expr.kind = parser::Expr::Kind::ListMut;
      expr.slot = expr.args[0].kind == parser::Expr::Kind::Name && target != nullptr && !target->global ? target->slot : 65535;
      expr.number = op;
      for (parser::Expr& arg : expr.args) {
        bind_expr(arg, locals, functions, structs, enums, diagnostics);
      }
      return;
    }
    if (expr.value == "len" && expr.args.size() == 1) {
      expr.kind = parser::Expr::Kind::Len;
      bind_expr(expr.args[0], locals, functions, structs, enums, diagnostics);
      return;
    }
    if (expr.value == "len" && expr.args.size() != 1) {
      diagnostics.push_back(Diagnostic{expr.location, expr.span, "len takes one argument"});
      return;
    }
    if (expr.value == "find" && expr.args.size() == 2) {
      expr.kind = parser::Expr::Kind::Find;
      for (parser::Expr& arg : expr.args) {
        bind_expr(arg, locals, functions, structs, enums, diagnostics);
      }
      return;
    }
    if ((expr.value == "sort" || expr.value == "find") &&
        ((expr.value == "sort" && expr.args.size() != 1) || (expr.value == "find" && expr.args.size() != 2))) {
      diagnostics.push_back(Diagnostic{expr.location, expr.span, "wrong number of arguments"});
      return;
    }
    if (expr.value == "fetch_add" || expr.value == "atomic_load" || expr.value == "mutex" || expr.value == "lock" ||
        expr.value == "unlock") {
      const bool two = expr.value == "fetch_add";
      const bool one = expr.value == "atomic_load" || expr.value == "lock" || expr.value == "unlock";
      const bool none = expr.value == "mutex";
      if ((two && expr.args.size() != 2) || (one && expr.args.size() != 1) || (none && !expr.args.empty())) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "wrong number of arguments"});
      }
      expr.slot = expr.value == "fetch_add" ? 1 : expr.value == "atomic_load" ? 2 : expr.value == "mutex" ? 3
                 : expr.value == "lock"     ? 4
                                            : 5;
      expr.kind = parser::Expr::Kind::Atomic;
      for (parser::Expr& arg : expr.args) {
        bind_expr(arg, locals, functions, structs, enums, diagnostics);
      }
      return;
    }
    const StructRef* constructed = find_struct(structs, bare_type(expr.value));
    if (constructed != nullptr) {
      if (!expr.named_initializer) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span,
            "data values use Type { Field = value }; positional constructors are not supported"});
        return;
      }
      if (constructed->is_abstract) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "abstract type"});
      }
      // Named field initializers are checked and put in definition order.
      {
        std::vector<parser::Expr> ordered(constructed->fields.size());
        std::vector<char> filled(constructed->fields.size(), 0);
        std::vector<bool> optional_fields;
        bool checked_optional_fields = false;
        bool ok = true;
        for (std::size_t index = 0; index < expr.args.size() && ok; ++index) {
          const std::string name = index < expr.arg_names.size() ? expr.arg_names[index] : std::string{};
          const int field = find_field(*constructed, name);
          if (field < 0) {
            diagnostics.push_back(Diagnostic{expr.location, expr.span, "unknown field: " + name});
            ok = false;
            break;
          }
          const std::size_t slot = static_cast<std::size_t>(field);
          if (slot >= ordered.size() || filled[slot] != 0) {
            diagnostics.push_back(Diagnostic{expr.location, expr.span, "field given twice"});
            ok = false;
            break;
          }
          ordered[slot] = std::move(expr.args[index]);
          filled[slot] = 1;
        }
        for (std::size_t field = 0; field < filled.size() && ok; ++field) {
          if (filled[field] != 0) continue;
          if (field < constructed->field_defaults.size() && constructed->field_defaults[field] != nullptr) {
            const auto* declaration = find_struct(structs, constructed->field_owners[field]);
            const auto declared_field = declaration ? find_field(*declaration, constructed->fields[field]) : -1;
            const auto& field_expected = declared_field >= 0 ? declaration->field_types[declared_field] : constructed->field_types[field];
            if (!bind_default(*constructed->field_defaults[field], functions, structs, enums, diagnostics, field_expected, constructed->field_owners[field])) return;
            ordered[field] = clone_expr(*constructed->field_defaults[field]);
            if (active_program) {
              const auto owner = typechecker::default_owner_type(*active_program, expr.value, constructed->field_owners[field], active_function);
              if (!owner) { diagnostics.push_back(Diagnostic{expr.location, expr.span, "cannot specialize field default owner"}); return; }
              ordered[field].default_scope = *owner;
            }
          } else {
            if (!checked_optional_fields && active_program) {
              optional_fields = typechecker::implicit_null_fields(*active_program, expr.value, active_function);
              checked_optional_fields = true;
            }
            if (field < optional_fields.size() && optional_fields[field]) {
              ordered[field].kind = parser::Expr::Kind::Number;
              ordered[field].null_literal = true;
              ordered[field].location = expr.location;
              ordered[field].span = expr.span;
            } else {
              diagnostics.push_back(Diagnostic{expr.location, expr.span, "missing required field: " + constructed->fields[field]});
              ok = false;
            }
          }
        }
        if (ok) {
          expr.args = std::move(ordered);
          expr.arg_names = constructed->fields;
        }
      }
      if (expr.args.size() != constructed->fields.size()) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "wrong number of arguments"});
      }
      expr.kind = parser::Expr::Kind::Construct;
      expr.slot = constructed->index;
    } else {
      const int native = native_id(expr.value, expr.args.size());
      if (native >= 0) {
        expr.kind = parser::Expr::Kind::Native;
        expr.slot = static_cast<std::uint16_t>(native);
      } else {
        const FuncRef* target = nullptr;
        bool named = false;
        const std::string lookup = bare_type(expr.value);
        const FuncRef* loose = nullptr;
        for (const FuncRef& function : functions) {
          if (function.name != lookup && function.name != expr.value) {
            continue;
          }
          named = true;
          loose = &function;
          if (function.arity == expr.args.size()) {
            target = &function;
            break;
          }
        }
        if (target == nullptr) {
          target = loose;
        }
        if (target == nullptr) {
          diagnostics.push_back(
              Diagnostic{expr.location, expr.span, named ? "wrong number of arguments" : "undefined function"});
        } else if (target->is_abstract) {
          diagnostics.push_back(Diagnostic{expr.location, expr.span, "abstract method requires virtual dispatch"});
          return;
        } else if (target->is_private &&
                   (active_owner == nullptr || *active_owner != target->name.substr(0, target->name.rfind('.')))) {
          diagnostics.push_back(Diagnostic{expr.location, expr.span, "private member"});
          return;
        } else if (target->is_extern) {
          expr.extern_call = true;
          expr.slot = target->extern_id;
        } else {
          expr.slot = target->index;
          expr.async_call = target->is_async;
        }
      }
    }
    if (expr.kind == parser::Expr::Kind::Call && active_program != nullptr && !expr.extern_call && expr.runtime_op == 0) {
      const parser::Function* definition = nullptr;
      for (const parser::Function& function : active_program->functions) {
        if (function.index == expr.slot && function.name == bare_type(expr.value)) {
          definition = &function;
          break;
        }
      }
      if (definition == nullptr) {
        for (const parser::Function& function : active_program->functions) {
          if (function.name == bare_type(expr.value)) {
            definition = &function;
            break;
          }
        }
      }
      if (definition != nullptr && (definition->variadic || !expr.arg_names.empty() || expr.args.size() != definition->params.size())) {
        const std::size_t arity = definition->params.size();
        std::vector<parser::Expr> ordered(arity);
        std::vector<char> filled(arity, 0);
        std::size_t positional = 0;
        bool failed = false;
        for (std::size_t index = 0; index < expr.args.size(); ++index) {
          const std::string given = index < expr.arg_names.size() ? expr.arg_names[index] : std::string{};
          std::size_t slot = positional;
          if (!given.empty()) {
            slot = arity;
            for (std::size_t param = 0; param < definition->params.size(); ++param) {
              if (definition->params[param] == given) {
                slot = param;
                break;
              }
            }
          }
          if (definition->variadic && slot + 1 >= arity) {
            break;
          }
          if (slot >= arity || filled[slot] != 0) {
            failed = true;
            break;
          }
          ordered[slot] = std::move(expr.args[index]);
          filled[slot] = 1;
          if (given.empty()) {
            ++positional;
          }
        }
        if (definition->variadic && arity > 0) {
          parser::Expr list;
          list.kind = parser::Expr::Kind::MakeList;
          for (std::size_t index = positional; index < expr.args.size(); ++index) {
            if (index < expr.arg_names.size() && !expr.arg_names[index].empty()) {
              continue;
            }
            list.args.push_back(std::move(expr.args[index]));
          }
          ordered[arity - 1] = std::move(list);
          filled[arity - 1] = 1;
        }
        for (std::size_t slot = 0; slot < arity; ++slot) {
          if (filled[slot] != 0) {
            continue;
          }
          failed = true;
        }
        if (failed) {
          diagnostics.push_back(Diagnostic{expr.location, expr.span, "wrong number of arguments"});
        } else {
          expr.args = std::move(ordered);
          expr.slot = definition->index;
        }
      }
    }
    bind_arguments(expr, locals, functions, structs, enums, diagnostics);
    return;
  }
  if (expr.kind == parser::Expr::Kind::Member) {
    if (expr.missing) {
      if (expr.left != nullptr) {
        bind_expr(*expr.left, locals, functions, structs, enums, diagnostics);
      }
      return;
    }
    if (expr.left != nullptr && expr.left->kind == parser::Expr::Kind::Name) {
      const EnumRef* enumeration = find_enum(enums, expr.left->value);
      if (enumeration != nullptr) {
        if (enumeration->scoped && !expr.scoped_member) {
          diagnostics.push_back(Diagnostic{expr.location, expr.span, "enum case requires ::"});
          return;
        }
        const int variant = find_variant(*enumeration, expr.value);
        if (variant < 0) {
          diagnostics.push_back(Diagnostic{expr.location, expr.span, "unknown variant"});
        } else {
          const auto enum_index = static_cast<std::uint16_t>(enumeration - enums.data());
          expr.kind = parser::Expr::Kind::Number;
          expr.number = static_cast<double>(variant);
          expr.float_literal = false;
          expr.enum_literal = true;
          expr.slot = enum_index;
          expr.left.reset();
        }
        return;
      }
    }
    if (expr.scoped_member) {
      diagnostics.push_back(Diagnostic{expr.location, expr.span, "scoped member requires an enum type"});
      return;
    }
    if (expr.left != nullptr) {
      bind_expr(*expr.left, locals, functions, structs, enums, diagnostics);
    }
    if (expr.left && active_program && expr.value == "Connected" &&
        typechecker::resource_type(*active_program, inferred_type(*expr.left, locals, structs), active_function).value_or("") == "connection") {
      expr.args.push_back(std::move(*expr.left)); expr.left.reset(); expr.kind = parser::Expr::Kind::Event; expr.runtime_op = 4; return;
    }
    const int struct_index = expr.left == nullptr ? -1 : struct_index_of(*expr.left, locals, structs, bare_type(expr.value));
    if (struct_index >= 0) {
      const StructRef* decl = struct_at(structs, static_cast<std::uint16_t>(struct_index));
      const int field = decl == nullptr ? -1 : find_field(*decl, expr.value);
      if (field < 0) {
        if (decl != nullptr) {
          const auto method = std::find(decl->method_names.begin(), decl->method_names.end(), bare_type(expr.value));
          if (method != decl->method_names.end()) {
            const auto slot = static_cast<std::size_t>(method - decl->method_names.begin());
            const FuncRef* target = nullptr;
            if (slot < decl->vtable.size()) for (const auto& candidate : functions)
              if (candidate.index == decl->vtable[slot]) { target = &candidate; break; }
            if (target == nullptr) { diagnostics.push_back(Diagnostic{expr.location, expr.span, "undefined method"}); return; }
            const auto owner = target->name.substr(0, target->name.rfind('.'));
            if (target->is_private && (active_owner == nullptr || *active_owner != owner)) {
              diagnostics.push_back(Diagnostic{expr.location, expr.span, "private member"}); return;
            }
            const auto open = expr.value.find('<');
            const auto supplied = open == std::string::npos ? std::string{} : expr.value.substr(open);
            expr.kind = parser::Expr::Kind::Closure; expr.virtual_call = true;
            expr.slot = static_cast<std::uint16_t>(slot); expr.value = target->name + supplied;
            return;
          }
        }
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "unknown field"});
      } else {
        expr.slot = static_cast<std::uint16_t>(field);
        if (static_cast<std::size_t>(field) < decl->private_fields.size() && decl->private_fields[static_cast<std::size_t>(field)] != 0) {
          const std::string owner = static_cast<std::size_t>(field) < decl->field_owners.size()
                                        ? decl->field_owners[static_cast<std::size_t>(field)]
                                        : std::string{};
          if (active_owner == nullptr || *active_owner != owner) {
            diagnostics.push_back(Diagnostic{expr.location, expr.span, "private member"});
          }
        }
      }
      return;
    }
    if (expr.left != nullptr && expr.left->kind == parser::Expr::Kind::Index && expr.value != "x" && expr.value != "y" &&
        expr.value != "z" && expr.value != "w") {
      diagnostics.push_back(Diagnostic{expr.location, expr.span,
                                       "unknown field: the element type is not known; declare the list with it, e.g. array<Stats> team"});
      return;
    }
    if (expr.value == "x") {
      expr.slot = 0;
    } else if (expr.value == "y") {
      expr.slot = 1;
    } else if (expr.value == "z") {
      expr.slot = 2;
    } else if (expr.value == "w") {
      expr.slot = 3;
    } else {
      diagnostics.push_back(Diagnostic{expr.location, expr.span, "unknown field"});
    }
    return;
  }
  if (expr.left != nullptr) {
    bind_expr(*expr.left, locals, functions, structs, enums, diagnostics);
  }
  if (expr.right != nullptr) {
    bind_expr(*expr.right, locals, functions, structs, enums, diagnostics);
  }
  for (parser::Expr& arg : expr.args) {
    bind_expr(arg, locals, functions, structs, enums, diagnostics);
  }
}

[[nodiscard]] std::string literal_text(const parser::Expr& expr, const std::vector<Local>& locals) {
  if (expr.kind == parser::Expr::Kind::Number) {
    if (expr.enum_literal) {
      return expr.value;
    }
    if (expr.null_literal) {
      return "null";
    }
    if (expr.bool_literal) {
      return expr.number == 0 ? "false" : "true";
    }
    if (expr.float_literal) {
      std::ostringstream out;
      out << expr.number;
      return out.str();
    }
    if (expr.exact_integer) return std::to_string(expr.integer);
    const auto integer = numeric::from_float(expr.number);
    return integer ? std::to_string(*integer) : std::string{};
  }
  if (expr.kind == parser::Expr::Kind::String) {
    return "\"" + expr.value + "\"";
  }
  if (expr.kind == parser::Expr::Kind::Name) {
    const Local* local = expression_local(locals, expr);
    if (local != nullptr) {
      return local->value;
    }
  }
  if (expr.kind == parser::Expr::Kind::Construct) {
    std::string text = expr.value + " { ";
    for (std::size_t index = 0; index < expr.args.size(); ++index) {
      const std::string part = literal_text(expr.args[index], locals);
      if (part.empty()) {
        return {};
      }
      if (index != 0) {
        text += ", ";
      }
      if (index < expr.arg_names.size()) text += expr.arg_names[index] + " = ";
      text += part;
    }
    text += " }";
    return text;
  }
  return {};
}

// Preserve the concrete arguments of a receiver when looking up its fields.
[[nodiscard]] std::string specialized_type_name(const std::vector<std::string>& parameters, const std::string& receiver,
                                                 const std::string& field) {
  if (parameters.empty()) return field;
  const auto open = receiver.find('<');
  if (open == std::string::npos || receiver.back() != '>') return {};
  std::vector<std::string> arguments;
  std::size_t start = open + 1;
  int depth = 0;
  bool quoted = false;
  for (std::size_t i = start; i < receiver.size(); ++i) {
    if (receiver[i] == '"' && (i == 0 || receiver[i - 1] != '\\')) quoted = !quoted;
    if (quoted) continue;
    if ((receiver[i] == ',' && depth == 0) || i == receiver.size() - 1) {
      std::string argument = receiver.substr(start, i - start);
      const auto first = argument.find_first_not_of(' ');
      const auto last = argument.find_last_not_of(' ');
      arguments.push_back(first == std::string::npos ? std::string{} : argument.substr(first, last - first + 1));
      start = i + 1;
    } else if (receiver[i] == '<') ++depth;
    else if (receiver[i] == '>') --depth;
  }
  if (arguments.size() != parameters.size()) return {};
  module_types::Map mapping;
  for (std::size_t index = 0; index < parameters.size(); ++index) mapping.push_back({parameters[index], arguments[index]});
  std::string result = field;
  module_types::annotation(result, mapping);
  return result;
}

[[nodiscard]] std::string semantic_expression_type(const parser::Expr& expr, const std::vector<Local>& locals) {
  if (active_program == nullptr) return {};
  std::vector<typechecker::LocalType> types;
  for (const auto& local : locals) types.push_back({local.name, local.type_name, local.global});
  const auto result = typechecker::expression_type(*active_program, expr, types, active_function);
  return result.value_or(std::string{});
}

[[nodiscard]] std::string inferred_type(const parser::Expr& expr, const std::vector<Local>& locals,
                                        const std::vector<StructRef>& structs) {
  if (expr.kind == parser::Expr::Kind::TypeTest) return "bool";
  if (expr.kind == parser::Expr::Kind::Ascribe) return expr.value;
  if (expr.kind == parser::Expr::Kind::Construct && !expr.value.empty()) {
    return expr.value;
  }
  if (expr.kind == parser::Expr::Kind::Name) {
    const Local* local = expression_local(locals, expr);
    if (local != nullptr) {
      return local->type_name;
    }
  }
  if (expr.kind == parser::Expr::Kind::Await && expr.left != nullptr &&
      expr.left->kind == parser::Expr::Kind::Call && g_bound_functions != nullptr && expr.left->async_call) {
    for (const FuncRef& function : *g_bound_functions) {
      if (function.name == bare_type(expr.left->value) && function.index == expr.left->slot) {
        return function.type_params.empty() || (function.owner_type_param_count == 0 && expr.left->value.find('<') != std::string::npos)
            ? specialized_type_name(function.type_params, expr.left->value, function.return_type)
            : semantic_expression_type(expr, locals);
      }
    }
  }
  if (expr.kind == parser::Expr::Kind::Call && g_bound_functions != nullptr &&
      !expr.async_call && !expr.thread_call && !expr.extern_call) {
    for (const FuncRef& function : *g_bound_functions) {
      if (function.name == bare_type(expr.value) && (function.index == expr.slot || expr.virtual_call)) {
        return function.type_params.empty() || (function.owner_type_param_count == 0 && expr.value.find('<') != std::string::npos)
            ? specialized_type_name(function.type_params, expr.value, function.return_type)
            : semantic_expression_type(expr, locals);
      }
    }
  }
  if (expr.kind == parser::Expr::Kind::Member && expr.left != nullptr) {
    const int owner_index = struct_index_of(*expr.left, locals, structs, expr.value);
    const StructRef* owner = owner_index < 0 ? nullptr : struct_at(structs, static_cast<std::uint16_t>(owner_index));
    if (owner != nullptr) {
      const int field = find_field(*owner, expr.value);
      if (field >= 0 && static_cast<std::size_t>(field) < owner->field_types.size()) {
        return specialized_type_name(owner->type_params, member_owner_spelling(inferred_type(*expr.left, locals, structs), structs, expr.value),
                                      owner->field_types[static_cast<std::size_t>(field)]);
      }
    }
  }
  if (expr.kind == parser::Expr::Kind::Index && expr.left != nullptr) {
    const std::string source = inferred_type(*expr.left, locals, structs);
    if (source == "string") return "string";
    if (expr.right && expr.right->kind == parser::Expr::Kind::Range) return semantic_expression_type(expr, locals);
    const std::size_t open = source.find('<');
    if (open != std::string::npos && source.back() == '>') {
      const std::string container = source.substr(0, open);
      const std::string args = source.substr(open + 1, source.size() - open - 2);
      if (container == "array" || container == "list") return args;
      if (container == "dictionary") {
        std::size_t depth = 0;
        for (std::size_t index = 0; index < args.size(); ++index) {
          if (args[index] == '<') ++depth;
          else if (args[index] == '>' && depth != 0) --depth;
          else if (args[index] == ',' && depth == 0) {
            const std::size_t value = args.find_first_not_of(' ', index + 1);
            return value == std::string::npos ? std::string{} : args.substr(value);
          }
        }
      }
    }
  }
  if (expr.kind == parser::Expr::Kind::String) return "string";
  if (expr.kind == parser::Expr::Kind::Number) {
    if (expr.enum_literal) {
      return semantic_expression_type(expr, locals);
    }
    if (expr.bool_literal) {
      return "bool";
    }
    if (!expr.null_literal) {
      return expr.float_literal ? "float" : "number";
    }
  }
  return semantic_expression_type(expr, locals);
}

void remember_binding(parser::Stmt& stmt, std::vector<Local>& locals, const std::vector<StructRef>& structs) {
  if (stmt.declared_type.empty()) {
    stmt.declared_type = inferred_type(stmt.expr, locals, structs);
  }
  stmt.remembered = literal_text(stmt.expr, locals);
}

void bind_stmts(std::vector<parser::Stmt>& statements, std::vector<Local>& locals,
                const std::vector<FuncRef>& functions, const std::vector<StructRef>& structs,
                const std::vector<EnumRef>& enums, std::size_t& next_slot, std::vector<Diagnostic>& diagnostics) {
  const std::size_t watermark = locals.size();
  // Declarations introduced by this block, excluding outer bindings that may
  // be shadowed. Avoid rescanning all preceding locals for every declaration.
  std::unordered_set<std::string> declared_names;
  for (parser::Stmt& stmt : statements) {
    if (stmt.kind == parser::Stmt::Kind::VariableDecl) {
      if (stmt.has_decltype) {
        bind_expr(stmt.type_expr, locals, functions, structs, enums, diagnostics);
      }
      bind_expr(stmt.expr, locals, functions, structs, enums, diagnostics, stmt.declared_type);
      if (stmt.module_global) {
        if (stmt.expr.missing) {
          diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "module bindings require explicit initializers"}); continue;
        }
        remember_binding(stmt, locals, structs);
        if (module_globals && stmt.slot < module_globals->size() && (*module_globals)[stmt.slot].name == stmt.name) {
          auto& global = (*module_globals)[stmt.slot];
          global.type_name = stmt.declared_type; global.value = stmt.remembered;
          active_program->globals[global.slot].type = stmt.declared_type;
          // Every binding scope starts with the module globals in slot order.
          if (stmt.slot < locals.size() && locals[stmt.slot].global && locals[stmt.slot].name == stmt.name) {
            locals[stmt.slot].type_name = stmt.declared_type;
            locals[stmt.slot].value = stmt.remembered;
          }
        }
        continue;
      }
      if (declared_names.contains(stmt.name)) {
        diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "already declared"});
        continue;
      }
      if (next_slot >= 65535) {
        diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "too many locals"});
        continue;
      }
      stmt.slot = static_cast<std::uint16_t>(next_slot);
      ++next_slot;
      remember_binding(stmt, locals, structs);
      locals.push_back(Local{stmt.name, stmt.slot, stmt.declared_type, stmt.immutable, stmt.remembered});
      declared_names.insert(stmt.name);
      if (stmt.observable_cell) {
        auto& boxed = active_function ? const_cast<parser::Function*>(active_function)->boxed_slots : active_program->boxed_slots;
        if (std::find(boxed.begin(), boxed.end(), stmt.slot) == boxed.end()) boxed.push_back(stmt.slot);
        locals.back().observable = true;
      }
      continue;
    }

    if (stmt.kind == parser::Stmt::Kind::SelfAssign && self_has_field(locals, structs, stmt.name)) {
      stmt.place = self_member(stmt.name, stmt.name_span, stmt.name_location);
      stmt.kind = parser::Stmt::Kind::PlaceAssign;
      stmt.name = "@this";
    }
    if (stmt.kind == parser::Stmt::Kind::PlaceAssign) {
      const Local* local = find_local_entry(locals, stmt.name);
      if (local == nullptr) {
        diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "undefined name"});
      } else if (local->constant) {
        diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span,
                                         "cannot assign to immutable binding (declare it with auto or a type)"});
      } else {
        stmt.slot = local->slot;
        stmt.module_global = local->global;
      }
      bind_expr(stmt.place, locals, functions, structs, enums, diagnostics);
      bind_expr(stmt.expr, locals, functions, structs, enums, diagnostics, inferred_type(stmt.place, locals, structs));
      continue;
    }

    if (stmt.kind == parser::Stmt::Kind::Assign) {
      const Local* local = find_local_entry(locals, stmt.name);
      if (local == nullptr) {
        diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "undefined name"});
      } else if (local->constant) {
        diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "cannot assign to immutable binding"});
      } else {
        stmt.slot = local->slot;
        stmt.module_global = local->global;
        if (local->observable && active_listeners != nullptr && local->slot < active_listeners->size()) {
          stmt.listeners = (*active_listeners)[local->slot];
        }
      }
      bind_expr(stmt.expr, locals, functions, structs, enums, diagnostics, local ? local->type_name : std::string{});
      stmt.remembered = literal_text(stmt.expr, locals);
      for (auto entry = locals.rbegin(); entry != locals.rend(); ++entry) {
        if (entry->name != stmt.name) continue;
        if (!entry->optional_origin.empty()) {
          entry->type_name = entry->optional_origin;
          entry->optional_origin.clear();
        }
        break;
      }
        if (!stmt.remembered.empty()) {
        for (std::size_t index = locals.size(); index > 0; --index) {
          if (locals[index - 1].name == stmt.name) {
            locals[index - 1].value = stmt.remembered;
            break;
          }
        }
      }
      continue;
    }

    if (stmt.kind == parser::Stmt::Kind::For) {
      const std::size_t mark = locals.size();
      for (parser::Stmt& init : stmt.init) {
        if (init.kind == parser::Stmt::Kind::VariableDecl) {
          bind_expr(init.expr, locals, functions, structs, enums, diagnostics);
          if (declared_since(locals, mark, init.name)) {
            diagnostics.push_back(Diagnostic{init.name_location, init.name_span, "already declared"});
          } else if (next_slot >= 65535) {
            diagnostics.push_back(Diagnostic{init.name_location, init.name_span, "too many locals"});
          } else {
            init.slot = static_cast<std::uint16_t>(next_slot);
            ++next_slot;
            locals.push_back(Local{init.name, init.slot, init.declared_type, init.immutable});
          }
        } else {
          const Local* local = find_local_entry(locals, init.name);
          if (local == nullptr) {
            diagnostics.push_back(Diagnostic{init.name_location, init.name_span, "undefined name"});
          } else if (local->constant) {
            diagnostics.push_back(Diagnostic{init.name_location, init.name_span, "cannot assign to immutable binding"});
          } else {
            init.slot = local->slot;
            init.module_global = local->global;
          }
          bind_expr(init.expr, locals, functions, structs, enums, diagnostics);
        }
      }
      bind_expr(stmt.expr, locals, functions, structs, enums, diagnostics);
      bind_stmts(stmt.then_body, locals, functions, structs, enums, next_slot, diagnostics);
      bind_stmts(stmt.step, locals, functions, structs, enums, next_slot, diagnostics);
      locals.resize(mark);
      continue;
    }

    if (stmt.kind == parser::Stmt::Kind::ForIn) {
      const std::size_t mark = locals.size();
      bind_expr(stmt.expr, locals, functions, structs, enums, diagnostics);
      if (declared_since(locals, mark, stmt.name)) {
        diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "already declared"});
      } else if (next_slot >= 65534) {
        diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "too many locals"});
      } else {
        stmt.slot = static_cast<std::uint16_t>(next_slot);
        ++next_slot;
        stmt.limit_slot = static_cast<std::uint16_t>(next_slot);
        ++next_slot;
        stmt.items_slot = static_cast<std::uint16_t>(next_slot);
        ++next_slot;
        stmt.index_slot = static_cast<std::uint16_t>(next_slot);
        ++next_slot;
        std::string item_type;
        if (active_program != nullptr) {
          std::vector<typechecker::LocalType> types;
          for (const auto& local : locals) types.push_back({local.name, local.type_name, local.global});
          item_type = typechecker::iteration_type(*active_program, stmt.expr, types, active_function).value_or(std::string{});
        }
        if (!stmt.declared_type.empty()) item_type = stmt.declared_type;
        else stmt.declared_type = item_type;
        locals.push_back(Local{stmt.name, stmt.slot, item_type, false});
      }
      bind_stmts(stmt.then_body, locals, functions, structs, enums, next_slot, diagnostics);
      locals.resize(mark);
      continue;
    }

    if (stmt.kind == parser::Stmt::Kind::Signal || stmt.kind == parser::Stmt::Kind::Connect ||
        stmt.kind == parser::Stmt::Kind::Using) {
      continue;
    }

    if (stmt.kind == parser::Stmt::Kind::Break) {
      continue;
    }

    if (stmt.kind == parser::Stmt::Kind::Try) {
      bind_stmts(stmt.then_body, locals, functions, structs, enums, next_slot, diagnostics);
      if (!stmt.name.empty()) {
        if (next_slot < 65535) {
          stmt.slot = static_cast<std::uint16_t>(next_slot);
          ++next_slot;
          locals.push_back(Local{stmt.name, stmt.slot, "string"});
        }
      }
      bind_stmts(stmt.else_body, locals, functions, structs, enums, next_slot, diagnostics);
      if (!stmt.name.empty() && !locals.empty()) {
        locals.pop_back();
      }
      continue;
    }
    if (stmt.kind == parser::Stmt::Kind::If || stmt.kind == parser::Stmt::Kind::While) {
      bind_expr(stmt.expr, locals, functions, structs, enums, diagnostics);
      auto then_locals = locals;
      auto else_locals = locals;
      if (!stmt.post_condition) narrow_optional(stmt.expr, true, then_locals);
      narrow_optional(stmt.expr, false, else_locals);
      bind_stmts(stmt.then_body, then_locals, functions, structs, enums, next_slot, diagnostics);
      if (stmt.has_else) bind_stmts(stmt.else_body, else_locals, functions, structs, enums, next_slot, diagnostics);
      if (stmt.is_guard) {
        narrow_optional(stmt.expr, false, locals);
      } else {
        for (std::size_t index = 0; index < locals.size(); ++index) {
          if (!locals[index].optional_origin.empty() &&
              (then_locals[index].type_name != locals[index].type_name || else_locals[index].type_name != locals[index].type_name)) {
            locals[index].type_name = locals[index].optional_origin;
            locals[index].optional_origin.clear();
          }
        }
      }
      continue;
    }

    if (stmt.kind == parser::Stmt::Kind::Match) {
      bind_expr(stmt.expr, locals, functions, structs, enums, diagnostics);
      for (parser::Stmt::MatchArm& arm : stmt.arms) {
        if (!arm.wildcard) {
          bind_expr(arm.pattern, locals, functions, structs, enums, diagnostics);
        }
        if (arm.has_guard) {
          bind_expr(arm.guard, locals, functions, structs, enums, diagnostics);
        }
        if (arm.has_payload) {
          if (next_slot >= 65535) {
            diagnostics.push_back(Diagnostic{arm.pattern.location, arm.pattern.span, "too many locals"});
          } else {
            arm.bind_slot = static_cast<std::uint16_t>(next_slot);
            ++next_slot;
            locals.push_back(Local{arm.bind_name, arm.bind_slot, {}});
          }
        }
        bind_stmts(arm.body, locals, functions, structs, enums, next_slot, diagnostics);
        if (arm.has_payload && !locals.empty() && locals.back().name == arm.bind_name) {
          locals.pop_back();
        }
      }
      continue;
    }

    bind_expr(stmt.expr, locals, functions, structs, enums, diagnostics,
              stmt.kind == parser::Stmt::Kind::Return && active_function ? active_function->return_type : std::string{});
  }
  locals.resize(watermark);
}

}  // namespace


void bind(parser::Program& program, std::vector<Diagnostic>& diagnostics) {
  active_program = &program;
  std::vector<Local> globals;
  std::unordered_set<std::string> global_names;
  program.globals.clear();
  const auto collect_globals = [&](std::vector<parser::Stmt>& statements) {
    for (auto& stmt : statements) if (stmt.kind == parser::Stmt::Kind::VariableDecl && stmt.module_global) {
      if (global_names.contains(stmt.name)) {
        diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "already declared"}); continue;
      }
      if (globals.size() >= 65535) { diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "too many module bindings"}); continue; }
      stmt.slot = static_cast<std::uint16_t>(globals.size());
      globals.push_back(Local{stmt.name, stmt.slot, stmt.declared_type, stmt.immutable});
      global_names.insert(stmt.name);
      globals.back().global = true; globals.back().observable = stmt.observable_cell;
      program.globals.push_back({stmt.name, stmt.declared_type, stmt.immutable, stmt.observable_cell});
    }
  };
  for (auto& initialize : program.functions) if (initialize.module_init) collect_globals(initialize.body);
  collect_globals(program.statements);
  module_globals = &globals;
  struct RestoreGlobals { ~RestoreGlobals() { module_globals = nullptr; } } restore_globals;
  // Imported definitions can follow their users in the flattened program.
  // Establish base-before-child order before assigning any runtime type slots.
  std::vector<std::vector<std::size_t>> dependencies(program.structs.size());
  for (std::size_t index = 0; index < program.structs.size(); ++index) {
    const auto& data = program.structs[index];
    const auto bases = data.bases.empty() && !data.base.empty() ? std::vector<std::string>{data.base} : data.bases;
    for (const auto& spelling : bases) {
      const auto resolved = typechecker::data_base_type(program, spelling, data);
      if (!resolved) continue;  // The normal binding pass reports invalid base annotations.
      const auto name = bare_type(*resolved);
      for (std::size_t base = 0; base < program.structs.size(); ++base) {
        if (program.structs[base].name == name) { dependencies[index].push_back(base); break; }
      }
    }
  }
  std::vector<std::size_t> order;
  std::vector<bool> emitted(program.structs.size());
  while (order.size() < program.structs.size()) {
    bool progressed = false;
    for (std::size_t index = 0; index < program.structs.size(); ++index) {
      if (emitted[index] || !std::all_of(dependencies[index].begin(), dependencies[index].end(),
                                       [&](const auto base) { return emitted[base]; })) continue;
      emitted[index] = true; order.push_back(index); progressed = true;
    }
    if (!progressed) {
      const auto index = static_cast<std::size_t>(std::find(emitted.begin(), emitted.end(), false) - emitted.begin());
      const auto& data = program.structs[index];
      diagnostics.push_back(Diagnostic{data.base_location, data.base_span, "cyclic inheritance"});
      return;
    }
  }
  std::vector<parser::StructDecl> ordered;
  ordered.reserve(order.size());
  for (const auto index : order) ordered.push_back(std::move(program.structs[index]));
  program.structs = std::move(ordered);
  std::vector<StructRef> structs;
  for (std::size_t index = 0; index < program.structs.size(); ++index) {
    parser::StructDecl& decl = program.structs[index];
    decl.index = static_cast<std::uint16_t>(index);
    StructRef ref;
    ref.name = decl.name;
    ref.type_params = decl.type_params;
    ref.index = decl.index;
    for (const parser::Field& field : decl.fields) {
      ref.fields.push_back(field.name);
      ref.field_types.push_back(field.type_name);
    }
    structs.push_back(std::move(ref));
    const std::vector<std::string> base_names = decl.bases.empty() && !decl.base.empty()
                                                    ? std::vector<std::string>{decl.base}
                                                    : decl.bases;
    const std::vector<parser::Field> own = decl.fields;
    std::vector<parser::Field> merged;
    std::size_t storage_bases = 0;
    bool interface_bases = true;
    for (std::size_t base_at = 0; base_at < base_names.size(); ++base_at) {
      const auto specialization = typechecker::data_base_type(program, base_names[base_at], decl);
      const StructRef* base = specialization ? find_struct(structs, bare_type(*specialization)) : nullptr;
      const bool known = base != nullptr && base->index < decl.index;
      if (!known) {
        const SourceLocation where = base_at < decl.base_locations.size() ? decl.base_locations[base_at] : decl.base_location;
        const std::string_view span = base_at < decl.base_spans.size() ? decl.base_spans[base_at] : decl.base_span;
        diagnostics.push_back(Diagnostic{where, span, "unknown type"});
        continue;
      }
      decl.base_indices.push_back(base->index);
      interface_bases = interface_bases && program.structs[base->index].is_interface;
      if (!program.structs[base->index].is_interface && ++storage_bases > 1) {
        diagnostics.push_back(Diagnostic{decl.name_location, decl.name_span, "inheritance permits only one storage base"});
      }
      if (decl.base_index == 65535) {
        decl.base_index = base->index;
      }
      for (parser::Field inherited : program.structs[base->index].fields) {
        inherited.type_name = specialized_type_name(base->type_params, *specialization, inherited.type_name);
        const auto same = std::find_if(merged.begin(), merged.end(), [&](const parser::Field& field) {
          return field.name == inherited.name;
        });
        if (same == merged.end()) {
          merged.push_back(inherited);
        } else if (same->owner != inherited.owner) {
          diagnostics.push_back(Diagnostic{decl.name_location, decl.name_span, "ambiguous inheritance"});
        }
      }
    }
    for (const parser::Field& field : own) {
      const auto same = std::find_if(merged.begin(), merged.end(), [&](const parser::Field& inherited) {
        return inherited.name == field.name;
      });
      if (same != merged.end()) {
        diagnostics.push_back(Diagnostic{field.name_location, field.name_span, "already declared"});
      }
      merged.push_back(field);
    }
    if (!base_names.empty()) {
      decl.fields = std::move(merged);
    }
    const std::string method_prefix = decl.name + ".";
    decl.is_interface = decl.is_abstract && decl.fields.empty() && interface_bases &&
        std::all_of(program.functions.begin(), program.functions.end(), [&](const parser::Function& method) {
          return method.name.compare(0, method_prefix.size(), method_prefix) != 0 || method.is_abstract;
        });
    structs[index].fields.clear();
    structs[index].field_owners.clear();
    structs[index].field_types.clear();
    structs[index].private_fields.clear();
    structs[index].is_abstract = decl.is_abstract;
    for (const parser::Field& field : decl.fields) {
      structs[index].fields.push_back(field.name);
      structs[index].field_owners.push_back(field.owner);
      structs[index].field_types.push_back(field.type_name);
      structs[index].field_defaults.push_back(field.default_value);
      structs[index].private_fields.push_back(field.is_private ? 1 : 0);
    }
  }

  std::vector<EnumRef> enums;
  for (const parser::EnumDecl& decl : program.enums) {
    EnumRef ref;
    ref.scoped = decl.scoped;
    ref.name = decl.name;
    ref.variants = decl.variants;
    enums.push_back(std::move(ref));
  }

  std::vector<FuncRef> functions;
  g_bound_functions = &functions;
  struct ClearBound {
    ~ClearBound() { g_bound_functions = nullptr; }
  } clear_bound;
  for (std::size_t index = 0; index < program.functions.size(); ++index) {
    parser::Function& function = program.functions[index];
    bool same_arity = false;
    for (const FuncRef& existing : functions) {
      if (existing.name == function.name && existing.arity == function.params.size()) {
        same_arity = true;
        break;
      }
    }
    if (same_arity || find_struct(structs, function.name) != nullptr ||
        find_enum(enums, function.name) != nullptr) {
      diagnostics.push_back(Diagnostic{function.name_location, function.name_span, "already declared"});
      function.index = 65535;
      continue;
    }
    function.index = static_cast<std::uint16_t>(functions.size());
    FuncRef ref{function.name, static_cast<std::uint16_t>(function.params.size()), function.index, function.is_async};
    if (function.is_abstract && (function.is_private || function.is_final)) {
      diagnostics.push_back(Diagnostic{function.name_location, function.name_span, "abstract methods must be public and overridable"});
    }
    ref.is_extern = function.is_extern;
    ref.extern_id = function.extern_id;
    ref.is_private = function.is_private;
    ref.is_abstract = function.is_abstract;
    ref.is_override = function.is_override;
    ref.is_final = function.is_final;
    ref.return_type = function.return_type;
    ref.type_params = function.type_params;
    ref.owner_type_param_count = function.owner_type_param_count;
    functions.push_back(std::move(ref));
  }
  record_lexical_stmts(program.statements, program, functions, 65535);
  for (const auto& function : program.functions) if (!function.anonymous) {
    record_lexical_stmts(function.body, program, functions, function.index);
  }
  for (const auto& data : program.structs) for (const auto& field : data.fields)
    if (field.default_value) record_lexical_expr(*field.default_value, program, functions, 65535);

  for (std::size_t index = 0; index < program.structs.size(); ++index) {
    parser::StructDecl& decl = program.structs[index];
    for (const std::uint16_t base : decl.base_indices) {
      if (base >= program.structs.size()) {
        continue;
      }
      const parser::StructDecl& parent = program.structs[base];
      for (std::size_t method = 0; method < parent.method_names.size() && method < parent.vtable.size(); ++method) {
        const std::string& name = parent.method_names[method];
        const std::uint16_t impl = parent.vtable[method];
        int found = -1;
        for (std::size_t slot = 0; slot < decl.method_names.size(); ++slot) {
          if (decl.method_names[slot] == name) {
            found = static_cast<int>(slot);
            break;
          }
        }
        if (found < 0) {
          decl.method_names.push_back(name);
          decl.vtable.push_back(impl);
        } else if (decl.vtable[static_cast<std::size_t>(found)] != impl) {
          const auto abstract_method = [&](const std::uint16_t function_index) {
            return std::any_of(functions.begin(), functions.end(), [&](const FuncRef& function) {
              return function.index == function_index && function.is_abstract;
            });
          };
          auto& previous = decl.vtable[static_cast<std::size_t>(found)];
          if (abstract_method(previous)) previous = impl;
          else if (!abstract_method(impl))
            diagnostics.push_back(Diagnostic{decl.name_location, decl.name_span, "ambiguous inheritance"});
        }
      }
      if (program.structs[base].is_final) {
        diagnostics.push_back(Diagnostic{decl.name_location, decl.name_span, "final type"});
      }
    }
    const std::string prefix = decl.name + ".";
    for (const FuncRef& symbol : functions) {
      if (symbol.name.compare(0, prefix.size(), prefix) != 0) {
        continue;
      }
      const std::string method = symbol.name.substr(prefix.size());
      if (method.empty() || method.find('.') != std::string::npos) {
        continue;
      }
      int found = -1;
      for (std::size_t slot = 0; slot < decl.method_names.size(); ++slot) {
        if (decl.method_names[slot] == method) {
          found = static_cast<int>(slot);
          break;
        }
      }
      if (symbol.is_override && found < 0) {
        for (const parser::Function& function : program.functions) {
          if (function.name == symbol.name) {
            diagnostics.push_back(Diagnostic{function.name_location, function.name_span, "nothing to override"});
            break;
          }
        }
      }
      if (found >= 0) {
        const FuncRef* previous = nullptr;
        for (const FuncRef& candidate : functions) {
          if (candidate.index == decl.vtable[static_cast<std::size_t>(found)]) {
            previous = &candidate;
            break;
          }
        }
        if (previous != nullptr && previous->is_final) {
          diagnostics.push_back(Diagnostic{symbol.name.empty() ? decl.name_location : decl.name_location, decl.name_span, "final method"});
        }
        if (previous != nullptr && !previous->is_private && symbol.is_private) {
          diagnostics.push_back(Diagnostic{decl.name_location, decl.name_span, "override cannot reduce method visibility"});
        }
        if (previous != nullptr && previous->arity != symbol.arity) {
          for (const parser::Function& function : program.functions) {
            if (function.name == symbol.name) {
              diagnostics.push_back(Diagnostic{function.name_location, function.name_span, "wrong number of arguments"});
              break;
            }
          }
        } else {
          decl.vtable[static_cast<std::size_t>(found)] = symbol.index;
        }
      } else {
        decl.method_names.push_back(method);
        decl.vtable.push_back(symbol.index);
      }
    }
    if (!decl.is_abstract) {
      for (const std::uint16_t impl : decl.vtable) {
        for (const parser::Function& function : program.functions) {
          if (function.index == impl && function.is_abstract) {
            diagnostics.push_back(Diagnostic{decl.name_location, decl.name_span, "abstract method"});
            break;
          }
        }
      }
    }
    structs[index].method_names = decl.method_names;
    structs[index].vtable = decl.vtable;
  }

  // Every method name has a stable slot across nominal interface views. Missing
  // methods keep empty names and invalid targets; they are never exposed by lookup.
  std::vector<std::string> method_slots;
  for (const auto& data : program.structs) for (const auto& name : data.method_names)
    if (std::find(method_slots.begin(), method_slots.end(), name) == method_slots.end()) method_slots.push_back(name);
  for (std::size_t index = 0; index < program.structs.size(); ++index) {
    auto& data = program.structs[index];
    std::vector<std::string> names(method_slots.size());
    std::vector<std::uint16_t> targets(method_slots.size(), 65535);
    for (std::size_t method = 0; method < data.method_names.size(); ++method) {
      const auto slot = static_cast<std::size_t>(std::find(method_slots.begin(), method_slots.end(), data.method_names[method]) - method_slots.begin());
      names[slot] = data.method_names[method]; targets[slot] = data.vtable[method];
    }
    data.method_names = std::move(names); data.vtable = std::move(targets);
    structs[index].method_names = data.method_names; structs[index].vtable = data.vtable;
  }



  std::vector<std::vector<std::uint16_t>> listeners;
  active_listeners = &listeners;
  pending_defaults.clear();
  bound_defaults.clear();
  std::vector<parser::Function*> bind_order;
  for (auto& function : program.functions) if (function.module_init) bind_order.push_back(&function);
  for (auto& function : program.functions) if (!function.module_init) bind_order.push_back(&function);
  const auto bind_entry = [&] {
    active_function = nullptr;
    std::vector<Local> locals = globals;
    std::size_t next_slot = 0;
    bind_stmts(program.statements, locals, functions, structs, enums, next_slot, diagnostics);
    program.local_count = static_cast<std::uint16_t>(next_slot);
  };
  bool entry_bound = false;
  for (auto* pointer : bind_order) {
    if (!pointer->module_init && !entry_bound) { bind_entry(); entry_bound = true; }
    auto& function = *pointer;
    if (function.anonymous) continue;
    active_function = &function;
    std::vector<Local> locals = globals;
    std::size_t next_slot = 0;
    for (std::size_t param = 0; param < function.params.size(); ++param) {
      if (find_local_entry(locals, function.params[param]) && !find_local_entry(locals, function.params[param])->global) {
        diagnostics.push_back(
            Diagnostic{function.param_locations[param], function.param_spans[param], "already declared"});
        continue;
      }
      const auto slot = static_cast<std::uint16_t>(next_slot);
      ++next_slot;
      const std::string type_name = param < function.param_types.size() ? function.param_types[param] : std::string{};
      locals.push_back(Local{function.params[param], slot, type_name});
    }
    std::string owner;
    const std::size_t dot = function.name.rfind('.');
    if (dot != std::string::npos) {
      owner = function.name.substr(0, dot);
      active_owner = &owner;
    }
    bind_stmts(function.body, locals, functions, structs, enums, next_slot, diagnostics);
    active_owner = nullptr;
    function.local_count = static_cast<std::uint16_t>(next_slot);
  }

  if (!entry_bound) bind_entry();
  active_function = nullptr;
  for (parser::StructDecl& decl : program.structs) {
    for (parser::Field& field : decl.fields) {
      if (field.default_value != nullptr) {
        const auto* declaration = find_struct(structs, field.owner);
        const auto declared_field = declaration ? find_field(*declaration, field.name) : -1;
        const auto& field_expected = declared_field >= 0 ? declaration->field_types[declared_field] : field.type_name;
        (void)bind_default(*field.default_value, functions, structs, enums, diagnostics, field_expected, field.owner);
      }
    }
  }
  active_listeners = nullptr;
  active_program = nullptr;
  pending_defaults.clear();
  bound_defaults.clear();
}

}  // namespace clpp::binder
