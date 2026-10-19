#include "core/typechecker/solver.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace clpp::typechecker {

namespace {

thread_local const std::vector<parser::EnumDecl>* active_enums = nullptr;
thread_local const std::vector<parser::TypeAlias>* active_aliases = nullptr;
thread_local const std::vector<parser::StructDecl>* active_structs = nullptr;
thread_local parser::CheckMode active_mode = parser::CheckMode::Nonstrict;
thread_local const parser::Program* active_program = nullptr;

struct Type {
  enum class Kind {
    Any, Void, Int, Float, Bool, String, Vector3, Buffer, Struct, Enum, Task, Literal, Union, Intersect, Null, Optional, Parameter, Function, Signal, Connection, Tuple, Error
  };

  Kind kind{Kind::Any};
  std::uint16_t index{0};
  Kind payload{Kind::Any};
  std::vector<Type> parts;
  std::string literal;

  Type() = default;
  Type(const Kind kind_in, const std::uint16_t index_in) : kind(kind_in), index(index_in) {}

  [[nodiscard]] friend bool operator==(const Type& left, const Type& right) {
    if (left.kind != right.kind) {
      return false;
    }
    if (left.kind == Kind::Struct || left.kind == Kind::Enum) {
      return left.index == right.index && left.parts == right.parts;
    }
    if (left.kind == Kind::Parameter) return left.index == right.index && left.literal == right.literal;
    if (left.kind == Kind::Literal) {
      return left.literal == right.literal;
    }
    if (left.kind == Kind::Union || left.kind == Kind::Intersect || left.kind == Kind::Optional || left.kind == Kind::Function || left.kind == Kind::Signal || left.kind == Kind::Tuple) {
      return left.parts == right.parts;
    }
    if (left.kind == Kind::Task) {
      return left.parts == right.parts && left.payload == right.payload && (left.payload != Kind::Struct || left.index == right.index);
    }
    return true;
  }
};

const Type kAny{Type::Kind::Any, 0};
const Type kVoid{Type::Kind::Void, 0};
const Type kInt{Type::Kind::Int, 0};
const Type kFloat{Type::Kind::Float, 0};
const Type kBool{Type::Kind::Bool, 0};
const Type kString{Type::Kind::String, 0};
const Type kVector3{Type::Kind::Vector3, 0};
const Type kBuffer{Type::Kind::Buffer, 0};
const Type kNull{Type::Kind::Null, 0};
const Type kError{Type::Kind::Error, 0};

struct Binding {
  std::string name;
  Type type{kAny};
  Type declared{kAny};
  bool global{false};
  Binding() = default;
  Binding(std::string name_in, const Type type_in, bool global_in = false) : name(std::move(name_in)), type(type_in), declared(type_in), global(global_in) {}
};

bool optional_operand(const parser::Expr& expr, const Type& type, const std::vector<Binding>& bindings) {
  if (type.kind == Type::Kind::Optional || type.kind == Type::Kind::Null) return true;
  if (expr.kind != parser::Expr::Kind::Name) return false;
  for (auto binding = bindings.rbegin(); binding != bindings.rend(); ++binding) {
    if (binding->name == expr.value && (!expr.global || binding->global))
      return binding->declared.kind == Type::Kind::Optional;
  }
  return false;
}

void optional_condition(const parser::Expr& expr, const Type& type, const std::vector<Binding>& bindings) {
  if (!optional_operand(expr, type, bindings)) return;
  // Lower a condition to !!value, with the inner negation testing null/false exactly once.
  auto& condition = const_cast<parser::Expr&>(expr);
  const auto location = condition.location;
  const auto span = condition.span;
  auto original = std::make_unique<parser::Expr>(std::move(condition));
  parser::Expr negative;
  negative.kind = parser::Expr::Kind::Not;
  negative.runtime_op = 1;
  negative.location = location;
  negative.span = span;
  negative.left = std::move(original);
  condition = parser::Expr{};
  condition.kind = parser::Expr::Kind::Not;
  condition.location = location;
  condition.span = span;
  condition.left = std::make_unique<parser::Expr>(std::move(negative));
}

Type type_test_target(const parser::Expr& expr);

void narrow_condition(const parser::Expr& condition, const bool truth, std::vector<Binding>& bindings) {
  if (condition.kind == parser::Expr::Kind::Not && condition.left != nullptr) {
    if (condition.runtime_op == 1 && condition.left->kind == parser::Expr::Kind::Name) {
      // !optional is true for null or false. Only its false branch proves presence.
      if (!truth) for (auto binding = bindings.rbegin(); binding != bindings.rend(); ++binding) {
        if (binding->name != condition.left->value || (condition.left->global && !binding->global)) continue;
        if (binding->declared.kind == Type::Kind::Optional && binding->declared.parts.size() == 1)
          binding->type = binding->declared.parts[0];
        break;
      }
      return;
    }
    narrow_condition(*condition.left, !truth, bindings);
    return;
  }
  if (((condition.kind == parser::Expr::Kind::And && truth) ||
       (condition.kind == parser::Expr::Kind::Or && !truth)) && condition.left != nullptr && condition.right != nullptr) {
    narrow_condition(*condition.left, truth, bindings);
    narrow_condition(*condition.right, truth, bindings);
    return;
  }
  if (condition.kind == parser::Expr::Kind::TypeTest && condition.left &&
      condition.left->kind == parser::Expr::Kind::Name && truth) {
    for (auto binding = bindings.rbegin(); binding != bindings.rend(); ++binding) {
      if (binding->name != condition.left->value || (condition.left->global && !binding->global)) continue;
      const Type target = type_test_target(condition);
      if (target != kError) binding->type = target;
      break;
    }
    return;
  }
  if (condition.kind == parser::Expr::Kind::Name && truth) {
    for (auto binding = bindings.rbegin(); binding != bindings.rend(); ++binding) {
      if (binding->name != condition.value || (condition.global && !binding->global)) continue;
      if (binding->declared.kind == Type::Kind::Optional && binding->declared.parts.size() == 1)
        binding->type = binding->declared.parts[0];
      break;
    }
    return;
  }
  if (condition.kind != parser::Expr::Kind::Eq && condition.kind != parser::Expr::Kind::NotEq) return;
  if (condition.left == nullptr || condition.right == nullptr) return;
  const parser::Expr* name = condition.left.get();
  const parser::Expr* value = condition.right.get();
  if (name->null_literal) std::swap(name, value);
  if (name->kind != parser::Expr::Kind::Name || !value->null_literal) return;
  const bool present = (condition.kind == parser::Expr::Kind::NotEq) == truth;
  for (auto binding = bindings.rbegin(); binding != bindings.rend(); ++binding) {
    if (binding->name != name->value) continue;
    if (binding->declared.kind == Type::Kind::Optional && binding->declared.parts.size() == 1) {
      binding->type = present ? binding->declared.parts[0] : kNull;
    }
    break;
  }
}

struct Signature {
  std::string name;
  std::vector<Type> params;
  Type result{kAny};
  bool is_async{false};
  bool anonymous{false};
  std::uint16_t index{65535};
  std::size_t owner_type_param_count{0};
  std::vector<std::string> type_params;
  std::vector<std::string> where_names;
  std::vector<std::string> where_types;
};

[[nodiscard]] Type task_of(const Type inner) {
  Type type;
  type.kind = Type::Kind::Task;
  type.payload = inner.kind;
  type.index = inner.index;
  type.parts.push_back(inner);
  return type;
}

[[nodiscard]] Type unwrap_task(const Type task) {
  if (task.parts.size() == 1) return task.parts[0];
  Type inner;
  inner.kind = task.payload;
  inner.index = task.index;
  return inner;
}

struct TypeScope {
  std::string name;
  std::vector<std::string> type_params;
  std::vector<Type> bounds;
  bool is_async{false};
};

struct StructInfo : TypeScope {
  std::vector<Type> fields;
  std::vector<Type> bases;
  bool is_abstract{false};
};

thread_local const TypeScope* active_function_types = nullptr;
thread_local const std::vector<StructInfo>* active_type_info = nullptr;

[[nodiscard]] std::vector<std::string_view> split_type(const std::string_view text, const char separator) {
  std::vector<std::string_view> parts;
  std::size_t start = 0;
  bool quoted = false;
  int depth = 0;
  for (std::size_t index = 0; index < text.size(); ++index) {
    if (text[index] == '"' && (index == 0 || text[index - 1] != '\\')) {
      quoted = !quoted;
    } else if (!quoted && (text[index] == '<' || text[index] == '(')) {
      ++depth;
    } else if (!quoted && (text[index] == ')' || (text[index] == '>' && (index == 0 || text[index - 1] != '-')))) {
      --depth;
    } else if (!quoted && depth == 0 && text[index] == separator) {
      parts.push_back(text.substr(start, index - start));
      start = index + 1;
    }
  }
  parts.push_back(text.substr(start));
  return parts;
}

[[nodiscard]] Type named_type(const std::string_view name, const std::vector<StructInfo>& structs, const int depth = 0,
                              const TypeScope* context = active_function_types) {
  if (depth > 8) {
    return kError;
  }
  if (context != nullptr) {
    for (std::size_t slot = 0; slot < context->type_params.size(); ++slot) {
      if (name == context->type_params[slot]) {
        Type parameter{Type::Kind::Parameter, static_cast<std::uint16_t>(slot)};
        parameter.literal = context->name;
        return parameter;
      }
    }
  }
  if (name.empty()) {
    return kAny;
  }
  if (!name.empty() && name.front() == '(') {
    int nesting = 0; std::size_t close = std::string_view::npos;
    for (std::size_t i = 0; i < name.size(); ++i) {
      if (name[i] == '(') ++nesting;
      else if (name[i] == ')' && --nesting == 0) { close = i; break; }
    }
    if (close == std::string_view::npos || name.substr(close + 1, 2) != "->") return kError;
    Type callable{Type::Kind::Function, 0};
    const auto parameters = name.substr(1, close - 1);
    if (!parameters.empty()) {
      std::size_t start = 0; int level = 0;
      for (std::size_t i = 0; i <= parameters.size(); ++i) {
        if (i == parameters.size() || (parameters[i] == ',' && level == 0)) {
          auto part = named_type(parameters.substr(start, i - start), structs, depth + 1, context);
          if (part == kError || part == kVoid) return kError;
          callable.parts.push_back(std::move(part)); start = i + 1;
        } else if (parameters[i] == '(' || parameters[i] == '<') ++level;
        else if (parameters[i] == ')' || parameters[i] == '>') --level;
      }
    }
    auto result = named_type(name.substr(close + 3), structs, depth + 1, context);
    if (result == kError) return kError;
    callable.parts.push_back(std::move(result));
    return callable;
  }
  const auto unions = split_type(name, '|');
  const auto intersections = split_type(name, '&');
  if (unions.size() > 1 && intersections.size() > 1) {
    return kError;
  }
  if (unions.size() > 1 || intersections.size() > 1) {
    const char separator = unions.size() > 1 ? '|' : '&';
    Type combined;
    combined.kind = separator == '|' ? Type::Kind::Union : Type::Kind::Intersect;
    for (const std::string_view part : split_type(name, separator)) {
      const Type inner = named_type(part, structs, depth + 1, context);
      if (inner == kError) {
        return kError;
      }
      combined.parts.push_back(inner);
    }
    return combined;
  }
  if (name.size() >= 2 && name.front() == '"' && name.back() == '"') {
    Type literal;
    literal.kind = Type::Kind::Literal;
    literal.literal = std::string(name.substr(1, name.size() - 2));
    return literal;
  }
  if (name == "void" || name == "any") {
    return name == "void" ? kVoid : kAny;
  }
  if (name == "number") {
    return kInt;
  }
  if (name == "float") {
    return kFloat;
  }
  if (name == "bool") {
    return kBool;
  }
  if (name == "connection") return Type{Type::Kind::Connection, 0};
  if (name == "string") {
    return kString;
  }
  if (name == "Vector3" || name == "Vector2" || name == "Vector4") {
    return kVector3;
  }
  if (name == "buffer") {
    return kBuffer;
  }
  const std::size_t generic = name.find('<');
  if (generic != std::string_view::npos && name.back() == '>') {
    const std::string_view bare = name.substr(0, generic);
    const std::string_view args = name.substr(generic + 1, name.size() - generic - 2);
    if (bare == "signal" || bare == "tuple") {
      Type signal{bare == "signal" ? Type::Kind::Signal : Type::Kind::Tuple, 0};
      if (args.empty()) return signal;
      const auto payloads = split_type(args, ',');
      if (payloads.size() > 7) return kError;
      for (const auto payload : payloads) {
        const auto type = named_type(payload, structs, depth + 1, context);
        if (type == kError || type == kVoid) return kError;
        signal.parts.push_back(type);
      }
      return signal;
    }
    std::vector<std::string_view> pieces;
    std::size_t start = 0;
    int nest = 0;
    bool quoted = false;
    for (std::size_t index = 0; index <= args.size(); ++index) {
      if (index < args.size() && args[index] == '"' && (index == 0 || args[index - 1] != '\\')) quoted = !quoted;
      if (quoted) continue;
      if (index == args.size() || (args[index] == ',' && nest == 0)) {
        std::string_view piece = args.substr(start, index - start);
        while (!piece.empty() && piece.front() == ' ') {
          piece.remove_prefix(1);
        }
        while (!piece.empty() && piece.back() == ' ') piece.remove_suffix(1);
        if (piece.empty()) return kError;
        pieces.push_back(piece);
        start = index + 1;
      } else if (args[index] == '<') {
        ++nest;
      } else if (args[index] == '>') {
        --nest;
      }
    }
    if (bare == "array" && pieces.size() == 1) {
      Type list{Type::Kind::Struct, 65532};
      const Type element = named_type(pieces[0], structs, depth + 1, context);
      if (element == kError) return kError;
      list.parts.push_back(element);
      return list;
    }
    if (bare == "task" && pieces.size() == 1) {
      const Type inner = named_type(pieces[0], structs, depth + 1, context);
      if (inner == kError || inner == kNull) return kError;
      return task_of(inner);
    }
    if (bare == "dictionary" && pieces.size() == 2) {
      Type table{Type::Kind::Struct, 65531};
      const Type key = named_type(pieces[0], structs, depth + 1, context);
      const Type value = named_type(pieces[1], structs, depth + 1, context);
      if (key == kError || value == kError) return kError;
      table.parts.push_back(key);
      table.parts.push_back(value);
      return table;
    }
    if (bare == "optional" && pieces.size() == 1) {
      const Type inner = named_type(pieces[0], structs, depth + 1, context);
      if (inner == kError || inner == kVoid || inner == kNull) return kError;
      Type optional{Type::Kind::Optional, 0};
      optional.parts.push_back(inner);
      return optional;
    }
    if (active_aliases) for (const auto& alias : *active_aliases) {
      if (alias.name != bare) continue;
      if (pieces.size() != alias.type_params.size()) return kError;
      TypeScope scope; scope.name = "$alias:" + alias.name; scope.type_params = alias.type_params;
      auto body = named_type(alias.body, structs, depth + 1, &scope);
      std::vector<Type> arguments;
      for (const auto part : pieces) {
        auto argument = named_type(part, structs, depth + 1, context);
        if (argument == kError) return kError;
        arguments.push_back(std::move(argument));
      }
      const auto substitute = [&](auto&& self, Type type) -> Type {
        if (type.kind == Type::Kind::Parameter && type.literal == scope.name)
          return type.index < arguments.size() ? arguments[type.index] : kError;
        for (auto& part : type.parts) part = self(self, std::move(part));
        if (type.kind == Type::Kind::Task && type.parts.size() == 1) return task_of(type.parts[0]);
        return type;
      };
      return substitute(substitute, std::move(body));
    }
    for (std::size_t index = 0; index < structs.size(); ++index) {
      if (structs[index].name == bare) {
        if (pieces.size() != structs[index].type_params.size()) return kError;
        Type generic_type{Type::Kind::Struct, static_cast<std::uint16_t>(index)};
        for (const std::string_view piece : pieces) {
          const Type argument = named_type(piece, structs, depth + 1, context);
          if (argument == kError) return kError;
          generic_type.parts.push_back(argument);
        }
        return generic_type;
      }
    }
  }
  for (std::size_t index = 0; index < structs.size(); ++index) {
    if (structs[index].name == name) {
      if (!structs[index].type_params.empty()) return kError;
      return Type{Type::Kind::Struct, static_cast<std::uint16_t>(index)};
    }
  }
  if (active_enums != nullptr) {
    for (std::size_t index = 0; index < active_enums->size(); ++index) {
      if ((*active_enums)[index].name == name) {
        return Type{Type::Kind::Enum, static_cast<std::uint16_t>(index)};
      }
    }
  }
  if (active_aliases != nullptr) {
    for (const parser::TypeAlias& alias : *active_aliases) {
      if (alias.name == name) {
        if (!alias.type_params.empty()) return kError;
        return named_type(alias.body, structs, depth + 1, nullptr);
      }
    }
  }
  return kError;
}

[[nodiscard]] TypeScope function_scope(const parser::Function& nested, const std::vector<StructInfo>& structs, const parser::Program& program) {
  const parser::Function* owner = &nested;
  for (std::size_t depth = 0; owner->anonymous && owner->lexical_parent < program.functions.size() && depth < program.functions.size(); ++depth)
    owner = &program.functions[owner->lexical_parent];
  const auto& function = *owner;
  if (!function.type_owner.empty()) {
    for (const auto& data : structs) if (data.name == function.type_owner) {
      TypeScope scope = data; scope.is_async = nested.is_async; return scope;
    }
  }
  TypeScope scope;
  scope.name = "$function:" + function.name;
  scope.is_async = nested.is_async;
  scope.type_params = function.type_params;
  scope.bounds.assign(scope.type_params.size(), kError);
  for (std::size_t index = 0; index < function.where_names.size() && index < function.where_types.size(); ++index) {
    const auto parameter = std::find(scope.type_params.begin(), scope.type_params.end(), function.where_names[index]);
    if (parameter != scope.type_params.end()) {
      auto& bound = scope.bounds[parameter - scope.type_params.begin()];
      const auto next = named_type(function.where_types[index], structs, 0, &scope);
      if (bound == kError) bound = next;
      else { Type combined{Type::Kind::Intersect, 0}; combined.parts = {bound, next}; bound = std::move(combined); }
    }
  }
  return scope;
}

[[nodiscard]] Type parameter_bound(const Type& type) {
  if (type.kind != Type::Kind::Parameter || active_function_types == nullptr ||
      type.literal != active_function_types->name || type.index >= active_function_types->bounds.size()) return kError;
  return active_function_types->bounds[type.index];
}

[[nodiscard]] Type intersection_view(const Type& type, unsigned depth);
[[nodiscard]] Type member_view(Type type, const unsigned nesting = 0) {
  if (nesting > 32) return kError;
  for (std::size_t depth = 0; type.kind == Type::Kind::Parameter; ++depth) {
    if (active_function_types == nullptr || depth >= active_function_types->type_params.size()) return kError;
    type = parameter_bound(type);
  }
  return type.kind == Type::Kind::Intersect ? intersection_view(type, nesting + 1) : type;
}

[[nodiscard]] Type named_member_view(Type type, const std::string& name, const unsigned depth = 0) {
  if (depth > 32) return kError;
  if (type.kind == Type::Kind::Parameter) return named_member_view(parameter_bound(type), name, depth + 1);
  if (type.kind == Type::Kind::Intersect) {
    for (const auto& part : type.parts) {
      const auto view = named_member_view(part, name, depth + 1);
      if (view != kError) return view;
    }
    return kError;
  }
  if (type.kind != Type::Kind::Struct || active_structs == nullptr || type.index >= active_structs->size()) return kError;
  const auto& data = (*active_structs)[type.index];
  if (std::find(data.method_names.begin(), data.method_names.end(), name) != data.method_names.end() ||
      std::any_of(data.fields.begin(), data.fields.end(), [&](const auto& field) { return field.name == name; })) return type;
  return kError;
}

[[nodiscard]] Type specialize_type(Type type, const TypeScope& owner, const std::vector<Type>& arguments) {
  if (type.kind == Type::Kind::Parameter && type.literal == owner.name) {
    return type.index < arguments.size() ? arguments[type.index] : kError;
  }
  for (Type& part : type.parts) {
    part = specialize_type(std::move(part), owner, arguments);
    if (part == kError) return kError;
  }
  if (type.kind == Type::Kind::Task && type.parts.size() == 1) return task_of(type.parts[0]);
  if (type.kind == Type::Kind::Optional && type.parts.size() == 1 &&
      (type.parts[0] == kVoid || type.parts[0] == kNull)) return kError;
  return type;
}

[[nodiscard]] bool contains_parameter(const Type& type) {
  if (type.kind == Type::Kind::Parameter) return true;
  return std::any_of(type.parts.begin(), type.parts.end(), contains_parameter);
}

// Formal identities may survive a callback specialization only in their lexical scope.
[[nodiscard]] bool scoped_callback_type(const Type& type) {
  if (type.kind == Type::Kind::Parameter)
    return active_function_types != nullptr && type.literal == active_function_types->name &&
        type.index < active_function_types->type_params.size();
  return std::all_of(type.parts.begin(), type.parts.end(), scoped_callback_type);
}

[[nodiscard]] bool unresolved_type(const Type& type) {
  if (type == kAny || type == kError) return true;
  return std::any_of(type.parts.begin(), type.parts.end(), unresolved_type);
}

// Collect evidence for declaration-scoped formals; never infer a dynamic escape.
[[nodiscard]] bool infer_arguments(const Type& pattern, const Type& actual, const TypeScope& scope,
                                   std::vector<Type>& inferred, const bool widen_literal = true, const std::size_t fixed_prefix = 0);

[[nodiscard]] Type base_view(Type type, const std::uint16_t target, const unsigned depth = 0) {
  if (active_type_info == nullptr) return kError;
  if (depth > active_type_info->size()) return kError;
  if (type.kind == Type::Kind::Struct && type.index < active_type_info->size()) {
    if (type.index == target) return type;
    const auto& data = (*active_type_info)[type.index];
    Type result = kError;
    for (const auto& base : data.bases) {
      const auto candidate = base_view(specialize_type(base, data, type.parts), target, depth + 1);
      if (candidate == kError) continue;
      if (result != kError && result != candidate) return kError;
      result = candidate;
    }
    return result;
  }
  return kError;
}

// A formal bound must guarantee numeric values, not merely contain one numeric branch.
[[nodiscard]] unsigned numeric_bound_domain(const Type& type, const unsigned depth = 0) {
  if (depth > 32) return 0;
  if (type == kInt) return 1;
  if (type == kFloat) return 3;  // Numeric compatibility admits integers at a float bound.
  if (type.kind == Type::Kind::Parameter) return numeric_bound_domain(parameter_bound(type), depth + 1);
  if (type.kind == Type::Kind::Union || type.kind == Type::Kind::Intersect) {
    unsigned result = type.kind == Type::Kind::Union ? 0 : 3;
    if (type.parts.empty()) return 0;
    for (const auto& part : type.parts) {
      const auto domain = numeric_bound_domain(part, depth + 1);
      if (!domain) return 0;
      result = type.kind == Type::Kind::Union ? result | domain : result & domain;
    }
    return result;
  }
  return 0;
}

[[nodiscard]] unsigned numeric_domain(const Type& type) {
  if (type == kInt) return 1;
  if (type == kFloat) return 2;
  if (type.kind == Type::Kind::Parameter) return numeric_bound_domain(type);
  if (type.kind == Type::Kind::Union || type.kind == Type::Kind::Intersect) return numeric_bound_domain(type);
  return 0;
}

[[nodiscard]] bool is_numeric(const Type type) {
  return type == kInt || type == kFloat || type == kAny;
}

[[nodiscard]] bool derives_from(const std::uint16_t source, const std::uint16_t destination) {
  if (source == destination) {
    return true;
  }
  if (active_structs == nullptr || source >= active_structs->size()) {
    return false;
  }
  const parser::StructDecl& decl = (*active_structs)[source];
  if (!decl.base_indices.empty()) {
    for (const std::uint16_t base : decl.base_indices) {
      if (derives_from(base, destination)) {
        return true;
      }
    }
    return false;
  }
  const std::uint16_t base = decl.base_index;
  if (base == 65535 || base == source) {
    return false;
  }
  return derives_from(base, destination);
}

// The VM addresses fields and virtual methods by slots from the static base.
// A base view is valid only when those slots have identical meaning in the child.
[[nodiscard]] bool nominal_layout_compatible(const std::uint16_t source, const std::uint16_t destination) {
  if (source == destination) return true;
  if (active_structs == nullptr || source >= active_structs->size() || destination >= active_structs->size()) return false;
  const auto& child = (*active_structs)[source];
  const auto& base = (*active_structs)[destination];
  if (child.fields.size() < base.fields.size() || child.method_names.size() < base.method_names.size()) return false;
  for (std::size_t index = 0; index < base.fields.size(); ++index) {
    if (child.fields[index].name != base.fields[index].name || child.fields[index].owner != base.fields[index].owner) return false;
  }
  for (std::size_t index = 0; index < base.method_names.size(); ++index) {
    if (!base.method_names[index].empty() && child.method_names[index] != base.method_names[index]) return false;
  }
  return true;
}

[[nodiscard]] bool assignable(const Type destination, const Type source, const unsigned depth = 0) {
  if (depth > 32) return false;
  const auto accepts = [&](const Type& target, const Type& value) { return assignable(target, value, depth + 1); };
  if (destination == kAny || source == kAny || destination == source) {
    return true;
  }
  if (destination.kind == Type::Kind::Function && source.kind == Type::Kind::Function) {
    if (destination.parts.size() != source.parts.size()) return false;
    for (std::size_t i = 0; i < destination.parts.size(); ++i)
      if (!accepts(destination.parts[i], source.parts[i]) || !accepts(source.parts[i], destination.parts[i])) return false;
    return true;
  }
  if (destination.kind == Type::Kind::Tuple && source.kind == Type::Kind::Tuple) {
    if (destination.parts.size() != source.parts.size()) return false;
    for (std::size_t i = 0; i < destination.parts.size(); ++i)
      if (!accepts(destination.parts[i], source.parts[i]) || !accepts(source.parts[i], destination.parts[i])) return false;
    return true;
  }
  if (destination.kind == Type::Kind::Signal && source.kind == Type::Kind::Signal) {
    if (destination.parts.size() != source.parts.size()) return false;
    for (std::size_t index = 0; index < destination.parts.size(); ++index)
      if (!accepts(destination.parts[index], source.parts[index]) || !accepts(source.parts[index], destination.parts[index])) return false;
    return true;
  }
  // A union source can hold any alternative: every alternative must be accepted
  // by the destination. Check this before choosing a destination union branch,
  // because different source alternatives may fit different destination branches.
  if (source.kind == Type::Kind::Union) {
    return !source.parts.empty() && std::all_of(source.parts.begin(), source.parts.end(),
        [&](const Type& part) { return accepts(destination, part); });
  }
  if (destination.kind == Type::Kind::Struct && source.kind == Type::Kind::Struct) {
    if (destination.index == source.index) {
      if (destination.parts.size() != source.parts.size()) return false;
      for (std::size_t index = 0; index < destination.parts.size(); ++index) {
        // Mutable containers and data types are invariant. Mutual compatibility
        // permits equivalent aliases/unions but prevents writable int -> float
        // or derived -> base views from changing the accepted element type.
        if (!accepts(destination.parts[index], source.parts[index]) ||
            !accepts(source.parts[index], destination.parts[index])) return false;
      }
      return true;
    }
    if (!nominal_layout_compatible(source.index, destination.index)) return false;
    const Type viewed = base_view(source, destination.index);
    if (viewed != kError) return accepts(destination, viewed);
    return destination.parts.empty() && source.parts.empty() && derives_from(source.index, destination.index);
  }
  if (destination.kind == Type::Kind::Optional && destination.parts.size() == 1) {
    if (source == kNull) return true;
    if (source.kind == Type::Kind::Optional && source.parts.size() == 1) {
      return accepts(destination.parts[0], source.parts[0]);
    }
    return accepts(destination.parts[0], source);
  }
  if (destination.kind == Type::Kind::Union) {
    for (const Type& part : destination.parts) {
      if (accepts(part, source)) {
        return true;
      }
    }
    return false;
  }
  if (destination.kind == Type::Kind::Intersect) {
    for (const Type& part : destination.parts) {
      if (!accepts(part, source)) {
        return false;
      }
    }
    return !destination.parts.empty();
  }
  if (source.kind == Type::Kind::Intersect) {
    return std::any_of(source.parts.begin(), source.parts.end(), [&](const Type& part) { return accepts(destination, part); });
  }
  if (destination == kString && source.kind == Type::Kind::Literal) {
    return true;
  }
  if (destination.kind == Type::Kind::Task && source.kind == Type::Kind::Task && destination.payload == Type::Kind::Any) {
    return true;
  }
  if (destination.kind == Type::Kind::Enum && destination.kind == source.kind &&
      destination.index == source.index && destination.parts.size() == source.parts.size() && !destination.parts.empty()) {
    for (std::size_t index = 0; index < destination.parts.size(); ++index) {
      if (!accepts(destination.parts[index], source.parts[index])) {
        return false;
      }
    }
    return true;
  }
  if (source.kind == Type::Kind::Parameter) {
    const Type bound = parameter_bound(source);
    return bound != kError && accepts(destination, bound);
  }
  return destination == kFloat && source == kInt;
}

[[nodiscard]] Type intersection_view(const Type& type, const unsigned depth) {
  for (const auto& part : type.parts) {
    const auto candidate = member_view(part, depth + 1);
    if (candidate.kind == Type::Kind::Struct &&
        std::all_of(type.parts.begin(), type.parts.end(), [&](const Type& requirement) { return assignable(requirement, candidate); })) return candidate;
  }
  return kError;
}

[[nodiscard]] bool accepts_value(Type destination, const Type source, const parser::Expr& expression) {
  if (!assignable(destination, source)) return false;
  const bool optional = destination.kind == Type::Kind::Optional;
  while (destination.kind == Type::Kind::Optional && destination.parts.size() == 1) {
    Type inner = destination.parts[0];
    destination = std::move(inner);
  }
  if (destination == kFloat) const_cast<parser::Expr&>(expression).numeric_promotion = optional ? 2 : 1;
  return true;
}

[[nodiscard]] bool infer_arguments(const Type& pattern, const Type& actual, const TypeScope& scope,
                                   std::vector<Type>& inferred, const bool widen_literal, const std::size_t fixed_prefix) {
  if (pattern.kind == Type::Kind::Parameter && pattern.literal == scope.name) {
    if (pattern.index < fixed_prefix) return true;
    if (pattern.index >= inferred.size() || unresolved_type(actual) || actual == kVoid || actual == kNull) return false;
    const Type candidate = widen_literal && actual.kind == Type::Kind::Literal ? kString : actual;
    Type& previous = inferred[pattern.index];
    if (previous == kError) { previous = candidate; return true; }
    return assignable(previous, candidate) && assignable(candidate, previous);
  }
  if (!contains_parameter(pattern)) return true;  // Concrete compatibility is checked after substitution.
  if (pattern.kind == Type::Kind::Optional && pattern.parts.size() == 1) {
    if (actual == kNull) return true;  // Absence contributes no type evidence.
    return infer_arguments(pattern.parts[0], actual.kind == Type::Kind::Optional && actual.parts.size() == 1 ? actual.parts[0] : actual,
                           scope, inferred, widen_literal, fixed_prefix);
  }
  if (pattern.kind == Type::Kind::Union) {
    // Every possible source alternative must fit. Concrete alternatives supply
    // no evidence: `T | string` receiving string must not guess T=string.
    const Type evidence = actual.kind == Type::Kind::Parameter ? parameter_bound(actual) : actual;
    const auto infer_alternative = [&](const Type& alternative, std::vector<Type>& arguments) {
      for (const auto& branch : pattern.parts) {
        if (!contains_parameter(branch) && assignable(branch, alternative)) return true;
      }
      std::vector<std::vector<Type>> candidates;
      for (const auto& branch : pattern.parts) {
        if (!contains_parameter(branch)) continue;
        auto candidate = arguments;
        if (!infer_arguments(branch, alternative, scope, candidate, widen_literal, fixed_prefix)) continue;
        const Type specialized = specialize_type(branch, scope, candidate);
        if (specialized == kError || !assignable(specialized, alternative)) continue;
        if (std::find(candidates.begin(), candidates.end(), candidate) == candidates.end()) {
          candidates.push_back(std::move(candidate));
        }
      }
      if (candidates.size() != 1) return false;
      arguments = std::move(candidates.front());
      return true;
    };
    auto candidate = inferred;
    if (evidence.kind == Type::Kind::Union) {
      if (evidence.parts.empty()) return false;
      for (const auto& alternative : evidence.parts) {
        if (!infer_alternative(alternative, candidate)) return false;
      }
    } else if (!infer_alternative(evidence, candidate)) return false;
    inferred = std::move(candidate);
    return true;
  }
  if (pattern.kind == Type::Kind::Intersect) {
    auto candidate = inferred;
    if (pattern.parts.empty()) return false;
    for (const auto& branch : pattern.parts) {
      if (!contains_parameter(branch)) {
        if (!assignable(branch, actual)) return false;
      } else if (!infer_arguments(branch, actual, scope, candidate, widen_literal, fixed_prefix)) return false;
    }
    inferred = std::move(candidate);
    return true;
  }
  Type evidence = actual.kind == Type::Kind::Parameter ? parameter_bound(actual) : actual;
  if (pattern.kind == Type::Kind::Struct && evidence.kind == Type::Kind::Intersect) {
    bool matched = false;
    for (const auto& part : evidence.parts) {
      auto candidate = inferred;
      if (!infer_arguments(pattern, part, scope, candidate, widen_literal, fixed_prefix)) continue;
      if (matched && candidate != inferred) return false;
      inferred = std::move(candidate); matched = true;
    }
    return matched;
  }
  evidence = member_view(evidence);
  if (pattern.kind == Type::Kind::Struct && evidence.kind == Type::Kind::Struct && pattern.index != evidence.index) {
    evidence = base_view(evidence, pattern.index);
  }
  if (pattern.kind != evidence.kind || pattern.index != evidence.index || pattern.parts.size() != evidence.parts.size()) return false;
  for (std::size_t part = 0; part < pattern.parts.size(); ++part) {
    if (!infer_arguments(pattern.parts[part], evidence.parts[part], scope, inferred, false, fixed_prefix)) return false;
  }
  return true;
}

using InferenceEvidence = std::pair<const Type*, const Type*>;

[[nodiscard]] bool infer_evidence(std::vector<InferenceEvidence> pending, const TypeScope& scope,
                                 std::vector<Type>& inferred, const std::size_t fixed_prefix) {
  // Resolve available evidence before retrying an ambiguous composed argument.
  // Failed candidates are transactional and cannot pollute another argument.
  while (!pending.empty()) {
    std::vector<InferenceEvidence> remaining;
    for (const auto& [pattern, actual] : pending) {
      auto candidate = inferred;
      if (infer_arguments(*pattern, *actual, scope, candidate, true, fixed_prefix)) inferred = std::move(candidate);
      else remaining.emplace_back(pattern, actual);
    }
    if (remaining.size() == pending.size()) return false;
    pending = std::move(remaining);
  }
  return true;
}

[[nodiscard]] bool is_text(const Type type) {
  return type == kString || type == kAny || type.kind == Type::Kind::Literal || type.kind == Type::Kind::Union;
}

[[nodiscard]] bool is_string_value(const Type type) {
  return type == kString || type == kAny || type.kind == Type::Kind::Literal;
}

[[nodiscard]] Type iteration_element(const Type& source) {
  if (numeric_domain(source) == 1) return kInt;
  const Type view = member_view(source);
  if (view == kAny) return kAny;
  if (is_string_value(view)) return kString;
  if (view.kind == Type::Kind::Struct && (view.index == 65532 || view.index == 65531) && !view.parts.empty()) return view.parts[0];
  return kError;
}

[[nodiscard]] bool contains_literal(const Type type, const std::string_view literal) {
  if (type.kind == Type::Kind::Literal) {
    return type.literal == literal;
  }
  if (type.kind == Type::Kind::Union) {
    for (const Type& part : type.parts) {
      if (contains_literal(part, literal)) {
        return true;
      }
    }
  }
  return false;
}

[[nodiscard]] Type find_binding(const std::vector<Binding>& bindings, const std::string_view name, bool global = false) {
  for (auto binding = bindings.rbegin(); binding != bindings.rend(); ++binding) {
    if (binding->name == name && (!global || binding->global)) {
      return binding->type;
    }
  }
  return kError;
}

[[nodiscard]] const Signature* find_signature(const std::vector<Signature>& signatures, const std::string_view name) {
  for (const Signature& signature : signatures) {
    if (signature.name == name) {
      return &signature;
    }
  }
  return nullptr;
}

void mismatch(const parser::Expr& expr, std::vector<Diagnostic>& diagnostics) {
  diagnostics.push_back(Diagnostic{expr.location, expr.span, "type mismatch"});
}

[[nodiscard]] std::string source_type(const Type& type, const parser::Program& program);

[[nodiscard]] std::string diagnostic_type(const Type& type) {
  if (type == kAny) return "any";
  if (type == kNull) return "null";
  return active_program ? source_type(type, *active_program) : std::string{};
}

void mismatch(const parser::Expr& expr, std::vector<Diagnostic>& diagnostics, const Type& expected, const Type& actual) {
  mismatch(expr, diagnostics);
  diagnostics.back().expected_type = diagnostic_type(expected);
  diagnostics.back().actual_type = diagnostic_type(actual);
}

[[nodiscard]] bool method_signature_compatible(const Signature& base, const Signature& implementation,
                                                const std::vector<StructInfo>& structs, const Type& receiver) {
  if (base.params.size() != implementation.params.size() || base.is_async != implementation.is_async) return false;
  const auto expected_count = base.type_params.size() - base.owner_type_param_count;
  const auto actual_count = implementation.type_params.size() - implementation.owner_type_param_count;
  if (expected_count != actual_count) return false;
  TypeScope expected_scope, actual_scope;
  expected_scope.name = "$function:" + base.name; expected_scope.type_params = base.type_params;
  actual_scope.name = "$function:" + implementation.name; actual_scope.type_params = implementation.type_params;
  const auto arguments = [&](const Signature& signature) {
    std::vector<Type> result;
    if (signature.owner_type_param_count) {
      if (signature.params.empty()) return result;
      const Type owner = base_view(receiver, signature.params[0].index);
      if (owner == kError) return result;
      result = owner.parts;
      if (result.size() != signature.owner_type_param_count) return std::vector<Type>{};
    }
    for (std::size_t index = 0; index < expected_count; ++index) {
      Type parameter{Type::Kind::Parameter, static_cast<std::uint16_t>(index)};
      parameter.literal = "$override"; result.push_back(std::move(parameter));
    }
    return result;
  };
  const auto expected_args = arguments(base), actual_args = arguments(implementation);
  if (expected_args.size() != base.type_params.size() || actual_args.size() != implementation.type_params.size()) return false;
  const auto bounds = [&](const Signature& signature, const TypeScope& scope, const std::vector<Type>& args) {
    std::vector<Type> result(expected_count, kError);
    for (std::size_t index = 0; index < signature.where_names.size() && index < signature.where_types.size(); ++index) {
      const auto parameter = std::find(signature.type_params.begin(), signature.type_params.end(), signature.where_names[index]);
      const auto slot = static_cast<std::size_t>(parameter - signature.type_params.begin());
      if (parameter != signature.type_params.end() && slot >= signature.owner_type_param_count) result[slot - signature.owner_type_param_count] =
          specialize_type(named_type(signature.where_types[index], structs, 0, &scope), scope, args);
    }
    return result;
  };
  if (bounds(base, expected_scope, expected_args) != bounds(implementation, actual_scope, actual_args)) return false;
  const Type expected_result = specialize_type(base.result, expected_scope, expected_args);
  const Type actual_result = specialize_type(implementation.result, actual_scope, actual_args);
  const bool generic = !base.type_params.empty() || !implementation.type_params.empty();
  const auto same_type = [](const Type& first, const Type& second) {
    if (unresolved_type(first) || unresolved_type(second)) return first == second;
    return assignable(first, second) && assignable(second, first);
  };
  if (generic ? !same_type(expected_result, actual_result) :
      ((!unresolved_type(expected_result) && unresolved_type(actual_result)) || !assignable(expected_result, actual_result))) return false;
  for (std::size_t index = 1; index < base.params.size(); ++index) {
    const Type expected = specialize_type(base.params[index], expected_scope, expected_args);
    const Type actual = specialize_type(implementation.params[index], actual_scope, actual_args);
    if (generic ? !same_type(expected, actual) : !assignable(actual, expected)) return false;
  }
  return true;
}

[[nodiscard]] bool bound_layout_compatible(Type actual, Type required, const std::vector<Signature>& signatures, const std::vector<StructInfo>& structs) {
  if (required.kind == Type::Kind::Intersect) {
    return std::all_of(required.parts.begin(), required.parts.end(), [&](const auto& part) { return bound_layout_compatible(actual, part, signatures, structs); });
  }
  if (required.kind == Type::Kind::Union) {
    return std::any_of(required.parts.begin(), required.parts.end(), [&](const auto& part) { return assignable(part, actual) && bound_layout_compatible(actual, part, signatures, structs); });
  }
  actual = member_view(actual); required = member_view(required);
  if (actual.kind != Type::Kind::Struct || required.kind != Type::Kind::Struct || actual.index == required.index) return true;
  if (active_structs == nullptr || actual.index >= active_structs->size() || required.index >= active_structs->size()) return false;
  const auto& concrete = (*active_structs)[actual.index];
  const auto& base = (*active_structs)[required.index];
  if (concrete.fields.size() < base.fields.size() || concrete.method_names.size() < base.method_names.size()) return false;
  for (std::size_t index = 0; index < base.fields.size(); ++index) {
    if (concrete.fields[index].name != base.fields[index].name || concrete.fields[index].owner != base.fields[index].owner) return false;
  }
  for (std::size_t index = 0; index < base.method_names.size(); ++index) {
    if (base.method_names[index].empty()) continue;
    if (concrete.method_names[index] != base.method_names[index] || index >= concrete.vtable.size() || index >= base.vtable.size()) return false;
    const Signature* expected = nullptr;
    const Signature* implementation = nullptr;
    for (const auto& signature : signatures) {
      if (signature.index == base.vtable[index]) expected = &signature;
      if (signature.index == concrete.vtable[index]) implementation = &signature;
    }
    if (expected == nullptr || implementation == nullptr || !method_signature_compatible(*expected, *implementation, structs, actual)) return false;
  }
  return true;
}

Type type_test_target(const parser::Expr& expr) {
  return active_type_info ? named_type(expr.value, *active_type_info) : kError;
}

// Runtime tags identify nominal definitions; erased generic arguments cannot safely refine a value.
bool runtime_type_tags(const Type& type, const std::vector<StructInfo>& structs, std::vector<std::uint16_t>& tags) {
  switch (type.kind) {
    case Type::Kind::Any: tags.push_back(0); return true;
    case Type::Kind::Int: tags.push_back(1); return true;
    case Type::Kind::Float: tags.push_back(2); return true;
    case Type::Kind::Bool: tags.push_back(3); return true;
    case Type::Kind::String: tags.push_back(4); return true;
    case Type::Kind::Null: tags.push_back(5); return true;
    case Type::Kind::Vector3: tags.push_back(6); return true;
    case Type::Kind::Buffer: tags.push_back(7); return true;
    case Type::Kind::Signal: return false;  // Payload signatures are erased at runtime.
    case Type::Kind::Connection: tags.push_back(9); return true;
    case Type::Kind::Optional:
      tags.push_back(5);
      return type.parts.size() == 1 && runtime_type_tags(type.parts[0], structs, tags);
    case Type::Kind::Union:
      for (const auto& part : type.parts) if (!runtime_type_tags(part, structs, tags)) return false;
      return !type.parts.empty();
    case Type::Kind::Struct:
      if (!type.parts.empty() || type.index >= structs.size() || type.index >= 65520) return false;
      for (std::size_t index = 0; index < structs.size() && index < 65520; ++index) {
        const Type candidate{Type::Kind::Struct, static_cast<std::uint16_t>(index)};
        if (index == type.index || base_view(candidate, type.index).kind != Type::Kind::Error)
          tags.push_back(static_cast<std::uint16_t>(index + 16));
      }
      return true;
    default: return false;
  }
}

[[nodiscard]] Type check_expr(const parser::Expr& expr, const std::vector<Binding>& bindings,
                              const std::vector<Signature>& signatures, const std::vector<StructInfo>& structs,
                              std::vector<Diagnostic>& diagnostics);

void check_function_constraints(const parser::Expr& expr, const Signature& signature,
                                const TypeScope& context, const std::vector<Type>& arguments,
                                const std::vector<Signature>& signatures, const std::vector<StructInfo>& structs,
                                std::vector<Diagnostic>& diagnostics) {
  for (std::size_t constraint = 0; constraint < signature.where_names.size() && constraint < signature.where_types.size(); ++constraint) {
    const auto param = std::find(signature.type_params.begin(), signature.type_params.end(), signature.where_names[constraint]);
    if (param == signature.type_params.end()) continue;
    const Type required = specialize_type(named_type(signature.where_types[constraint], structs, 0, &context), context, arguments);
    const Type actual = arguments[param - signature.type_params.begin()];
    if (required == kError || unresolved_type(actual) || !assignable(required, actual)) mismatch(expr, diagnostics);
    else if (!bound_layout_compatible(actual, required, signatures, structs)) {
      diagnostics.push_back(Diagnostic{expr.location, expr.span, "generic constraint requires compatible member layout and method signatures"});
    }
  }
}


[[nodiscard]] Type check_numeric_binary(const parser::Expr& expr, const Type left, const Type right,
                                        const bool allow_vector, std::vector<Diagnostic>& diagnostics) {
  if (allow_vector && left == kVector3 && right == kVector3) {
    return kVector3;
  }
  if (allow_vector && ((left == kVector3 && (is_numeric(right) || numeric_domain(right))) || ((is_numeric(left) || numeric_domain(left)) && right == kVector3))) {
    return kVector3;
  }
  const auto left_domain = numeric_domain(left), right_domain = numeric_domain(right);
  if (left_domain && right_domain) {
    // Same actual type, or an integer operand, keeps the formal's numeric category.
    if (left.kind == Type::Kind::Parameter && (left == right || right_domain == 1)) return left;
    if (right.kind == Type::Kind::Parameter && left_domain == 1) return right;
    if (left_domain == 1 && right_domain == 1) return kInt;
    if (left_domain == 2 || right_domain == 2) return kFloat;
    Type result; result.kind = Type::Kind::Union; result.parts = {kInt, kFloat}; return result;
  }
  if ((left == kAny || right == kAny) && is_numeric(left) && is_numeric(right)) return kAny;
  mismatch(expr, diagnostics);
  return kError;
}

[[nodiscard]] Type check_expr_impl(const parser::Expr& expr, const std::vector<Binding>& bindings,
                              const std::vector<Signature>& signatures, const std::vector<StructInfo>& structs,
                              std::vector<Diagnostic>& diagnostics);

[[nodiscard]] Type check_expr(const parser::Expr& expr, const std::vector<Binding>& bindings,
                              const std::vector<Signature>& signatures, const std::vector<StructInfo>& structs,
                              std::vector<Diagnostic>& diagnostics) {
  if (expr.default_scope.empty()) return check_expr_impl(expr, bindings, signatures, structs, diagnostics);
  const Type instantiated = named_type(expr.default_scope, structs);
  if (instantiated.kind != Type::Kind::Struct || instantiated.index >= structs.size()) { mismatch(expr, diagnostics); return kError; }
  const TypeScope context = structs[instantiated.index];
  struct Restore { const TypeScope* previous; ~Restore() { active_function_types = previous; } } restore{active_function_types};
  active_function_types = &context;
  return specialize_type(check_expr_impl(expr, bindings, signatures, structs, diagnostics), context, instantiated.parts);
}

[[nodiscard]] Type check_expr_impl(const parser::Expr& expr, const std::vector<Binding>& bindings,
                              const std::vector<Signature>& signatures, const std::vector<StructInfo>& structs,
                              std::vector<Diagnostic>& diagnostics) {
  switch (expr.kind) {
    case parser::Expr::Kind::Event: {
      if (expr.runtime_op == 0) {
        const auto type = named_type(expr.value, structs);
        if (type.kind != Type::Kind::Signal || !expr.args.empty()) { mismatch(expr, diagnostics); return kError; }
        return type;
      }
      std::vector<Type> args;
      for (const auto& arg : expr.args) args.push_back(check_expr(arg, bindings, signatures, structs, diagnostics));
      if (args.empty()) { mismatch(expr, diagnostics); return kError; }
      args[0] = member_view(args[0]);
      if (expr.runtime_op == 1 || expr.runtime_op == 5 || expr.runtime_op == 9) {
        if (args.size() != 2 || args[0].kind != Type::Kind::Signal) { mismatch(expr, diagnostics); return kError; }
        Type expected{Type::Kind::Function, 0}; expected.parts = args[0].parts; expected.parts.push_back(kVoid);
        if (!assignable(expected, args[1])) mismatch(expr.args[1], diagnostics);
        return Type{Type::Kind::Connection, 0};
      }
      if (expr.runtime_op == 2) {
        if (args[0].kind != Type::Kind::Signal || args.size() != args[0].parts.size() + 1) { mismatch(expr, diagnostics); return kError; }
        for (std::size_t index = 1; index < args.size(); ++index)
          if (!accepts_value(args[0].parts[index - 1], args[index], expr.args[index])) mismatch(expr.args[index], diagnostics);
        return kVoid;
      }
      if (expr.runtime_op == 3 || expr.runtime_op == 4) {
        if (args.size() != 1 || args[0].kind != Type::Kind::Connection) mismatch(expr, diagnostics);
        return expr.runtime_op == 3 ? kVoid : kBool;
      }
      if (expr.runtime_op == 10) {
        if (args.size() != 1 || args[0].kind != Type::Kind::Signal) mismatch(expr, diagnostics);
        return kVoid;
      }
      if (expr.runtime_op == 11) {
        if (args.size() != 1 || args[0].kind != Type::Kind::Signal) { mismatch(expr, diagnostics); return kError; }
        if (!active_function_types || !active_function_types->is_async) {
          diagnostics.push_back(Diagnostic{expr.location, expr.span, "event Wait requires an async function"}); return kError;
        }
        if (args[0].parts.empty()) return kVoid;
        if (args[0].parts.size() == 1) return args[0].parts[0];
        Type payload{Type::Kind::Tuple, 0}; payload.parts = args[0].parts; return payload;
      }
      mismatch(expr, diagnostics); return kError;
    }
    case parser::Expr::Kind::Ascribe: {
      const Type target = named_type(expr.value, structs);
      const Type source = expr.left ? check_expr(*expr.left, bindings, signatures, structs, diagnostics) : kError;
      if (source == kError) return kError;
      if (target == kError || target == kVoid || (source == kAny && target != kAny) ||
          !expr.left || !accepts_value(target, source, *expr.left)) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "value does not satisfy explicit expression type"});
        return kError;
      }
      return target;
    }
    case parser::Expr::Kind::TypeTest: {
      if (expr.left) (void)check_expr(*expr.left, bindings, signatures, structs, diagnostics);
      const Type target = named_type(expr.value, structs);
      auto& tags = const_cast<parser::Expr&>(expr).targets;
      tags.clear();
      if (target == kError || target == kVoid) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "unknown or invalid type in :is test"});
        return kError;
      }
      if (!runtime_type_tags(target, structs, tags)) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "type has no preserved runtime identity for :is test"});
        return kError;
      }
      return kBool;
    }
    case parser::Expr::Kind::Cast: {
      const Type target = named_type(expr.value, structs);
      const Type source = expr.left ? check_expr(*expr.left, bindings, signatures, structs, diagnostics) : kError;
      if ((target != kInt && target != kFloat) || !numeric_domain(source)) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "numeric cast requires number or float types"});
        return kError;
      }
      const_cast<parser::Expr&>(expr).runtime_op = target == kInt ? 0 : 1;
      return target;
    }
    case parser::Expr::Kind::String: {
      Type literal;
      literal.kind = Type::Kind::Literal;
      literal.literal = expr.value;
      return literal;
    }
    case parser::Expr::Kind::Number:
      if (expr.null_literal) {
        return kNull;
      }
      if (expr.bool_literal) {
        return kBool;
      }
      if (expr.enum_literal) {
        return Type{Type::Kind::Enum, expr.slot};
      }
      return expr.float_literal ? kFloat : kInt;
    case parser::Expr::Kind::SelfField:
      return kAny;
    case parser::Expr::Kind::Observe: {
      Type expected{Type::Kind::Function, 0};
      expected.parts = {named_type(expr.value, structs), kVoid};
      const auto callback = expr.args.empty() ? kError : check_expr(expr.args[0], bindings, signatures, structs, diagnostics);
      if (!assignable(expected, callback)) mismatch(expr, diagnostics);
      return Type{Type::Kind::Connection, 0};
    }
    case parser::Expr::Kind::Closure: {
      const auto open = expr.value.find('<');
      const auto name = expr.value.substr(0, open);
      const Signature* signature = find_signature(signatures, name);
      const bool bound_method = expr.virtual_call && expr.left;
      if (signature == nullptr || (!bound_method && signature->owner_type_param_count != 0)) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "callback requires a function without an implicit receiver"});
        return kError;
      }
      TypeScope context; context.name = "$function:" + signature->name; context.type_params = signature->type_params;
      std::vector<Type> arguments;
      const std::size_t owner_count = bound_method ? signature->owner_type_param_count : 0;
      if (bound_method) {
        if (signature->params.empty()) { mismatch(expr, diagnostics); return kError; }
        const Type receiver = check_expr(*expr.left, bindings, signatures, structs, diagnostics);
        arguments.assign(owner_count, kError);
        if (owner_count) {
          std::vector<Type> owner_arguments(signature->type_params.size(), kError);
          if (!infer_arguments(signature->params[0], receiver, context, owner_arguments)) { mismatch(expr, diagnostics); return kError; }
          arguments.assign(owner_arguments.begin(), owner_arguments.begin() + static_cast<std::ptrdiff_t>(owner_count));
        }
        const Type expected = specialize_type(signature->params[0], context, arguments);
        if (!assignable(expected, receiver)) { mismatch(expr, diagnostics); return kError; }
      }
      if (open != std::string::npos && expr.value.back() == '>') {
        const auto supplied = std::string_view(expr.value).substr(open + 1, expr.value.size() - open - 2);
        for (const auto piece : split_type(supplied, ',')) {
          const auto type = named_type(piece, structs);
          if (type == kError || unresolved_type(type) || !scoped_callback_type(type)) {
            diagnostics.push_back(Diagnostic{expr.location, expr.span, "callback type arguments must be known types in the current generic scope"});
            return kError;
          }
          arguments.push_back(type);
        }
      }
      if (arguments.size() != signature->type_params.size()) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "wrong number of function type arguments"});
        return kError;
      }
      check_function_constraints(expr, *signature, context, arguments, signatures, structs, diagnostics);
      Type callable{Type::Kind::Function, 0};
      for (std::size_t index = bound_method ? 1 : 0; index < signature->params.size(); ++index)
        callable.parts.push_back(specialize_type(signature->params[index], context, arguments));
      const Type result = specialize_type(signature->result, context, arguments);
      callable.parts.push_back(signature->is_async ? task_of(result) : result);
      if (std::any_of(callable.parts.begin(), callable.parts.end(), [&](const Type& type) { return type == kError || !scoped_callback_type(type); })) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "callback signature contains an unresolved generic scope"}); return kError;
      }
      return callable;
    }
    case parser::Expr::Kind::Invoke: {
      const Type callable = expr.left ? check_expr(*expr.left, bindings, signatures, structs, diagnostics) : kError;
      if (callable.kind != Type::Kind::Function || callable.parts.size() != expr.args.size() + 1) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "callback signature or argument count mismatch"});
        return kError;
      }
      if (expr.thread_call && callable.parts.back().kind == Type::Kind::Task) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "use spawn for a task-returning callback"});
        return kError;
      }
      for (std::size_t i = 0; i < expr.args.size(); ++i) {
        const auto actual = check_expr(expr.args[i], bindings, signatures, structs, diagnostics);
        if (!accepts_value(callable.parts[i], actual, expr.args[i])) mismatch(expr.args[i], diagnostics);
        if (callable.parts[i] == kFloat && actual == kInt) const_cast<parser::Expr&>(expr.args[i]).numeric_promotion = 1;
      }
      if (!expr.thread_call && callable.parts.back() == kFloat) const_cast<parser::Expr&>(expr).numeric_promotion = 1;
      return expr.thread_call ? kInt : callable.parts.back();
    }
    case parser::Expr::Kind::Name: {
      const Type type = find_binding(bindings, expr.value, expr.global);
      if (type == kError) {
        mismatch(expr, diagnostics);
      }
      return type;
    }
    case parser::Expr::Kind::Member: {
      if (expr.missing) {
        if (expr.left != nullptr) {
          (void)check_expr(*expr.left, bindings, signatures, structs, diagnostics);
        }
        return kError;
      }
      const Type source = expr.left == nullptr ? kError : check_expr(*expr.left, bindings, signatures, structs, diagnostics);
      const Type object = source.kind == Type::Kind::Parameter || source.kind == Type::Kind::Intersect
          ? named_member_view(source, expr.value) : member_view(source);
      if (object.kind == Type::Kind::Struct) {
        if (object.index >= structs.size() || expr.slot >= structs[object.index].fields.size()) {
          mismatch(expr, diagnostics);
          return kError;
        }
        return specialize_type(structs[object.index].fields[expr.slot], structs[object.index], object.parts);
      }
      if (object != kVector3 && object != kAny) {
        mismatch(expr, diagnostics);
        return kError;
      }
      return kFloat;
    }
    case parser::Expr::Kind::MoveFrom: {
      const Type type = find_binding(bindings, expr.value, expr.global);
      if (type == kError) {
        mismatch(expr, diagnostics);
        return kError;
      }
      return type;
    }
    case parser::Expr::Kind::Construct: {
      if (expr.slot >= structs.size()) {
        mismatch(expr, diagnostics);
        return kError;
      }
      const std::vector<Type>& fields = structs[expr.slot].fields;
      const Type specialization = named_type(expr.value, structs);
      if (specialization.kind != Type::Kind::Struct || specialization.index != expr.slot) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "invalid data type arguments"});
        for (const auto& argument : expr.args) (void)check_expr(argument, bindings, signatures, structs, diagnostics);
        return kError;
      }
      std::vector<Type> supplied = specialization.parts;
      for (std::size_t index = 0; index < expr.args.size() && index < fields.size(); ++index) {
        const Type arg = check_expr(expr.args[index], bindings, signatures, structs, diagnostics);
        const Type expected = specialize_type(fields[index], structs[expr.slot], supplied);
        if (expected == kError || !accepts_value(expected, arg, expr.args[index])) {
          mismatch(expr.args[index], diagnostics, expected, arg);
        }
      }
      Type result{Type::Kind::Struct, expr.slot};
      result.parts = std::move(supplied);
      return result;
    }
    case parser::Expr::Kind::Native: {
      std::vector<Type> args;
      for (const parser::Expr& arg : expr.args) {
        args.push_back(check_expr(arg, bindings, signatures, structs, diagnostics));
      }
      const auto numeric_arg = [&](const std::size_t index) {
        return index < args.size() && is_numeric(args[index]);
      };
      if (expr.slot == 0 || expr.slot == 7 || expr.slot == 8) {
        const std::size_t count = expr.slot == 7 ? 2 : expr.slot == 8 ? 4 : 3;
        for (std::size_t index = 0; index < count; ++index) {
          if (!numeric_arg(index)) {
            mismatch(expr, diagnostics);
            break;
          }
        }
        return kVector3;
      }
      if (expr.slot == 1) {
        if (!numeric_arg(0)) {
          mismatch(expr, diagnostics);
        }
        return kBuffer;
      }
      if (expr.slot == 2) {
        if (args.size() < 3 || (args[0] != kBuffer && args[0] != kAny) || !numeric_arg(1) ||
            !is_string_value(args[2])) {
          mismatch(expr, diagnostics);
        }
        return kVoid;
      }
      if (expr.slot == 3) {
        if (args.empty() || (args[0] != kBuffer && args[0] != kAny)) {
          mismatch(expr, diagnostics);
        }
        return kInt;
      }
      if (expr.slot == 4) {
        if (args.empty() || !is_string_value(args[0])) {
          mismatch(expr, diagnostics);
        }
        return kInt;
      }
      if (expr.slot == 5 || expr.slot == 6 || expr.slot == 9 || expr.slot == 10 || expr.slot == 11 || expr.slot == 13) {
        if (args.empty() || !is_string_value(args[0])) {
          mismatch(expr, diagnostics);
        }
        return kString;
      }
      if (expr.slot == 12 || expr.slot == 18 || expr.slot == 19) {
        if (args.size() < 2 || !is_string_value(args[0]) || !is_string_value(args[1])) {
          mismatch(expr, diagnostics);
        }
        return kBool;  // contains / starts_with / ends_with
      }
      if (expr.slot == 16) {  // split -> list of strings
        if (args.size() < 2 || !is_string_value(args[0]) || !is_string_value(args[1])) {
          mismatch(expr, diagnostics);
        }
        return kAny;
      }
      if (expr.slot == 17 || expr.slot == 20 || expr.slot == 23) {  // replace, repeat, join -> string
        return kString;
      }
      if (expr.slot == 21) {  // to_number
        if (args.empty() || !is_string_value(args[0])) {
          mismatch(expr, diagnostics);
        }
        return kFloat;
      }
      if (expr.slot == 22) {  // index_of
        return kInt;
      }
      if (expr.slot == 14 || expr.slot == 15) {
        if (expr.slot == 14 && (args.empty() || !is_string_value(args[0]))) {
          mismatch(expr, diagnostics);
        }
        return kAny;
      }
      if (expr.slot >= 1000) {
        return kAny;
      }
      mismatch(expr, diagnostics);
      return kError;
    }
    case parser::Expr::Kind::Call: {
      const std::size_t open = expr.value.find('<');
      const std::string bare = open == std::string::npos ? expr.value : expr.value.substr(0, open);
      const Signature* signature = find_signature(signatures, bare);
      if (expr.runtime_op == 5) {
        for (const auto& candidate : signatures) if (candidate.index == expr.slot) { signature = &candidate; break; }
      }
      std::vector<Type> args;
      for (const parser::Expr& arg : expr.args) {
        args.push_back(check_expr(arg, bindings, signatures, structs, diagnostics));
      }
      if (signature == nullptr) {
        return kAny;
      }
      std::vector<Type> arguments;
      if (open != std::string::npos && expr.value.back() == '>') {
        const auto supplied = std::string_view(expr.value).substr(open + 1, expr.value.size() - open - 2);
        for (auto piece : split_type(supplied, ',')) {
          while (!piece.empty() && piece.front() == ' ') piece.remove_prefix(1);
          while (!piece.empty() && piece.back() == ' ') piece.remove_suffix(1);
          const Type type = piece.empty() ? kError : named_type(piece, structs);
          if (type == kError) {
            diagnostics.push_back(Diagnostic{expr.location, expr.span, "unknown function type argument"});
            return kError;
          }
          arguments.push_back(type);
        }
      }
      TypeScope context;
      context.name = "$function:" + signature->name;
      context.type_params = signature->type_params;
      const std::size_t owner_count = signature->owner_type_param_count;
      const auto supplied = std::move(arguments);
      arguments.assign(signature->type_params.size(), kError);
      bool resolved = true;
      if (owner_count > 0) {
        resolved = !args.empty() && !signature->params.empty() &&
            infer_arguments(signature->params[0], args[0], context, arguments);
        for (std::size_t index = 0; resolved && index < owner_count; ++index) resolved = arguments[index] != kError;
      }
      if (open != std::string::npos) {
        if (supplied.size() != signature->type_params.size() - owner_count) {
          diagnostics.push_back(Diagnostic{expr.location, expr.span, "wrong number of function type arguments"});
          return kError;
        }
        for (std::size_t index = 0; index < supplied.size(); ++index) arguments[owner_count + index] = supplied[index];
      } else if (!signature->type_params.empty()) {
        resolved = resolved && args.size() == signature->params.size();
        std::vector<InferenceEvidence> evidence;
        for (std::size_t index = owner_count ? 1 : 0; resolved && index < args.size(); ++index)
          evidence.emplace_back(&signature->params[index], &args[index]);
        resolved = resolved && infer_evidence(std::move(evidence), context, arguments, owner_count);
      }
      if (!resolved || std::any_of(arguments.begin(), arguments.end(), [](const Type& type) { return type == kError; })) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "cannot infer function type arguments; supply them explicitly"});
        return kError;
      }
      check_function_constraints(expr, *signature, context, arguments, signatures, structs, diagnostics);
      for (std::size_t index = 0; index < args.size() && index < signature->params.size(); ++index) {
        const Type expected = specialize_type(signature->params[index], context, arguments);
        if (expected == kError || !accepts_value(expected, args[index], expr.args[index])) mismatch(expr.args[index], diagnostics, expected, args[index]);
      }
      const Type result = specialize_type(signature->result, context, arguments);
      if (result == kError) mismatch(expr, diagnostics);
      if (expr.thread_call) {
        return kInt;
      }
      if (!signature->is_async && result == kFloat) const_cast<parser::Expr&>(expr).numeric_promotion = 1;
      return signature->is_async ? task_of(result) : result;
    }
    case parser::Expr::Kind::BitAnd:
    case parser::Expr::Kind::BitOr:
    case parser::Expr::Kind::BitXor:
    case parser::Expr::Kind::Shl:
    case parser::Expr::Kind::Shr:
    case parser::Expr::Kind::BitNot: {
      const Type left = expr.left ? check_expr(*expr.left, bindings, signatures, structs, diagnostics) : kError;
      const Type right = expr.right ? check_expr(*expr.right, bindings, signatures, structs, diagnostics) : kInt;
      if ((left != kAny && numeric_domain(left) != 1) || (right != kAny && numeric_domain(right) != 1)) {
        mismatch(expr, diagnostics); return kError;
      }
      return left.kind == Type::Kind::Parameter ? left : kInt;
    }
    case parser::Expr::Kind::Update: {
      const Type value = expr.left ? check_expr(*expr.left, bindings, signatures, structs, diagnostics) : kError;
      if (!is_numeric(value) && !numeric_domain(value)) { mismatch(expr, diagnostics); return kError; }
      return value;
    }
    case parser::Expr::Kind::Range: {
      const Type left = expr.left ? check_expr(*expr.left, bindings, signatures, structs, diagnostics) : kError;
      const Type right = expr.right ? check_expr(*expr.right, bindings, signatures, structs, diagnostics) : kError;
      if ((left != kAny && numeric_domain(left) != 1) || (right != kAny && numeric_domain(right) != 1)) mismatch(expr, diagnostics);
      Type result{Type::Kind::Struct, 65532}; result.parts = {kInt}; return result;
    }
    case parser::Expr::Kind::Ternary: {
      if (expr.left != nullptr) {
        const Type condition = check_expr(*expr.left, bindings, signatures, structs, diagnostics);
        optional_condition(*expr.left, condition, bindings);
      }
      const Type then_type = expr.right == nullptr ? kAny : check_expr(*expr.right, bindings, signatures, structs, diagnostics);
      if (!expr.args.empty()) {
        (void)check_expr(expr.args[0], bindings, signatures, structs, diagnostics);
      }
      return then_type;
    }
    case parser::Expr::Kind::Join: {
      if (expr.left != nullptr) {
        const Type handle = check_expr(*expr.left, bindings, signatures, structs, diagnostics);
        if (!is_numeric(handle) && handle != kError) {
          mismatch(*expr.left, diagnostics);
          diagnostics.back().expected_type = "numeric thread handle for join";
          diagnostics.back().actual_type = diagnostic_type(handle);
        }
      }
      return kAny;
    }
    case parser::Expr::Kind::Atomic: {
      for (const parser::Expr& arg : expr.args) {
        const Type type = check_expr(arg, bindings, signatures, structs, diagnostics);
        if (!is_numeric(type) && type != kError) {
          mismatch(arg, diagnostics);
        }
      }
      return kInt;
    }
    case parser::Expr::Kind::Parallel: {
      bool failed = false;
      for (const parser::Expr& arg : expr.args) {
        const Type inner = check_expr(arg, bindings, signatures, structs, diagnostics);
        if (inner == kError) {
          failed = true;
          continue;
        }
        if (inner == kAny) {
          continue;
        }
        if (inner.kind != Type::Kind::Task) {  // each argument is an async call; results may be any type
          mismatch(arg, diagnostics);
          failed = true;
        }
      }
      return failed ? kError : kAny;  // a list with one result per task
    }
    case parser::Expr::Kind::Spawn: {
      const Type inner = expr.left == nullptr ? kError : check_expr(*expr.left, bindings, signatures, structs, diagnostics);
      if (inner.kind == Type::Kind::Task || inner == kAny) {
        return inner.kind == Type::Kind::Task ? inner : task_of(kAny);
      }
      if (inner != kError) {
        mismatch(expr, diagnostics);
      }
      return kError;
    }
    case parser::Expr::Kind::Await: {
      const Type inner = expr.left == nullptr ? kError : check_expr(*expr.left, bindings, signatures, structs, diagnostics);
      if (inner.kind == Type::Kind::Task) {
        return unwrap_task(inner);
      }
      if (inner == kAny) {
        return kAny;
      }
      if (inner != kError) {
        mismatch(expr, diagnostics);
      }
      return kError;
    }
    case parser::Expr::Kind::Not: {
      const Type inner = expr.left == nullptr ? kError : check_expr(*expr.left, bindings, signatures, structs, diagnostics);
      const bool optional_test = expr.left && optional_operand(*expr.left, inner, bindings);
      const_cast<parser::Expr&>(expr).runtime_op = optional_test ? 1 : 0;
      if (!optional_test && inner != kBool && !is_numeric(inner)) {
        mismatch(expr, diagnostics);
        return kError;
      }
      return kBool;
    }
    case parser::Expr::Kind::And:
    case parser::Expr::Kind::Or: {
      const Type left = expr.left == nullptr ? kError : check_expr(*expr.left, bindings, signatures, structs, diagnostics);
      if (expr.left != nullptr) optional_condition(*expr.left, left, bindings);
      auto right_bindings = bindings;
      if (expr.left != nullptr) narrow_condition(*expr.left, expr.kind == parser::Expr::Kind::And, right_bindings);
      const Type right = expr.right == nullptr ? kError : check_expr(*expr.right, right_bindings, signatures, structs, diagnostics);
      if (expr.right != nullptr) optional_condition(*expr.right, right, right_bindings);
      if ((left != kBool && !is_numeric(left) && left.kind != Type::Kind::Optional && left != kNull) ||
          (right != kBool && !is_numeric(right) && right.kind != Type::Kind::Optional && right != kNull)) {
        mismatch(expr, diagnostics);
        return kError;
      }
      return kBool;
    }
    case parser::Expr::Kind::Eq:
    case parser::Expr::Kind::NotEq:
    case parser::Expr::Kind::Less:
    case parser::Expr::Kind::LessEq:
    case parser::Expr::Kind::Greater:
    case parser::Expr::Kind::GreaterEq: {
      const Type left = expr.left == nullptr ? kError : check_expr(*expr.left, bindings, signatures, structs, diagnostics);
      const Type right = expr.right == nullptr ? kError : check_expr(*expr.right, bindings, signatures, structs, diagnostics);
      const bool equality = expr.kind == parser::Expr::Kind::Eq || expr.kind == parser::Expr::Kind::NotEq;
      if (equality && is_text(left) && is_text(right)) {
        // A literal that the union can never hold is a bug ("Sleeping" vs "Idle" | "Running");
        // two literals are just a comparison ("abc" != "abd").
        if (left.kind == Type::Kind::Literal && right.kind == Type::Kind::Union && !contains_literal(right, left.literal)) {
          mismatch(expr, diagnostics);
        }
        if (right.kind == Type::Kind::Literal && left.kind == Type::Kind::Union && !contains_literal(left, right.literal)) {
          mismatch(expr, diagnostics);
        }
        return kBool;
      }
      if (equality && left != kError && right != kError && (assignable(left, right) || assignable(right, left))) {
        return kBool;  // enums, bools, structs, vectors: == and != compare values of the same type
      }
      if ((!is_numeric(left) && !numeric_domain(left)) || (!is_numeric(right) && !numeric_domain(right))) {
        mismatch(expr, diagnostics);
        return kError;
      }
      return kBool;
    }
    case parser::Expr::Kind::Concat: {
      const Type left = expr.left == nullptr ? kError : check_expr(*expr.left, bindings, signatures, structs, diagnostics);
      const Type right = expr.right == nullptr ? kError : check_expr(*expr.right, bindings, signatures, structs, diagnostics);
      // Every value can be joined as text; only a call with no result (void) cannot.
      if (left == kVoid || right == kVoid) {
        mismatch(expr, diagnostics);
        return kError;
      }
      const_cast<parser::Expr&>(expr).bool_left = left == kBool;
      const_cast<parser::Expr&>(expr).bool_right = right == kBool;
      return kString;
    }
    case parser::Expr::Kind::Add: {
      const Type left = expr.left == nullptr ? kError : check_expr(*expr.left, bindings, signatures, structs, diagnostics);
      const Type right = expr.right == nullptr ? kError : check_expr(*expr.right, bindings, signatures, structs, diagnostics);
      if (left == kVector3 && right == kVector3) {
        return kVector3;
      }
      if (left == kInt && right == kInt) {
        const_cast<parser::Expr&>(expr).int_specialized = true;
      }
      return check_numeric_binary(expr, left, right, false, diagnostics);
    }
    case parser::Expr::Kind::Mul: {
      const Type left = expr.left == nullptr ? kError : check_expr(*expr.left, bindings, signatures, structs, diagnostics);
      const Type right = expr.right == nullptr ? kError : check_expr(*expr.right, bindings, signatures, structs, diagnostics);
      return check_numeric_binary(expr, left, right, true, diagnostics);
    }
    case parser::Expr::Kind::Sub:
    case parser::Expr::Kind::Div:
    case parser::Expr::Kind::Mod: {
      const Type left = expr.left == nullptr ? kError : check_expr(*expr.left, bindings, signatures, structs, diagnostics);
      const Type right = expr.right == nullptr ? kError : check_expr(*expr.right, bindings, signatures, structs, diagnostics);
      if (expr.kind == parser::Expr::Kind::Div && left == kInt && right == kInt) {
        const_cast<parser::Expr&>(expr).int_specialized = true;  // int / int truncates (C++ semantics)
      }
      if (expr.kind == parser::Expr::Kind::Sub && left == kVector3 && right == kVector3) {
        return kVector3;  // target - origin
      }
      if (expr.kind == parser::Expr::Kind::Div && left == kVector3 && (is_numeric(right) || numeric_domain(right))) {
        return kVector3;  // velocity / 2
      }
      return check_numeric_binary(expr, left, right, false, diagnostics);
    }
    case parser::Expr::Kind::Index: {
      const Type source = expr.left ? check_expr(*expr.left, bindings, signatures, structs, diagnostics) : kError;
      const Type object = member_view(source);
      const bool slice = expr.right && expr.right->kind == parser::Expr::Kind::Range;
      const Type index = expr.right ? check_expr(*expr.right, bindings, signatures, structs, diagnostics) : kError;
      if (object.kind == Type::Kind::Tuple) {
        if (!slice && expr.right && expr.right->kind == parser::Expr::Kind::Number &&
            expr.right->exact_integer && index == kInt && expr.right->integer >= 0 &&
            static_cast<std::size_t>(expr.right->integer) < object.parts.size())
          return object.parts[static_cast<std::size_t>(expr.right->integer)];
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "tuple access requires an in-range constant integer index"});
        return kError;
      }
      const bool array = object.kind == Type::Kind::Struct && object.index == 65532;
      const bool dictionary = object.kind == Type::Kind::Struct && object.index == 65531;
      if (object == kAny) return kAny;
      if (object != kError && object != kVector3 && !is_string_value(object) && !array && !dictionary) mismatch(expr, diagnostics);
      if (dictionary) {
        if (slice) { mismatch(expr, diagnostics); return kError; }
        if (object.parts.size() == 2 && expr.right && !accepts_value(object.parts[0], index, *expr.right)) mismatch(*expr.right, diagnostics);
        return object.parts.size() == 2 ? object.parts[1] : kError;
      }
      if (!slice && index != kAny && index != kError && numeric_domain(index) != 1) mismatch(expr, diagnostics);
      if (is_string_value(object)) return kString;
      if (array && !object.parts.empty()) return slice ? source : object.parts[0];
      if (slice) { mismatch(expr, diagnostics); return kError; }
      return object == kVector3 ? kFloat : kError;
    }
    case parser::Expr::Kind::MakeList: {
      const bool typed = expr.value.rfind("array<", 0) == 0 || expr.value.rfind("dictionary<", 0) == 0;
      const Type collection = typed ? named_type(expr.value, structs) : kAny;
      const auto invalid = [&](const auto& self, const Type& type) -> bool {
        return type == kError || std::any_of(type.parts.begin(), type.parts.end(),
            [&](const Type& part) { return self(self, part); });
      };
      if (typed && invalid(invalid, collection)) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "unknown collection type"});
      }
      const bool dictionary = collection.kind == Type::Kind::Struct && collection.index == 65531;
      if (dictionary && expr.args.size() % 2 != 0) {
        diagnostics.push_back(Diagnostic{expr.location, expr.span, "dictionary requires key/value pairs"});
      }
      for (std::size_t index = 0; index < expr.args.size(); ++index) {
        const Type value = check_expr(expr.args[index], bindings, signatures, structs, diagnostics);
        if (typed && !collection.parts.empty()) {
          const Type expected = collection.parts[dictionary ? index % 2 : 0];
          if (!accepts_value(expected, value, expr.args[index])) mismatch(expr.args[index], diagnostics);
        }
      }
      // Preserve explicit constructor arguments instead of erasing array<T> and
      // dictionary<K,V> to a dynamic value at call/return/inference boundaries.
      return collection;
    }
    case parser::Expr::Kind::Sort: {
      const Type source = expr.args.empty() ? kError : check_expr(expr.args[0], bindings, signatures, structs, diagnostics);
      const Type array = member_view(source);
      if (array == kAny) return kAny;
      if (array.kind != Type::Kind::Struct || array.index != 65532 || array.parts.size() != 1 || !numeric_domain(array.parts[0])) {
        mismatch(expr, diagnostics); return kError;
      }
      return source;
    }
    case parser::Expr::Kind::ListMut: {
      std::vector<Type> args;
      for (const auto& argument : expr.args) args.push_back(check_expr(argument, bindings, signatures, structs, diagnostics));
      if (args.empty()) return kError;
      const Type collection = member_view(args[0]);
      const auto operation = static_cast<int>(expr.number);
      if (collection == kAny) return operation == 0 || operation == 2 ? kVoid : kAny;
      const bool array = collection.kind == Type::Kind::Struct && collection.index == 65532 && collection.parts.size() == 1;
      const bool dictionary = collection.kind == Type::Kind::Struct && collection.index == 65531 && collection.parts.size() == 2;
      if ((!array && !dictionary) || (dictionary && operation != 3)) { mismatch(expr, diagnostics); return kError; }
      if (operation == 0 || operation == 2) {
        const auto index = operation == 0 ? 1u : 2u;
        if (index >= args.size() || !accepts_value(collection.parts[0], args[index], expr.args[index])) mismatch(expr, diagnostics);
      }
      if (operation == 2 || operation == 3) {
        if (args.size() < 2) { mismatch(expr, diagnostics); return kError; }
        if (dictionary) {
          if (!accepts_value(collection.parts[0], args[1], expr.args[1])) mismatch(expr.args[1], diagnostics);
        } else if (args[1] != kAny && numeric_domain(args[1]) != 1) mismatch(expr.args[1], diagnostics);
      }
      return operation == 0 || operation == 2 ? kVoid : collection.parts[dictionary ? 1 : 0];
    }
    case parser::Expr::Kind::Len: {
      const Type collection = expr.args.empty() ? kError : member_view(check_expr(expr.args[0], bindings, signatures, structs, diagnostics));
      const bool table = collection.kind == Type::Kind::Struct && (collection.index == 65531 || collection.index == 65532);
      if (collection != kAny && !is_string_value(collection) && !table) mismatch(expr, diagnostics);
      return kInt;
    }
    case parser::Expr::Kind::Find: {
      if (expr.args.size() != 2) { mismatch(expr, diagnostics); return kError; }
      const Type array = expr.args.empty() ? kError : member_view(check_expr(expr.args[0], bindings, signatures, structs, diagnostics));
      const Type needle = expr.args.size() < 2 ? kError : check_expr(expr.args[1], bindings, signatures, structs, diagnostics);
      if (array != kAny && (array.kind != Type::Kind::Struct || array.index != 65532 || array.parts.size() != 1 ||
          !accepts_value(array.parts[0], needle, expr.args[1]))) mismatch(expr, diagnostics);
      return kInt;
    }
    case parser::Expr::Kind::PCall: {
      if (expr.left != nullptr) {
        (void)check_expr(*expr.left, bindings, signatures, structs, diagnostics);
      }
      return kInt;
    }
  }
  mismatch(expr, diagnostics);
  return kError;
}

void check_condition(const parser::Expr& expr, const std::vector<Binding>& bindings,
                     const std::vector<Signature>& signatures, const std::vector<StructInfo>& structs,
                     std::vector<Diagnostic>& diagnostics) {
  const Type type = check_expr(expr, bindings, signatures, structs, diagnostics);
  optional_condition(expr, type, bindings);
  if (type != kBool && !is_numeric(type) && type != kError && type != kNull && type.kind != Type::Kind::Optional) {
    mismatch(expr, diagnostics);
  }
}

[[nodiscard]] bool contains_break(const std::vector<parser::Stmt>& statements) {
  for (const parser::Stmt& stmt : statements) {
    if (stmt.kind == parser::Stmt::Kind::Break && !stmt.is_continue) {
      return true;
    }
    if (stmt.kind != parser::Stmt::Kind::While && stmt.kind != parser::Stmt::Kind::For &&
        stmt.kind != parser::Stmt::Kind::ForIn && (contains_break(stmt.then_body) || contains_break(stmt.else_body))) {
      return true;
    }
  }
  return false;
}

// Every path through the statements ends in `return value`, `report(...)` or an endless loop.
// (contains_return only asked whether some return exists, so `if (n > 0) { return 1; }` passed.)
[[nodiscard]] bool always_returns(const std::vector<parser::Stmt>& statements) {
  for (const parser::Stmt& stmt : statements) {
    switch (stmt.kind) {
      case parser::Stmt::Kind::Return:
        return true;
      case parser::Stmt::Kind::Post:
        if (stmt.channel == 2) {
          return true;  // report(...) stops here
        }
        break;
      case parser::Stmt::Kind::If:
        if (stmt.has_else && always_returns(stmt.then_body) && always_returns(stmt.else_body)) {
          return true;
        }
        break;
      case parser::Stmt::Kind::Try:
        if (always_returns(stmt.then_body) && always_returns(stmt.else_body)) {
          return true;
        }
        break;
      case parser::Stmt::Kind::Match: {
        bool all = stmt.match_exhaustive && !stmt.arms.empty();
        for (const parser::Stmt::MatchArm& arm : stmt.arms) {
          all = all && always_returns(arm.body);
        }
        if (all) {
          return true;
        }
        break;
      }
      case parser::Stmt::Kind::While:
        if (stmt.expr.kind == parser::Expr::Kind::Number && stmt.expr.number != 0 && !contains_break(stmt.then_body)) {
          return true;  // while (true) without break never falls through
        }
        break;
      default:
        break;
    }
  }
  return false;
}

[[nodiscard]] bool exits_guard_branch(const std::vector<parser::Stmt>& statements) {
  if (always_returns(statements)) return true;
  for (const auto& statement : statements) {
    if (statement.kind == parser::Stmt::Kind::Break) return true;
    if (statement.kind == parser::Stmt::Kind::If && statement.has_else &&
        exits_guard_branch(statement.then_body) && exits_guard_branch(statement.else_body)) return true;
  }
  return false;
}

void check_stmts(const std::vector<parser::Stmt>& statements, std::vector<Binding>& bindings,
                 const std::vector<Signature>& signatures, const std::vector<StructInfo>& structs, const Type return_type,
                 std::vector<Diagnostic>& diagnostics) {
  const std::size_t watermark = bindings.size();
  bool dead = false;
  for (const parser::Stmt& stmt : statements) {
    if (dead) {
      diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "unreachable code"});
      continue;
    }
    if (stmt.kind == parser::Stmt::Kind::Return) {
      dead = true;
    }
    if (stmt.kind == parser::Stmt::Kind::VariableDecl && stmt.expr.missing) {
      const Type declared = stmt.declared_type.empty() ? kAny : named_type(stmt.declared_type, structs);
      bindings.push_back(Binding{stmt.name, declared == kError ? kAny : declared});
      continue;
    }
    if (stmt.kind == parser::Stmt::Kind::VariableDecl) {
      const Type value = check_expr(stmt.expr, bindings, signatures, structs, diagnostics);
      const Type declared = stmt.has_decltype ? check_expr(stmt.type_expr, bindings, signatures, structs, diagnostics)
                                              : named_type(stmt.declared_type, structs);
      if (declared == kError) {
        diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "unknown type"});
      } else if (!accepts_value(declared, value, stmt.expr)) {
        mismatch(stmt.expr, diagnostics, declared, value);
      }
      Type stored = declared == kAny ? value : declared;
      // `auto name = "Ada";` holds a string, not the literal type "Ada": otherwise every later
      // assignment (`name = "Bob";`) was a type mismatch. Literal types stay for immutable
      // bindings and for explicitly declared unions (`const State state = "Idle";`).
      if (declared == kAny && !stmt.immutable && stored.kind == Type::Kind::Literal) {
        stored = kString;
      }
      if (active_mode == parser::CheckMode::Strict && stmt.declared_type.empty() && !stmt.has_decltype && stored == kAny) {
        diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "missing type"});
      }
      bindings.push_back(Binding{stmt.name, stored == kError ? kAny : stored, stmt.module_global});
      continue;
    }
    if (stmt.kind == parser::Stmt::Kind::PlaceAssign) {
      const Type value = check_expr(stmt.expr, bindings, signatures, structs, diagnostics);
      const Type destination = check_expr(stmt.place, bindings, signatures, structs, diagnostics);
      if (destination != kError && !accepts_value(destination, value, stmt.expr)) {
        mismatch(stmt.expr, diagnostics, destination, value);
      }
      continue;
    }
    if (stmt.kind == parser::Stmt::Kind::Assign) {
      const Type value = check_expr(stmt.expr, bindings, signatures, structs, diagnostics);
      for (auto binding = bindings.rbegin(); binding != bindings.rend(); ++binding) {
        if (binding->name != stmt.name) continue;
        if (binding->declared != kError && !accepts_value(binding->declared, value, stmt.expr)) mismatch(stmt.expr, diagnostics, binding->declared, value);
        // A write invalidates any refinement of the previous value.
        binding->type = binding->declared;
        break;
      }
      continue;
    }
    if (stmt.kind == parser::Stmt::Kind::Return) {
      if (!stmt.returns_value) {
        if (return_type != kVoid && return_type != kAny) {
          diagnostics.push_back(Diagnostic{stmt.name_location, {}, "type mismatch"});
        }
        continue;
      }
      const Type value = check_expr(stmt.expr, bindings, signatures, structs, diagnostics);
      if (!accepts_value(return_type, value, stmt.expr)) {
        mismatch(stmt.expr, diagnostics, return_type, value);
      }
      continue;
    }
    if (stmt.kind == parser::Stmt::Kind::If || stmt.kind == parser::Stmt::Kind::While) {
      check_condition(stmt.expr, bindings, signatures, structs, diagnostics);
      auto then_bindings = bindings;
      auto else_bindings = bindings;
      if (!stmt.post_condition) narrow_condition(stmt.expr, true, then_bindings);
      narrow_condition(stmt.expr, false, else_bindings);
      check_stmts(stmt.then_body, then_bindings, signatures, structs, return_type, diagnostics);
      if (stmt.has_else) check_stmts(stmt.else_body, else_bindings, signatures, structs, return_type, diagnostics);
      if (stmt.is_guard) {
        if (!exits_guard_branch(stmt.then_body)) {
          diagnostics.push_back(Diagnostic{stmt.expr.location, stmt.expr.span, "guard failure branch must exit"});
        }
        narrow_condition(stmt.expr, false, bindings);
      } else {
        for (std::size_t index = 0; index < bindings.size(); ++index) {
          if (then_bindings[index].type != bindings[index].type || else_bindings[index].type != bindings[index].type) {
            bindings[index].type = bindings[index].declared;
          }
        }
      }
      continue;
    }
    if (stmt.kind == parser::Stmt::Kind::Break || stmt.kind == parser::Stmt::Kind::Signal ||
        stmt.kind == parser::Stmt::Kind::Connect || stmt.kind == parser::Stmt::Kind::Using) {
      continue;
    }
    if (stmt.kind == parser::Stmt::Kind::ForIn) {
      // `for (auto i in 10)` counts 0..9; `for (auto item in list)` walks the elements.
      const Type source = check_expr(stmt.expr, bindings, signatures, structs, diagnostics);
      const Type view = member_view(source);
      const bool numeric = numeric_domain(source) == 1;
      const bool collection = view.kind == Type::Kind::Struct &&
          (view.index == 65532 || view.index == 65531);
      if (!numeric && source != kAny && !is_string_value(view) && !collection) {
        diagnostics.push_back(Diagnostic{stmt.expr.location, stmt.expr.span, "for-in expects an integer count, array, dictionary or string"});
      }
      const_cast<parser::Stmt&>(stmt).numeric_range = numeric;
      Type item = iteration_element(source);
      if (!stmt.declared_type.empty()) {
        const Type declared = named_type(stmt.declared_type, structs);
        if (declared == kError) {
          diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "unknown type"});
        } else if (!assignable(declared, item)) {
          diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "type mismatch"});
        }
        if (declared == kFloat) {
          const_cast<parser::Stmt&>(stmt).item_promotion = 1;
          const_cast<parser::Stmt&>(stmt).numeric_range = false;
        } else if (declared.kind == Type::Kind::Optional && !declared.parts.empty() && declared.parts[0] == kFloat) {
          const_cast<parser::Stmt&>(stmt).item_promotion = 2;
          const_cast<parser::Stmt&>(stmt).numeric_range = false;
        }
        item = declared;
      }
      const std::size_t mark = bindings.size();
      bindings.push_back(Binding{stmt.name, item});
      check_stmts(stmt.then_body, bindings, signatures, structs, return_type, diagnostics);
      bindings.resize(mark);
      continue;
    }
    if (stmt.kind == parser::Stmt::Kind::For) {
      const std::size_t mark = bindings.size();
      for (const parser::Stmt& init : stmt.init) {
        if (init.kind == parser::Stmt::Kind::VariableDecl) {
          const Type value = check_expr(init.expr, bindings, signatures, structs, diagnostics);
          const Type declared = named_type(init.declared_type, structs);
          if (declared == kError) {
            diagnostics.push_back(Diagnostic{init.name_location, init.name_span, "unknown type"});
          } else if (!accepts_value(declared, value, stmt.expr)) {
            mismatch(init.expr, diagnostics);
          }
          const Type stored = declared == kAny ? value : declared;
          bindings.push_back(Binding{init.name, stored == kError ? kAny : stored});
        } else {
          const Type value = check_expr(init.expr, bindings, signatures, structs, diagnostics);
          const Type destination = find_binding(bindings, init.name);
          if (destination != kError && !accepts_value(destination, value, stmt.expr)) {
            mismatch(init.expr, diagnostics);
          }
        }
      }
      check_condition(stmt.expr, bindings, signatures, structs, diagnostics);
      check_stmts(stmt.then_body, bindings, signatures, structs, return_type, diagnostics);
      check_stmts(stmt.step, bindings, signatures, structs, return_type, diagnostics);
      bindings.resize(mark);
      continue;
    }
    if (stmt.kind == parser::Stmt::Kind::Match) {
      const Type scrutinee = check_expr(stmt.expr, bindings, signatures, structs, diagnostics);
      const bool enum_match = scrutinee.kind == Type::Kind::Enum;
      const bool literal_union = scrutinee.kind == Type::Kind::Union && !scrutinee.parts.empty() &&
          std::all_of(scrutinee.parts.begin(), scrutinee.parts.end(), [](const Type& type) { return type.kind == Type::Kind::Literal; });
      std::vector<char> covered;
      if (enum_match && active_enums != nullptr && scrutinee.index < active_enums->size()) {
        covered.assign((*active_enums)[scrutinee.index].variants.size(), 0);
      } else if (literal_union) {
        covered.assign(scrutinee.parts.size(), 0);
      } else if (!is_numeric(scrutinee) && !is_string_value(scrutinee) && scrutinee != kBool && scrutinee != kError) {
        mismatch(stmt.expr, diagnostics);
      }
      // Text is matched against text ("play" ~> ...), numbers and bools against numbers and bools.
      const bool text_match = !enum_match && scrutinee != kAny && (literal_union || is_string_value(scrutinee));
      bool wildcard = false;
      for (const parser::Stmt::MatchArm& arm : stmt.arms) {
        if (wildcard) diagnostics.push_back(Diagnostic{arm.pattern.location, arm.pattern.span, "unreachable match arm"});
        if (arm.wildcard) {
          wildcard = wildcard || !arm.has_guard;
        } else if (arm.tag_match) {
          if (enum_match && arm.pattern.number >= 0 &&
              static_cast<std::size_t>(arm.pattern.number) < covered.size()) {
            covered[static_cast<std::size_t>(arm.pattern.number)] = 1;
          }
        } else {
          const Type pattern = check_expr(arm.pattern, bindings, signatures, structs, diagnostics);
          if (enum_match) {
            if ((pattern.kind != Type::Kind::Enum || pattern.index != scrutinee.index) && pattern != kError) {
              mismatch(arm.pattern, diagnostics);
            } else if (!arm.has_guard && pattern.kind == Type::Kind::Enum &&
                       arm.pattern.number >= 0 &&
                       static_cast<std::size_t>(arm.pattern.number) < covered.size()) {
              const std::size_t case_index = static_cast<std::size_t>(arm.pattern.number);
              if (covered[case_index] != 0) diagnostics.push_back(Diagnostic{arm.pattern.location, arm.pattern.span, "duplicate match case"});
              covered[case_index] = 1;
            }
          } else if (literal_union) {
            const auto found = std::find_if(scrutinee.parts.begin(), scrutinee.parts.end(),
                [&](const Type& alternative) { return pattern.kind == Type::Kind::Literal && alternative.literal == pattern.literal; });
            if (found == scrutinee.parts.end()) {
              mismatch(arm.pattern, diagnostics);
            } else if (!arm.has_guard) {
              const std::size_t case_index = static_cast<std::size_t>(found - scrutinee.parts.begin());
              if (covered[case_index] != 0) diagnostics.push_back(Diagnostic{arm.pattern.location, arm.pattern.span, "duplicate match case"});
              covered[case_index] = 1;
            }
          } else if (text_match ? !is_string_value(pattern)
                                : !is_numeric(pattern) && pattern != kBool && !(scrutinee == kAny && is_string_value(pattern))) {
            if (pattern != kError) {
              mismatch(arm.pattern, diagnostics);
            }
          }
        }
        if (arm.has_guard) {
          check_condition(arm.guard, bindings, signatures, structs, diagnostics);
        }
        if (arm.has_payload) {
          bindings.push_back(Binding{arm.bind_name, kAny});
        }
        check_stmts(arm.body, bindings, signatures, structs, return_type, diagnostics);
        if (arm.has_payload && !bindings.empty()) {
          bindings.pop_back();
        }
      }
      const bool finite = enum_match || literal_union;
      const bool all_covered = finite && !covered.empty() &&
          std::all_of(covered.begin(), covered.end(), [](const char seen) { return seen != 0; });
      const_cast<parser::Stmt&>(stmt).match_exhaustive = wildcard || all_covered;
      if (finite && !stmt.match_exhaustive) {
        diagnostics.push_back(Diagnostic{stmt.expr.location, stmt.expr.span, "non-exhaustive match"});
      }
      continue;
    }
    if (stmt.kind == parser::Stmt::Kind::Expr) {
      (void)check_expr(stmt.expr, bindings, signatures, structs, diagnostics);
      continue;
    }
    const Type value = check_expr(stmt.expr, bindings, signatures, structs, diagnostics);
    if (value == kVoid || value.kind == Type::Kind::Task) {
      mismatch(stmt.expr, diagnostics);
    }
    if (stmt.kind == parser::Stmt::Kind::Post && value == kBool) {
      const_cast<parser::Stmt&>(stmt).print_as_bool = true;
    }
  }
  bindings.resize(watermark);
}

[[nodiscard]] std::vector<StructInfo> build_data_types(const parser::Program& program, std::vector<Diagnostic>& diagnostics) {
  std::vector<StructInfo> structs(program.structs.size());
  for (std::size_t index = 0; index < program.structs.size(); ++index) {
    structs[index].name = program.structs[index].name;
    structs[index].type_params = program.structs[index].type_params;
    structs[index].is_abstract = program.structs[index].is_abstract;
  }
  for (std::size_t index = 0; index < program.structs.size(); ++index) {
    for (const parser::Field& field : program.structs[index].fields) {
      Type field_type = named_type(field.type_name, structs, 0, &structs[index]);
      if (field_type == kError) {
        diagnostics.push_back(Diagnostic{field.name_location, field.name_span, "unknown type"});
        field_type = kAny;
      }
      structs[index].fields.push_back(field_type);
    }
  }

  for (std::size_t index = 0; index < program.structs.size(); ++index) {
    const auto& data = program.structs[index];
    const auto bases = data.bases.empty() && !data.base.empty() ? std::vector<std::string>{data.base} : data.bases;
    for (const auto& spelling : bases) {
      const Type base = named_type(spelling, structs, 0, &structs[index]);
      if (base.kind != Type::Kind::Struct || base.index >= structs.size()) {
        diagnostics.push_back(Diagnostic{data.base_location, data.base_span, "invalid base type specialization"});
      } else {
        structs[index].bases.push_back(base);
      }
    }
  }
  for (std::size_t index = 0; index < structs.size(); ++index) {
    std::vector<Type> views(structs.size(), kError);
    const auto visit = [&](const auto& recurse, const Type& type) -> bool {
      if (type.kind != Type::Kind::Struct || type.index >= structs.size()) return false;
      auto& previous = views[type.index];
      if (previous != kError) return previous == type;
      previous = type;
      const auto& owner = structs[type.index];
      for (const auto& base : owner.bases)
        if (!recurse(recurse, specialize_type(base, owner, type.parts))) return false;
      return true;
    };
    Type self{Type::Kind::Struct, static_cast<std::uint16_t>(index)};
    for (std::size_t parameter = 0; parameter < structs[index].type_params.size(); ++parameter) {
      Type formal{Type::Kind::Parameter, static_cast<std::uint16_t>(parameter)};
      formal.literal = structs[index].name; self.parts.push_back(std::move(formal));
    }
    if (!visit(visit, self)) {
      const auto& data = program.structs[index];
      diagnostics.push_back(Diagnostic{data.name_location, data.name_span, "conflicting inherited type specializations"});
    }
  }
  return structs;
}

[[nodiscard]] std::vector<Signature> build_signatures(const parser::Program& program, const std::vector<StructInfo>& structs,
                                                    std::vector<Diagnostic>& diagnostics) {
  std::vector<Signature> signatures;
  for (const parser::Function& function : program.functions) {
    if (function.index == 65535) {
      continue;
    }
    Signature signature;
    signature.name = function.name;
    signature.is_async = function.is_async;
    signature.anonymous = function.anonymous;
    signature.index = function.index;
    signature.type_params = function.type_params;
    signature.owner_type_param_count = function.owner_type_param_count;
    signature.where_names = function.where_names;
    signature.where_types = function.where_types;
    const TypeScope context = function_scope(function, structs, program);
    signature.result = named_type(function.return_type, structs, 0, &context);
    if (signature.result == kError) {
      diagnostics.push_back(Diagnostic{function.name_location, function.name_span, "unknown return type"});
    }
    for (const std::string& param_type : function.param_types) {
      const Type type = named_type(param_type, structs, 0, &context);
      if (type == kError) diagnostics.push_back(Diagnostic{function.name_location, function.name_span, "unknown type"});
      signature.params.push_back(type);
    }
    for (std::size_t constraint = 0; constraint < function.where_names.size() && constraint < function.where_types.size(); ++constraint) {
      if (std::find(function.type_params.begin(), function.type_params.end(), function.where_names[constraint]) == function.type_params.end() ||
          named_type(function.where_types[constraint], structs, 0, &context) == kError) {
        diagnostics.push_back(Diagnostic{function.name_location, function.name_span, "invalid generic constraint"});
      }
    }
    signatures.push_back(std::move(signature));
  }

  return signatures;
}

[[nodiscard]] std::string source_type(const Type& type, const parser::Program& program) {
  switch (type.kind) {
    case Type::Kind::Int: return "number";
    case Type::Kind::Float: return "float";
    case Type::Kind::Bool: return "bool";
    case Type::Kind::String: return "string";
    case Type::Kind::Void: return "void";
    case Type::Kind::Vector3: return "Vector3";
    case Type::Kind::Buffer: return "buffer";
    case Type::Kind::Connection: return "connection";
    case Type::Kind::Signal:
    case Type::Kind::Tuple: {
      std::string text = type.kind == Type::Kind::Signal ? "signal<" : "tuple<";
      for (std::size_t index = 0; index < type.parts.size(); ++index) {
        if (index) text += ',';
        const auto part = source_type(type.parts[index], program); if (part.empty()) return {};
        text += part;
      }
      return text + ">";
    }
    case Type::Kind::Task: {
      const auto inner = source_type(unwrap_task(type), program);
      return inner.empty() ? "task" : "task<" + inner + ">";
    }
    case Type::Kind::Literal: return "\"" + type.literal + "\"";
    case Type::Kind::Enum: return type.index < program.enums.size() ? program.enums[type.index].name : std::string{};
    case Type::Kind::Function: {
      if (type.parts.empty()) return {};
      std::string text = "(";
      for (std::size_t i = 0; i + 1 < type.parts.size(); ++i) {
        if (i) text += ',';
        text += source_type(type.parts[i], program);
      }
      return text + ")->" + source_type(type.parts.back(), program);
    }
    case Type::Kind::Parameter: {
      for (const auto& function : program.functions) {
        if (type.literal == "$function:" + function.name && type.index < function.type_params.size()) return function.type_params[type.index];
      }
      for (const auto& data : program.structs) {
        if (type.literal == data.name && type.index < data.type_params.size()) return data.type_params[type.index];
      }
      return {};
    }
    case Type::Kind::Struct:
    case Type::Kind::Optional:
    case Type::Kind::Union:
    case Type::Kind::Intersect: {
      std::string result;
      if (type.kind == Type::Kind::Struct) {
        if (type.index == 65532) result = "array";
        else if (type.index == 65531) result = "dictionary";
        else if (type.index < program.structs.size()) result = program.structs[type.index].name;
        else return {};
      } else if (type.kind == Type::Kind::Optional) result = "optional";
      const bool combined = type.kind == Type::Kind::Union || type.kind == Type::Kind::Intersect;
      if (!combined && !type.parts.empty()) result += '<';
      for (std::size_t index = 0; index < type.parts.size(); ++index) {
        const auto part = source_type(type.parts[index], program);
        if (part.empty()) return {};
        if (index) result += combined ? (type.kind == Type::Kind::Union ? '|' : '&') : ',';
        result += part;
      }
      if (!combined && !type.parts.empty()) result += '>';
      return result;
    }
    default: return {};  // Do not publish an invented type for dynamic/unsupported values.
  }
}

struct ActiveTypesGuard {
  const parser::Program* program = active_program;
  decltype(active_enums) enums = active_enums;
  decltype(active_aliases) aliases = active_aliases;
  decltype(active_structs) structs = active_structs;
  parser::CheckMode mode = active_mode;
  const TypeScope* function = active_function_types;
  const std::vector<StructInfo>* info = active_type_info;
  ~ActiveTypesGuard() {
    active_program = program;
    active_enums = enums; active_aliases = aliases;
    active_structs = structs; active_mode = mode; active_function_types = function; active_type_info = info;
  }
};

}  // namespace

static std::optional<std::string> expression_query(const parser::Program& program, const parser::Expr& expression,
                                                  const std::vector<LocalType>& locals, const parser::Function* scope, bool iteration) {
  const ActiveTypesGuard restore;
  active_mode = program.mode;
  active_structs = &program.structs;
  active_enums = &program.enums;
  active_aliases = &program.aliases;
  active_function_types = nullptr;
  std::vector<Diagnostic> diagnostics;
  const auto structs = build_data_types(program, diagnostics);
  active_type_info = &structs;
  const auto signatures = build_signatures(program, structs, diagnostics);
  TypeScope context;
  if (scope != nullptr) {
    context = function_scope(*scope, structs, program);
    active_function_types = &context;
  }
  std::vector<Binding> bindings;
  for (const auto& local : locals) bindings.emplace_back(local.name, named_type(local.type, structs), local.global);
  // Unbound default callbacks can have provisional signatures elsewhere in the
  // file. They must not erase valid evidence for this expression while binding.
  const std::size_t query_diagnostics = diagnostics.size();
  Type type = check_expr(expression, bindings, signatures, structs, diagnostics);
  if (iteration) type = iteration_element(type);
  if (diagnostics.size() != query_diagnostics || type == kError) return std::nullopt;
  const std::string spelling = source_type(type, program);
  return spelling.empty() ? std::nullopt : std::optional<std::string>{spelling};
}

std::optional<std::string> expression_type(const parser::Program& program, const parser::Expr& expression,
                                           const std::vector<LocalType>& locals, const parser::Function* scope) {
  return expression_query(program, expression, locals, scope, false);
}

std::optional<std::string> iteration_type(const parser::Program& program, const parser::Expr& expression,
                                         const std::vector<LocalType>& locals, const parser::Function* scope) {
  return expression_query(program, expression, locals, scope, true);
}

std::optional<std::vector<std::string>> callback_parameters(
    const parser::Program& program, const std::string& expected, const parser::Function* scope) {
  if (expected.empty()) return std::nullopt;
  const ActiveTypesGuard restore;
  active_structs = &program.structs; active_enums = &program.enums;
  active_aliases = &program.aliases; active_function_types = nullptr;
  std::vector<Diagnostic> diagnostics;
  const auto structs = build_data_types(program, diagnostics); active_type_info = &structs;
  TypeScope context;
  if (scope) { context = function_scope(*scope, structs, program); active_function_types = &context; }
  const Type type = named_type(expected, structs);
  if (!diagnostics.empty() || type.kind != Type::Kind::Function || type.parts.empty() || unresolved_type(type)) return std::nullopt;
  std::vector<std::string> parameters;
  for (std::size_t index = 0; index + 1 < type.parts.size(); ++index) {
    const auto spelling = source_type(type.parts[index], program);
    if (spelling.empty() || type.parts[index] == kVoid) return std::nullopt;
    parameters.push_back(spelling);
  }
  return parameters;
}

std::optional<std::string> array_initializer_type(
    const parser::Program& program, const std::string& expected, const parser::Function* scope) {
  if (expected.empty()) return std::nullopt;
  const ActiveTypesGuard restore;
  active_structs = &program.structs; active_enums = &program.enums;
  active_aliases = &program.aliases; active_function_types = nullptr;
  std::vector<Diagnostic> diagnostics;
  const auto structs = build_data_types(program, diagnostics); active_type_info = &structs;
  TypeScope context;
  if (scope) { context = function_scope(*scope, structs, program); active_function_types = &context; }
  const Type type = named_type(expected, structs);
  if (!diagnostics.empty() || type.kind != Type::Kind::Struct || type.index != 65532 || type.parts.size() != 1) return std::nullopt;
  const auto spelling = source_type(type, program);
  return spelling.empty() ? std::nullopt : std::optional<std::string>{spelling};
}

std::vector<std::string> argument_types(const parser::Program& program, const parser::Expr& expression,
                                      const std::vector<LocalType>& locals, const parser::Function* scope) {
  const ActiveTypesGuard restore;
  active_structs = &program.structs; active_enums = &program.enums;
  active_aliases = &program.aliases; active_function_types = nullptr;
  active_mode = program.mode;
  std::vector<Diagnostic> diagnostics;
  const auto structs = build_data_types(program, diagnostics); active_type_info = &structs;
  const auto signatures = build_signatures(program, structs, diagnostics);
  TypeScope caller;
  if (scope) { caller = function_scope(*scope, structs, program); active_function_types = &caller; }
  std::vector<Binding> bindings;
  for (const auto& local : locals) bindings.emplace_back(local.name, named_type(local.type, structs), local.global);
  const std::size_t query_diagnostics = diagnostics.size();
  std::vector<Type> expected;
  if (expression.kind == parser::Expr::Kind::Invoke && expression.left) {
    const Type callable = check_expr(*expression.left, bindings, signatures, structs, diagnostics);
    if (callable.kind != Type::Kind::Function || callable.parts.size() != expression.args.size() + 1) return {};
    expected.assign(callable.parts.begin(), callable.parts.end() - 1);
  } else if (expression.kind == parser::Expr::Kind::Event &&
             (expression.runtime_op == 1 || expression.runtime_op == 5 || expression.runtime_op == 9) && !expression.args.empty()) {
    const auto signal = member_view(check_expr(expression.args[0], bindings, signatures, structs, diagnostics));
    if (signal.kind != Type::Kind::Signal) return {};
    Type callback{Type::Kind::Function, 0}; callback.parts = signal.parts; callback.parts.push_back(kVoid);
    expected = {signal, callback};
  } else if (expression.kind == parser::Expr::Kind::Call) {
    const auto open = expression.value.find('<');
    const Signature* signature = find_signature(signatures, expression.value.substr(0, open));
    if (!signature || signature->params.size() != expression.args.size()) return {};
    TypeScope callee; callee.name = "$function:" + signature->name; callee.type_params = signature->type_params;
    std::vector<Type> arguments(signature->type_params.size(), kError);
    const auto owner_count = signature->owner_type_param_count;
    if (owner_count) {
      const auto receiver = check_expr(expression.args[0], bindings, signatures, structs, diagnostics);
      if (!infer_arguments(signature->params[0], receiver, callee, arguments)) return {};
    }
    if (open != std::string::npos && expression.value.back() == '>') {
      const auto supplied = split_type(std::string_view(expression.value).substr(open + 1, expression.value.size() - open - 2), ',');
      if (supplied.size() != arguments.size() - owner_count) return {};
      for (std::size_t index = 0; index < supplied.size(); ++index) arguments[owner_count + index] = named_type(supplied[index], structs);
    } else if (!arguments.empty()) {
      std::vector<Type> actuals(expression.args.size(), kError);
      std::vector<InferenceEvidence> evidence;
      for (std::size_t index = owner_count ? 1 : 0; index < expression.args.size(); ++index) {
        const auto& arg = expression.args[index];
        if (arg.kind == parser::Expr::Kind::MakeList && arg.value == "$array_initializer") continue;
        if (arg.kind == parser::Expr::Kind::Closure) {
          const auto lambda = std::find_if(program.functions.begin(), program.functions.end(), [&](const auto& function) { return function.name == arg.value; });
          if (lambda != program.functions.end() && lambda->anonymous &&
              std::any_of(lambda->param_types.begin(), lambda->param_types.end(), [](const auto& type) { return type.empty(); })) continue;
        }
        actuals[index] = check_expr(arg, bindings, signatures, structs, diagnostics);
        evidence.emplace_back(&signature->params[index], &actuals[index]);
      }
      if (!infer_evidence(std::move(evidence), callee, arguments, owner_count)) return {};
    }
    if (std::any_of(arguments.begin(), arguments.end(), unresolved_type)) return {};
    check_function_constraints(expression, *signature, callee, arguments, signatures, structs, diagnostics);
    for (const auto& param : signature->params) expected.push_back(specialize_type(param, callee, arguments));
  } else if (expression.kind == parser::Expr::Kind::Construct) {
    const Type data = named_type(expression.value, structs);
    if (data.kind != Type::Kind::Struct || data.index >= structs.size()) return {};
    for (const auto& field : structs[data.index].fields) expected.push_back(specialize_type(field, structs[data.index], data.parts));
  } else if (expression.kind == parser::Expr::Kind::MakeList) {
    const Type collection = named_type(expression.value, structs);
    if (collection.kind != Type::Kind::Struct || collection.parts.empty()) return {};
    for (std::size_t index = 0; index < expression.args.size(); ++index)
      expected.push_back(collection.parts[index % collection.parts.size()]);
  } else return {};
  if (diagnostics.size() != query_diagnostics) return {};
  std::vector<std::string> result;
  for (const auto& type : expected) result.push_back(unresolved_type(type) ? std::string{} : source_type(type, program));
  return result;
}

std::optional<std::string> member_owner_type(const parser::Program& program, const std::string& spelling,
                                             const parser::Function* scope, const std::string& member) {
  const ActiveTypesGuard restore;
  active_structs = &program.structs; active_enums = &program.enums;
  active_aliases = &program.aliases;
  active_function_types = nullptr;
  std::vector<Diagnostic> diagnostics;
  const auto structs = build_data_types(program, diagnostics);
  active_type_info = &structs;
  TypeScope context;
  if (scope != nullptr) {
    context = function_scope(*scope, structs, program);
    active_function_types = &context;
  }
  const Type type = member.empty() ? member_view(named_type(spelling, structs)) : named_member_view(named_type(spelling, structs), member);
  if (!diagnostics.empty() || type.kind != Type::Kind::Struct) return std::nullopt;
  const auto result = source_type(type, program);
  return result.empty() ? std::nullopt : std::optional<std::string>{result};
}

std::optional<std::string> data_initializer_type(const parser::Program& program, const std::string& spelling,
                                                const parser::Function* scope) {
  const ActiveTypesGuard restore;
  active_structs = &program.structs; active_enums = &program.enums;
  active_aliases = &program.aliases; active_function_types = nullptr;
  std::vector<Diagnostic> diagnostics;
  const auto structs = build_data_types(program, diagnostics); active_type_info = &structs;
  TypeScope context;
  if (scope) { context = function_scope(*scope, structs, program); active_function_types = &context; }
  const Type type = named_type(spelling, structs);
  if (!diagnostics.empty() || type.kind != Type::Kind::Struct) return std::nullopt;
  return source_type(type, program);
}

std::optional<std::string> resource_type(const parser::Program& program, const std::string& spelling,
                                       const parser::Function* scope) {
  const ActiveTypesGuard restore;
  active_structs = &program.structs; active_enums = &program.enums;
  active_aliases = &program.aliases; active_function_types = nullptr;
  std::vector<Diagnostic> diagnostics;
  const auto structs = build_data_types(program, diagnostics); active_type_info = &structs;
  TypeScope context;
  if (scope) { context = function_scope(*scope, structs, program); active_function_types = &context; }
  const auto type = member_view(named_type(spelling, structs));
  if (!diagnostics.empty() || (type.kind != Type::Kind::Signal && type.kind != Type::Kind::Connection)) return std::nullopt;
  return source_type(type, program);
}

std::optional<std::string> data_base_type(const parser::Program& program, const std::string& spelling,
                                          const parser::StructDecl& owner) {
  const ActiveTypesGuard restore;
  active_structs = &program.structs; active_enums = &program.enums;
  active_aliases = &program.aliases; active_function_types = nullptr;
  std::vector<Diagnostic> diagnostics;
  const auto structs = build_data_types(program, diagnostics); active_type_info = &structs;
  TypeScope context; context.name = owner.name; context.type_params = owner.type_params;
  const Type type = named_type(spelling, structs, 0, &context);
  if (!diagnostics.empty() || type.kind != Type::Kind::Struct || type.index >= structs.size()) return std::nullopt;
  const auto result = source_type(type, program);
  return result.empty() ? std::nullopt : std::optional<std::string>{result};
}

std::optional<std::string> default_owner_type(const parser::Program& program, const std::string& constructed,
                                             const std::string& owner, const parser::Function* scope) {
  const ActiveTypesGuard restore;
  active_structs = &program.structs; active_enums = &program.enums;
  active_aliases = &program.aliases; active_function_types = nullptr;
  std::vector<Diagnostic> diagnostics;
  const auto structs = build_data_types(program, diagnostics); active_type_info = &structs;
  TypeScope caller; if (scope) { caller = function_scope(*scope, structs, program); active_function_types = &caller; }
  const auto found = std::find_if(structs.begin(), structs.end(), [&](const auto& data) { return data.name == owner; });
  if (found == structs.end()) return std::nullopt;
  const Type viewed = base_view(named_type(constructed, structs), static_cast<std::uint16_t>(found - structs.begin()));
  if (viewed == kError || !diagnostics.empty()) return std::nullopt;
  const auto result = source_type(viewed, program); return result.empty() ? std::nullopt : std::optional<std::string>{result};
}

std::vector<InitializerField> initializer_fields(const parser::Program& program, const std::string& constructed,
                                                const parser::Function* scope) {
  const ActiveTypesGuard restore;
  active_program = &program;
  active_mode = program.mode;
  active_structs = &program.structs; active_enums = &program.enums;
  active_aliases = &program.aliases; active_function_types = nullptr;
  std::vector<Diagnostic> diagnostics;
  const auto structs = build_data_types(program, diagnostics); active_type_info = &structs;
  TypeScope caller;
  if (scope) { caller = function_scope(*scope, structs, program); active_function_types = &caller; }
  const Type data = named_type(constructed, structs);
  if (data.kind != Type::Kind::Struct || data.index >= structs.size()) return {};
  const auto& declaration = program.structs[data.index];
  std::vector<InitializerField> result;
  result.reserve(declaration.fields.size());
  for (std::size_t index = 0; index < declaration.fields.size(); ++index) {
    const auto& field = declaration.fields[index];
    const auto owner = field.owner.empty() ? declaration.name : field.owner;
    auto location = field.name_location;
    for (const auto& original : program.structs) {
      if (original.name != owner) continue;
      for (const auto& candidate : original.fields)
        if (candidate.name == field.name) { location = candidate.name_location; break; }
      break;
    }
    const Type type = specialize_type(structs[data.index].fields[index], structs[data.index], data.parts);
    result.push_back(InitializerField{field.name, source_type(type, program), owner, location, field.is_private});
  }
  return result;
}

std::vector<bool> implicit_null_fields(const parser::Program& program, const std::string& constructed,
                                       const parser::Function* scope) {
  const ActiveTypesGuard restore;
  active_structs = &program.structs; active_enums = &program.enums;
  active_aliases = &program.aliases; active_function_types = nullptr;
  std::vector<Diagnostic> diagnostics;
  const auto structs = build_data_types(program, diagnostics); active_type_info = &structs;
  TypeScope caller;
  if (scope) { caller = function_scope(*scope, structs, program); active_function_types = &caller; }
  const Type data = named_type(constructed, structs);
  if (!diagnostics.empty() || data.kind != Type::Kind::Struct || data.index >= structs.size()) return {};
  std::vector<bool> result;
  result.reserve(structs[data.index].fields.size());
  for (const auto& field : structs[data.index].fields) {
    result.push_back(specialize_type(field, structs[data.index], data.parts).kind == Type::Kind::Optional);
  }
  return result;
}

void Solver::check(const parser::Program& program, std::vector<Diagnostic>& diagnostics) {
  const ActiveTypesGuard restore;
  active_program = &program;
  active_mode = program.mode;
  active_structs = &program.structs;
  active_enums = &program.enums;
  active_aliases = &program.aliases;
  const auto structs = build_data_types(program, diagnostics);
  active_type_info = &structs;
  const auto signatures = build_signatures(program, structs, diagnostics);

  // Every override must preserve the callable contract of its base.
  // Generic formals are compared by parameter position, not source spelling.
  for (const auto& data : program.structs) {
    Type receiver{Type::Kind::Struct, data.index};
    for (std::size_t slot = 0; slot < data.type_params.size(); ++slot) {
      Type formal{Type::Kind::Parameter, static_cast<std::uint16_t>(slot)};
      formal.literal = data.name; receiver.parts.push_back(std::move(formal));
    }
    for (const auto base_index : data.base_indices) {
      if (base_index >= program.structs.size()) continue;
      const auto& base = program.structs[base_index];
      for (std::size_t index = 0; index < base.method_names.size() && index < base.vtable.size(); ++index) {
        const auto slot = std::find(data.method_names.begin(), data.method_names.end(), base.method_names[index]);
        if (slot == data.method_names.end()) continue;
        const auto position = static_cast<std::size_t>(slot - data.method_names.begin());
        if (position >= data.vtable.size() || base.vtable[index] == data.vtable[position]) continue;
        const Signature* expected = nullptr; const Signature* actual = nullptr;
        for (const auto& signature : signatures) {
          if (signature.index == base.vtable[index]) expected = &signature;
          if (signature.index == data.vtable[position]) actual = &signature;
        }
        if (expected != nullptr && actual != nullptr &&
            !method_signature_compatible(*expected, *actual, structs, receiver)) {
          diagnostics.push_back(Diagnostic{data.name_location, data.name_span, "incompatible method override"});
        }
      }
    }
  }

  // Validate every declared default, even if no object uses it.
  std::vector<Binding> definition_scope;
  for (const auto& global : program.globals) definition_scope.push_back(Binding{global.name, named_type(global.type, structs), true});
  for (std::size_t index = 0; index < program.structs.size(); ++index) {
    const auto& fields = program.structs[index].fields;
    for (std::size_t field = 0; field < fields.size(); ++field) {
      if (fields[field].default_value == nullptr) continue;
      const parser::Expr& value = *fields[field].default_value;
      const auto declared = std::find_if(structs.begin(), structs.end(), [&](const auto& data) { return data.name == fields[field].owner; });
      const TypeScope context = declared == structs.end() ? structs[index] : *declared;
      active_function_types = &context;
      Type actual = check_expr(value, definition_scope, signatures, structs, diagnostics);
      if (declared != structs.end()) {
        Type self{Type::Kind::Struct, static_cast<std::uint16_t>(index)};
        for (std::size_t param = 0; param < structs[index].type_params.size(); ++param) {
          Type formal{Type::Kind::Parameter, static_cast<std::uint16_t>(param)}; formal.literal = structs[index].name;
          self.parts.push_back(std::move(formal));
        }
        const auto source = base_view(self, static_cast<std::uint16_t>(declared - structs.begin()));
        actual = specialize_type(actual, context, source.parts);
      }
      if (!contains_parameter(structs[index].fields[field]) &&
          !accepts_value(structs[index].fields[field], actual, value)) mismatch(value, diagnostics);
      active_function_types = nullptr;
    }
  }

  for (const parser::Function& function : program.functions) {
    if (function.index == 65535) {
      continue;
    }
    if (function.is_abstract) continue;
    const Signature* signature = find_signature(signatures, function.name);
    const TypeScope context = function_scope(function, structs, program);
    active_function_types = &context;
    std::vector<Binding> bindings = definition_scope;
    for (std::size_t index = 0; index < function.params.size(); ++index) {
      const bool annotated = index < function.param_types.size() && !function.param_types[index].empty();
      if ((active_mode == parser::CheckMode::Strict || function.anonymous) && !annotated) {
        diagnostics.push_back(Diagnostic{function.param_locations[index], function.param_spans[index], "missing type"});
      }
      const Type type = annotated ? named_type(function.param_types[index], structs) : kAny;
      auto& promotions = const_cast<parser::Function&>(function).param_promotions;
      if (index == 0) promotions.assign(function.params.size(), 0);
      if (type == kFloat) promotions[index] = 1;
      else if (type.kind == Type::Kind::Optional && type.parts.size() == 1 && type.parts[0] == kFloat) promotions[index] = 2;
      bindings.push_back(Binding{function.params[index], type == kError ? kAny : type});
    }
    for (const auto& capture : function.captures) {
      const auto type = named_type(capture.type, structs);
      if (type == kError) diagnostics.push_back(Diagnostic{function.name_location, function.name_span, "capture type could not be resolved"});
      bindings.push_back(Binding{capture.name, type});
    }
    const Type result = signature == nullptr ? kAny : signature->result;
    check_stmts(function.body, bindings, signatures, structs, result, diagnostics);
    if (!function.is_extern && result != kAny && result != kVoid && result != kError && !always_returns(function.body)) {
      diagnostics.push_back(Diagnostic{function.name_location, function.name_span, "missing return"});
    }
  }

  active_function_types = nullptr;
  std::vector<Binding> bindings = definition_scope;
  check_stmts(program.statements, bindings, signatures, structs, kAny, diagnostics);
  active_enums = nullptr;
  active_aliases = nullptr;
  active_structs = nullptr;
  active_mode = parser::CheckMode::Nonstrict;
  active_type_info = nullptr;
}

}  // namespace clpp::typechecker
