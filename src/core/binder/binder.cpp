#include "core/binder/binder.hpp"

#include "clpp/stdlib.hpp"

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace clpp::binder {

namespace {

struct Local {
  std::string name;
  std::uint16_t slot{0};
  std::string type_name;
  bool constant{false};
  std::string value;
  bool observable{false};
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
  std::string return_type;
};

// The functions of the program being bound, so `make().field` can find make's return type.
thread_local const std::vector<FuncRef>* g_bound_functions = nullptr;

struct StructRef {
  std::string name;
  std::vector<std::string> fields;
  std::vector<std::string> field_owners;
  std::vector<std::string> field_types;
  std::vector<char> private_fields;
  std::vector<std::string> method_names;
  std::vector<std::uint16_t> vtable;
  std::uint16_t index{0};
  bool is_abstract{false};
};

struct EnumRef {
  std::string name;
  std::vector<std::string> variants;
  std::vector<std::string> payload_types;
};

struct VariantRef {
  std::string name;
  std::uint16_t index{0};
};

std::vector<VariantRef>* active_variants = nullptr;
std::vector<std::vector<std::uint16_t>>* active_listeners = nullptr;
const std::string* active_owner = nullptr;
parser::Program* active_program = nullptr;

[[nodiscard]] parser::Expr clone_expr(const parser::Expr& expr) {
  parser::Expr copy;
  copy.kind = expr.kind;
  copy.value = expr.value;
  copy.number = expr.number;
  copy.float_literal = expr.float_literal;
  copy.bool_literal = expr.bool_literal;
  copy.null_literal = expr.null_literal;
  copy.slot = expr.slot;
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

[[nodiscard]] const VariantRef* find_variant_type(const std::string_view name) {
  if (active_variants == nullptr) {
    return nullptr;
  }
  for (const VariantRef& decl : *active_variants) {
    if (decl.name == name) {
      return &decl;
    }
  }
  return nullptr;
}

[[nodiscard]] bool is_builtin_type(const std::string_view name) {
  return name.empty() || name == "int" || name == "float" || name == "double" || name == "string" || name == "bool" ||
         name == "void" || name == "Vector2" || name == "Vector3" || name == "Vector4" || name == "buffer" ||
         name == "task";
}

struct SignalBind {
  std::string name;
  std::vector<std::uint16_t> targets;
};

std::vector<SignalBind>* active_signals = nullptr;

[[nodiscard]] SignalBind* find_signal(const std::string_view name) {
  if (active_signals == nullptr) {
    return nullptr;
  }
  for (SignalBind& signal : *active_signals) {
    if (signal.name == name) {
      return &signal;
    }
  }
  return nullptr;
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

void bind_expr(parser::Expr& expr, const std::vector<Local>& locals, const std::vector<FuncRef>& functions,
               const std::vector<StructRef>& structs, const std::vector<EnumRef>& enums,
               std::vector<Diagnostic>& diagnostics);

[[nodiscard]] int struct_index_of(const parser::Expr& expr, const std::vector<Local>& locals,
                                  const std::vector<StructRef>& structs) {
  if (expr.kind == parser::Expr::Kind::Construct) {
    return static_cast<int>(expr.slot);
  }
  if (expr.kind == parser::Expr::Kind::Name) {
    const Local* local = find_local_entry(locals, expr.value);
    if (local == nullptr) {
      return -1;
    }
    const StructRef* decl = find_struct(structs, bare_type(local->type_name));
    if (decl == nullptr) {
      return -1;
    }
    return static_cast<int>(decl->index);
  }
  if (expr.kind == parser::Expr::Kind::Call && g_bound_functions != nullptr) {
    // heal(f).hp: the struct the called function returns
    for (const FuncRef& function : *g_bound_functions) {
      if (function.name == expr.value && !function.return_type.empty()) {
        const StructRef* decl = find_struct(structs, bare_type(function.return_type));
        return decl == nullptr ? -1 : static_cast<int>(decl->index);
      }
    }
    return -1;
  }
  if (expr.kind == parser::Expr::Kind::Member && expr.left != nullptr) {
    // p.stats.hp: the struct of `p.stats` is the declared type of the field `stats`.
    const int parent = struct_index_of(*expr.left, locals, structs);
    if (parent < 0) {
      return -1;
    }
    for (const StructRef& owner : structs) {
      if (owner.index != static_cast<std::uint16_t>(parent)) {
        continue;
      }
      for (std::size_t field = 0; field < owner.fields.size() && field < owner.field_types.size(); ++field) {
        if (owner.fields[field] == expr.value) {
          const StructRef* decl = find_struct(structs, bare_type(owner.field_types[field]));
          return decl == nullptr ? -1 : static_cast<int>(decl->index);
        }
      }
    }
    return -1;
  }
  if (expr.kind == parser::Expr::Kind::Index && expr.left != nullptr && expr.left->kind == parser::Expr::Kind::Name) {
    // team[1].hp with `array<Stats> team` or `list<Stats> team`: the element type.
    const Local* local = find_local_entry(locals, expr.left->value);
    if (local == nullptr) {
      return -1;
    }
    const std::string& type = local->type_name;
    const std::size_t open = type.find('<');
    if (open == std::string::npos || type.back() != '>' ||
        (type.compare(0, open, "array") != 0 && type.compare(0, open, "list") != 0)) {
      return -1;
    }
    const StructRef* decl = find_struct(structs, type.substr(open + 1, type.size() - open - 2));
    return decl == nullptr ? -1 : static_cast<int>(decl->index);
  }
  return -1;
}

// Inside a method, `@hp` / `@this.hp` / `@this::hp` name a field of self (when self has one);
// elsewhere `@name` keeps meaning the shared self table.
[[nodiscard]] bool self_has_field(const std::vector<Local>& locals, const std::vector<StructRef>& structs,
                                  const std::string& field) {
  const Local* self = find_local_entry(locals, "self");
  if (self == nullptr) {
    return false;
  }
  const StructRef* decl = find_struct(structs, bare_type(self->type_name));
  return decl != nullptr && find_field(*decl, field) >= 0;
}

[[nodiscard]] parser::Expr self_member(const std::string& field, const std::string_view span, const SourceLocation location) {
  parser::Expr receiver;
  receiver.kind = parser::Expr::Kind::Name;
  receiver.value = "self";
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

void bind_expr(parser::Expr& expr, const std::vector<Local>& locals, const std::vector<FuncRef>& functions,
               const std::vector<StructRef>& structs, const std::vector<EnumRef>& enums,
               std::vector<Diagnostic>& diagnostics) {
  if (expr.kind == parser::Expr::Kind::SelfField && self_has_field(locals, structs, expr.value)) {
    expr = self_member(expr.value, expr.span, expr.location);
  }
  if (expr.kind == parser::Expr::Kind::Update) {
    if (expr.left != nullptr) {
      bind_expr(*expr.left, locals, functions, structs, enums, diagnostics);
      expr.slot = expr.left->slot;
      expr.value = expr.left->value;
    }
    return;
  }
  if (expr.kind == parser::Expr::Kind::Name && expr.value == "None") {
    expr.kind = parser::Expr::Kind::Construct;
    expr.payload_ctor = true;
    expr.slot = 65534;
    parser::Expr tag;
    tag.kind = parser::Expr::Kind::Number;
    parser::Expr filler;
    filler.kind = parser::Expr::Kind::Number;
    expr.args.push_back(std::move(tag));
    expr.args.push_back(std::move(filler));
    return;
  }
  if (expr.kind == parser::Expr::Kind::Name) {
    const int slot = find_local(locals, expr.value);
    if (slot < 0) {
      diagnostics.push_back(Diagnostic{expr.location, expr.span, "undefined name"});
      return;
    }
    expr.slot = static_cast<std::uint16_t>(slot);
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
    } else if (local->constant) {
      diagnostics.push_back(Diagnostic{expr.left->location, expr.left->span, "cannot move"});
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
      if (side.kind == parser::Expr::Kind::Name) {
        const Local* local = find_local_entry(locals, side.value);
        if (local != nullptr && !is_builtin_type(local->type_name)) {
          return local->type_name;
        }
      }
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
        const int index = take_function(0, 1);
        if (index < 0) {
          return;
        }
        if (active_listeners != nullptr) {
          if (active_listeners->size() <= local->slot) {
            active_listeners->resize(static_cast<std::size_t>(local->slot) + 1);
          }
          (*active_listeners)[local->slot].push_back(static_cast<std::uint16_t>(index));
        }
        expr.kind = parser::Expr::Kind::Number;
        expr.number = 0;
        expr.args.clear();
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
      expr.slot = static_cast<std::uint16_t>(index);
      expr.args.erase(expr.args.begin());
      bind_expr(expr.args[0], locals, functions, structs, enums, diagnostics);
      return;
    }
    if (bare_type(expr.value) == "array" || bare_type(expr.value) == "dictionary") {
      for (parser::Expr& arg : expr.args) {
        bind_expr(arg, locals, functions, structs, enums, diagnostics);
      }
      if (bare_type(expr.value) == "dictionary") {
        expr.slot = 65535;
      }
      expr.kind = parser::Expr::Kind::MakeList;
      return;
    }
    if ((expr.value == "Some" || expr.value == "Ok" || expr.value == "Err") && expr.args.size() == 1) {
      bind_expr(expr.args[0], locals, functions, structs, enums, diagnostics);
      parser::Expr payload = std::move(expr.args[0]);
      parser::Expr tag;
      tag.kind = parser::Expr::Kind::Number;
      tag.number = expr.value == "Err" ? 0 : 1;
      expr.kind = parser::Expr::Kind::Construct;
      expr.payload_ctor = true;
      expr.slot = expr.value == "Some" ? 65534 : 65533;
      expr.args.clear();
      expr.args.push_back(std::move(tag));
      expr.args.push_back(std::move(payload));
      return;
    }
    if (expr.value == "None" && expr.args.empty()) {
      expr.kind = parser::Expr::Kind::Name;
      expr.value = "None";
      bind_expr(expr, locals, functions, structs, enums, diagnostics);
      return;
    }
    const std::size_t dot = expr.value.find('.');
    if (dot != std::string::npos) {
      const std::string owner = expr.value.substr(0, dot);
      const std::string method = expr.value.substr(dot + 1);
      if (const EnumRef* enumeration = find_enum(enums, owner); enumeration != nullptr) {
        const int variant = find_variant(*enumeration, method);
        if (variant >= 0 && variant < static_cast<int>(enumeration->payload_types.size()) &&
            !enumeration->payload_types[static_cast<std::size_t>(variant)].empty() && expr.args.size() == 1) {
          bind_expr(expr.args[0], locals, functions, structs, enums, diagnostics);
          parser::Expr payload = std::move(expr.args[0]);
          parser::Expr tag;
          tag.kind = parser::Expr::Kind::Number;
          tag.number = static_cast<double>(variant);
          expr.kind = parser::Expr::Kind::Construct;
          expr.payload_ctor = true;
          expr.slot = static_cast<std::uint16_t>(enumeration - enums.data());
          expr.args.clear();
          expr.args.push_back(std::move(tag));
          expr.args.push_back(std::move(payload));
          return;
        }
      }
      const Local* local = find_local_entry(locals, owner);
      const StructRef* type = local == nullptr ? nullptr : find_struct(structs, local->type_name);
      if (type != nullptr && method.find('.') == std::string::npos) {
        int slot = -1;
        for (std::size_t index = 0; index < type->method_names.size(); ++index) {
          if (type->method_names[index] == method) {
            slot = static_cast<int>(index);
            break;
          }
        }
        const FuncRef* target = nullptr;
        if (slot >= 0 && static_cast<std::size_t>(slot) < type->vtable.size()) {
          for (const FuncRef& symbol : functions) {
            if (symbol.index == type->vtable[static_cast<std::size_t>(slot)]) {
              target = &symbol;
              break;
            }
          }
        }
        if (target == nullptr) {
          diagnostics.push_back(Diagnostic{expr.location, expr.span, "undefined function"});
          return;
        }
        if (target->is_private && (active_owner == nullptr || *active_owner != owner)) {
          diagnostics.push_back(Diagnostic{expr.location, expr.span, "private member"});
          return;
        }
        if (expr.args.size() + 1 != target->arity) {
          diagnostics.push_back(Diagnostic{expr.location, expr.span, "wrong number of arguments"});
          return;
        }
        parser::Expr receiver;
        receiver.kind = parser::Expr::Kind::Name;
        receiver.value = owner;
        receiver.span = expr.span;
        receiver.location = expr.location;
        expr.args.insert(expr.args.begin(), std::move(receiver));
        expr.virtual_call = true;
        expr.slot = static_cast<std::uint16_t>(slot);
        expr.value = target->name;
        for (parser::Expr& arg : expr.args) {
          bind_expr(arg, locals, functions, structs, enums, diagnostics);
        }
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
      if (expr.args[0].kind != parser::Expr::Kind::Name) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, expr.value + " needs a list variable as its first argument"});
        return;
      }
      const Local* target = find_local_entry(locals, expr.args[0].value);
      if (target == nullptr) {
        diagnostics.push_back(Diagnostic{expr.args[0].location, expr.args[0].span, "undefined name"});
        return;
      }
      if (target->constant) {
        diagnostics.push_back(Diagnostic{expr.args[0].location, expr.args[0].span,
                                         "cannot assign to immutable binding (declare it with let mut or a type)"});
        return;
      }
      expr.kind = parser::Expr::Kind::ListMut;
      expr.slot = target->slot;
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
    if (SignalBind* signal = find_signal(expr.value); signal != nullptr) {
      expr.signal_call = true;
      expr.targets = signal->targets;
      for (parser::Expr& arg : expr.args) {
        bind_expr(arg, locals, functions, structs, enums, diagnostics);
      }
      return;
    }
    if (const VariantRef* variant = find_variant_type(expr.value); variant != nullptr) {
      if (expr.args.size() != 1) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "wrong number of arguments"});
      }
      expr.kind = parser::Expr::Kind::Construct;
      expr.variant_ctor = true;
      expr.slot = variant->index;
      for (parser::Expr& arg : expr.args) {
        bind_expr(arg, locals, functions, structs, enums, diagnostics);
      }
      return;
    }
    const StructRef* constructed = find_struct(structs, bare_type(expr.value));
    if (constructed != nullptr) {
      if (constructed->is_abstract) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "abstract type"});
      }
      // Player(hp: 5, name: "Ada"): named arguments are put in field order.
      bool any_named = false;
      for (const std::string& name : expr.arg_names) {
        any_named = any_named || !name.empty();
      }
      if (any_named && expr.args.size() == constructed->fields.size()) {
        std::vector<parser::Expr> ordered(constructed->fields.size());
        std::vector<char> filled(constructed->fields.size(), 0);
        std::size_t positional = 0;
        bool ok = true;
        for (std::size_t index = 0; index < expr.args.size() && ok; ++index) {
          const std::string name = index < expr.arg_names.size() ? expr.arg_names[index] : std::string{};
          std::size_t slot = positional;
          if (!name.empty()) {
            const int field = find_field(*constructed, name);
            if (field < 0) {
              diagnostics.push_back(Diagnostic{expr.location, expr.span, "unknown field: " + name});
              ok = false;
              break;
            }
            slot = static_cast<std::size_t>(field);
          } else {
            ++positional;
          }
          if (slot >= ordered.size() || filled[slot] != 0) {
            diagnostics.push_back(Diagnostic{expr.location, expr.span, "field given twice"});
            ok = false;
            break;
          }
          ordered[slot] = std::move(expr.args[index]);
          filled[slot] = 1;
        }
        if (ok) {
          expr.args = std::move(ordered);
          expr.arg_names.clear();
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
          if (slot >= arity) {
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
          if (slot < definition->has_default.size() && definition->has_default[slot] != 0) {
            ordered[slot] = clone_expr(definition->defaults[slot]);
            filled[slot] = 1;
          } else {
            failed = true;
          }
        }
        if (failed) {
          diagnostics.push_back(Diagnostic{expr.location, expr.span, "wrong number of arguments"});
        } else {
          expr.args = std::move(ordered);
          expr.slot = definition->index;
        }
      }
    }
    for (parser::Expr& arg : expr.args) {
      bind_expr(arg, locals, functions, structs, enums, diagnostics);
    }
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
    if (expr.left != nullptr) {
      bind_expr(*expr.left, locals, functions, structs, enums, diagnostics);
    }
    const int struct_index = expr.left == nullptr ? -1 : struct_index_of(*expr.left, locals, structs);
    if (struct_index >= 0) {
      const StructRef* decl = struct_at(structs, static_cast<std::uint16_t>(struct_index));
      const int field = decl == nullptr ? -1 : find_field(*decl, expr.value);
      if (field < 0) {
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
    return std::to_string(static_cast<long long>(expr.number));
  }
  if (expr.kind == parser::Expr::Kind::String) {
    return "\"" + expr.value + "\"";
  }
  if (expr.kind == parser::Expr::Kind::Name) {
    const Local* local = find_local_entry(locals, expr.value);
    if (local != nullptr) {
      return local->value;
    }
  }
  if (expr.kind == parser::Expr::Kind::Construct) {
    std::string text = expr.value + "(";
    for (std::size_t index = 0; index < expr.args.size(); ++index) {
      const std::string part = literal_text(expr.args[index], locals);
      if (part.empty()) {
        return {};
      }
      if (index != 0) {
        text += ", ";
      }
      text += part;
    }
    text += ")";
    return text;
  }
  return {};
}

[[nodiscard]] std::string inferred_type(const parser::Expr& expr, const std::vector<Local>& locals) {
  if (expr.kind == parser::Expr::Kind::Construct && !expr.value.empty()) {
    return expr.value;
  }
  if (expr.kind == parser::Expr::Kind::Name) {
    const Local* local = find_local_entry(locals, expr.value);
    if (local != nullptr) {
      return local->type_name;
    }
  }
  if (expr.kind == parser::Expr::Kind::Number) {
    if (expr.enum_literal) {
      return {};
    }
    if (expr.bool_literal) {
      return "bool";
    }
    if (!expr.null_literal) {
      return expr.float_literal ? "float" : "int";
    }
  }
  return {};
}

void remember_binding(parser::Stmt& stmt, std::vector<Local>& locals) {
  if (stmt.declared_type.empty()) {
    stmt.declared_type = inferred_type(stmt.expr, locals);
  }
  stmt.remembered = literal_text(stmt.expr, locals);
}

void bind_stmts(std::vector<parser::Stmt>& statements, std::vector<Local>& locals,
                const std::vector<FuncRef>& functions, const std::vector<StructRef>& structs,
                const std::vector<EnumRef>& enums, std::size_t& next_slot, std::vector<Diagnostic>& diagnostics) {
  const std::size_t watermark = locals.size();
  for (parser::Stmt& stmt : statements) {
    if (stmt.kind == parser::Stmt::Kind::Let) {
      if (stmt.has_decltype) {
        bind_expr(stmt.type_expr, locals, functions, structs, enums, diagnostics);
      }
      bind_expr(stmt.expr, locals, functions, structs, enums, diagnostics);
      if (declared_since(locals, watermark, stmt.name)) {
        diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "already declared"});
        continue;
      }
      if (next_slot >= 65535) {
        diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "too many locals"});
        continue;
      }
      stmt.slot = static_cast<std::uint16_t>(next_slot);
      ++next_slot;
      remember_binding(stmt, locals);
      if (stmt.unpack.size() > 1) {
        for (const std::string& name : stmt.unpack) {
          if (next_slot >= 65535) {
            break;
          }
          const auto slot = static_cast<std::uint16_t>(next_slot);
          ++next_slot;
          stmt.unpack_slots.push_back(slot);
          locals.push_back(Local{name, slot, {}});
        }
      } else {
      locals.push_back(Local{stmt.name, stmt.slot, stmt.declared_type, stmt.immutable, stmt.remembered});
      }
      if (stmt.observable_cell) {
        locals.back().observable = true;
      }
      continue;
    }

    if (stmt.kind == parser::Stmt::Kind::SelfAssign && self_has_field(locals, structs, stmt.name)) {
      stmt.place = self_member(stmt.name, stmt.name_span, stmt.name_location);
      stmt.kind = parser::Stmt::Kind::PlaceAssign;
      stmt.name = "self";
    }
    if (stmt.kind == parser::Stmt::Kind::PlaceAssign) {
      const Local* local = find_local_entry(locals, stmt.name);
      if (local == nullptr) {
        diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "undefined name"});
      } else if (local->constant) {
        diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span,
                                         "cannot assign to immutable binding (declare it with let mut or a type)"});
      } else {
        stmt.slot = local->slot;
      }
      bind_expr(stmt.place, locals, functions, structs, enums, diagnostics);
      bind_expr(stmt.expr, locals, functions, structs, enums, diagnostics);
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
        if (local->observable && active_listeners != nullptr && local->slot < active_listeners->size()) {
          stmt.listeners = (*active_listeners)[local->slot];
        }
      }
      bind_expr(stmt.expr, locals, functions, structs, enums, diagnostics);
      stmt.remembered = literal_text(stmt.expr, locals);
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
        if (init.kind == parser::Stmt::Kind::Let) {
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
        // for (let e in wave) with `array<Enemy> wave`: e is an Enemy, so e.hp resolves.
        std::string item_type = "int";
        if (stmt.expr.kind == parser::Expr::Kind::Name) {
          if (const Local* source = find_local_entry(locals, stmt.expr.value); source != nullptr) {
            const std::string& type = source->type_name;
            const std::size_t open = type.find('<');
            if (open != std::string::npos && type.back() == '>' &&
                (type.compare(0, open, "array") == 0 || type.compare(0, open, "list") == 0)) {
              item_type = type.substr(open + 1, type.size() - open - 2);
            } else if (!type.empty() && type != "int" && type != "float" && type != "double") {
              item_type = "";
            }
          }
        } else if (stmt.expr.kind != parser::Expr::Kind::Number) {
          item_type = "";
        }
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
      if (stmt.if_let && next_slot < 65535) {
        stmt.slot = static_cast<std::uint16_t>(next_slot);
        ++next_slot;
        locals.push_back(Local{stmt.name, stmt.slot, {}});
      }
      bind_stmts(stmt.then_body, locals, functions, structs, enums, next_slot, diagnostics);
      if (stmt.if_let && !locals.empty()) {
        locals.pop_back();
      }
      if (stmt.has_else) {
        bind_stmts(stmt.else_body, locals, functions, structs, enums, next_slot, diagnostics);
      }
      continue;
    }

    if (stmt.kind == parser::Stmt::Kind::Match) {
      bind_expr(stmt.expr, locals, functions, structs, enums, diagnostics);
      for (parser::Stmt::MatchArm& arm : stmt.arms) {
        if (!arm.wildcard && arm.pattern.kind == parser::Expr::Kind::Call && arm.pattern.args.size() == 1 &&
            arm.pattern.args[0].kind == parser::Expr::Kind::Name) {
          const std::string ctor = arm.pattern.value;
          const std::string bound = arm.pattern.args[0].value;
          int tag = -1;
          if (ctor == "Some" || ctor == "Ok") {
            tag = 1;
          } else if (ctor == "Err") {
            tag = 0;
          } else {
            int hits = 0;
            for (const EnumRef& enumeration : enums) {
              const int found = find_variant(enumeration, ctor);
              if (found >= 0 && found < static_cast<int>(enumeration.payload_types.size()) &&
                  !enumeration.payload_types[static_cast<std::size_t>(found)].empty()) {
                ++hits;
                tag = found;
              }
            }
            if (hits != 1) {
              tag = -1;
            }
          }
          if (tag >= 0) {
            arm.has_payload = true;
            arm.tag_match = true;
            arm.bind_name = bound;
            arm.pattern.kind = parser::Expr::Kind::Number;
            arm.pattern.number = static_cast<double>(tag);
            arm.pattern.args.clear();
            stmt.slot = 1;
          }
        }
        if (!arm.wildcard && arm.pattern.kind == parser::Expr::Kind::Name && arm.pattern.value == "None") {
          arm.tag_match = true;
          arm.pattern.kind = parser::Expr::Kind::Number;
          arm.pattern.number = 0;
          stmt.slot = 1;
        }
        if (!arm.wildcard && arm.pattern.kind == parser::Expr::Kind::Name) {
          int hits = 0;
          int enum_index = -1;
          int variant = -1;
          for (std::size_t index = 0; index < enums.size(); ++index) {
            const int found = find_variant(enums[index], arm.pattern.value);
            if (found >= 0) {
              ++hits;
              enum_index = static_cast<int>(index);
              variant = found;
            }
          }
          if (hits == 1) {
            arm.pattern.kind = parser::Expr::Kind::Number;
            arm.pattern.number = static_cast<double>(variant);
            arm.pattern.enum_literal = true;
            arm.pattern.slot = static_cast<std::uint16_t>(enum_index);
          }
        }
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

    bind_expr(stmt.expr, locals, functions, structs, enums, diagnostics);
  }
  locals.resize(watermark);
}

}  // namespace

void collect_declarations(std::vector<parser::Stmt>& statements, std::vector<FuncRef>& functions,
                          std::vector<Diagnostic>& diagnostics) {
  for (parser::Stmt& stmt : statements) {
    if (stmt.kind == parser::Stmt::Kind::Signal) {
      if (find_signal(stmt.name) != nullptr || find_func(functions, stmt.name) >= 0) {
        diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "already declared"});
      } else if (active_signals != nullptr) {
        active_signals->push_back(SignalBind{stmt.name, {}});
      }
    } else if (stmt.kind == parser::Stmt::Kind::Connect) {
      SignalBind* signal = find_signal(stmt.name);
      const int function = find_func(functions, stmt.declared_type);
      if (signal == nullptr) {
        diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "undefined name"});
      } else if (function < 0) {
        diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "undefined function"});
      } else {
        signal->targets.push_back(static_cast<std::uint16_t>(function));
      }
    } else if (stmt.kind == parser::Stmt::Kind::Using) {
      const int function = find_func(functions, stmt.declared_type);
      if (function < 0) {
        diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "undefined function"});
      } else if (find_func(functions, stmt.name) >= 0) {
        diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "already declared"});
      } else {
        functions.push_back(FuncRef{stmt.name, 0, static_cast<std::uint16_t>(function), false});
        for (const FuncRef& existing : functions) {
          if (existing.index == static_cast<std::uint16_t>(function) && existing.name == stmt.declared_type) {
            functions.back().arity = existing.arity;
            functions.back().is_async = existing.is_async;
            break;
          }
        }
      }
    }
    collect_declarations(stmt.then_body, functions, diagnostics);
    collect_declarations(stmt.else_body, functions, diagnostics);
    collect_declarations(stmt.init, functions, diagnostics);
    collect_declarations(stmt.step, functions, diagnostics);
    for (parser::Stmt::MatchArm& arm : stmt.arms) {
      collect_declarations(arm.body, functions, diagnostics);
    }
  }
}

void bind(parser::Program& program, std::vector<Diagnostic>& diagnostics) {
  active_program = &program;
  std::vector<StructRef> structs;
  for (std::size_t index = 0; index < program.structs.size(); ++index) {
    parser::StructDecl& decl = program.structs[index];
    decl.index = static_cast<std::uint16_t>(index);
    StructRef ref;
    ref.name = decl.name;
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
    for (std::size_t base_at = 0; base_at < base_names.size(); ++base_at) {
      const StructRef* base = find_struct(structs, base_names[base_at]);
      const bool known = base != nullptr && base->index < decl.index;
      if (!known) {
        const SourceLocation where = base_at < decl.base_locations.size() ? decl.base_locations[base_at] : decl.base_location;
        const std::string_view span = base_at < decl.base_spans.size() ? decl.base_spans[base_at] : decl.base_span;
        diagnostics.push_back(Diagnostic{where, span, "unknown type"});
        continue;
      }
      decl.base_indices.push_back(base->index);
      if (decl.base_index == 65535) {
        decl.base_index = base->index;
      }
      for (const parser::Field& inherited : program.structs[base->index].fields) {
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
    structs[index].fields.clear();
    structs[index].field_owners.clear();
    structs[index].field_types.clear();
    structs[index].private_fields.clear();
    structs[index].is_abstract = decl.is_abstract;
    for (const parser::Field& field : decl.fields) {
      structs[index].fields.push_back(field.name);
      structs[index].field_owners.push_back(field.owner);
      structs[index].field_types.push_back(field.type_name);
      structs[index].private_fields.push_back(field.is_private ? 1 : 0);
    }
  }

  std::vector<EnumRef> enums;
  for (const parser::EnumDecl& decl : program.enums) {
    EnumRef ref;
    ref.name = decl.name;
    ref.variants = decl.variants;
    ref.payload_types = decl.payload_types;
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
    ref.is_extern = function.is_extern;
    ref.extern_id = function.extern_id;
    ref.is_private = function.is_private;
    ref.is_abstract = function.is_abstract;
    ref.is_override = function.is_override;
    ref.is_final = function.is_final;
    ref.return_type = function.return_type;
    functions.push_back(std::move(ref));
  }

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

  std::vector<VariantRef> variants;
  for (std::size_t index = 0; index < program.variants.size(); ++index) {
    variants.push_back(VariantRef{program.variants[index].name, static_cast<std::uint16_t>(index)});
  }
  active_variants = &variants;

  std::vector<SignalBind> signals;
  active_signals = &signals;
  collect_declarations(program.statements, functions, diagnostics);
  for (parser::Function& function : program.functions) {
    collect_declarations(function.body, functions, diagnostics);
  }

  std::vector<std::vector<std::uint16_t>> listeners;
  active_listeners = &listeners;
  for (parser::Function& function : program.functions) {
    std::vector<Local> locals;
    std::size_t next_slot = 0;
    for (std::size_t param = 0; param < function.params.size(); ++param) {
      if (find_local(locals, function.params[param]) >= 0) {
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

  std::vector<Local> locals;
  std::size_t next_slot = 0;
  bind_stmts(program.statements, locals, functions, structs, enums, next_slot, diagnostics);
  program.local_count = static_cast<std::uint16_t>(next_slot);
  active_signals = nullptr;
  active_variants = nullptr;
  active_listeners = nullptr;
  active_program = nullptr;
}

}  // namespace clpp::binder
