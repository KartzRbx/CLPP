#include "core/typechecker/solver.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace clpp::typechecker {

namespace {

const std::vector<parser::EnumDecl>* active_enums = nullptr;
const std::vector<parser::VariantDecl>* active_variants = nullptr;
const std::vector<parser::TypeAlias>* active_aliases = nullptr;
const std::vector<parser::StructDecl>* active_structs = nullptr;
parser::CheckMode active_mode = parser::CheckMode::Nonstrict;

struct Type {
  enum class Kind {
    Any, Void, Int, Float, Bool, String, Vector3, Buffer, Struct, Enum, Variant, Task, Literal, Union, Intersect, Error
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
    if (left.kind == Kind::Struct || left.kind == Kind::Enum || left.kind == Kind::Variant) {
      return left.index == right.index && left.parts == right.parts;
    }
    if (left.kind == Kind::Literal) {
      return left.literal == right.literal;
    }
    if (left.kind == Kind::Union || left.kind == Kind::Intersect) {
      return left.parts == right.parts;
    }
    if (left.kind == Kind::Task) {
      return left.payload == right.payload && (left.payload != Kind::Struct || left.index == right.index);
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
const Type kError{Type::Kind::Error, 0};

struct Binding {
  std::string name;
  Type type{kAny};
};

struct Signature {
  std::string name;
  std::vector<Type> params;
  Type result{kAny};
  bool is_async{false};
  std::vector<std::string> type_params;
  std::vector<std::string> where_names;
  std::vector<std::string> where_types;
};

[[nodiscard]] Type task_of(const Type inner) {
  Type type;
  type.kind = Type::Kind::Task;
  type.payload = inner.kind;
  type.index = inner.index;
  return type;
}

[[nodiscard]] Type unwrap_task(const Type task) {
  Type inner;
  inner.kind = task.payload;
  inner.index = task.index;
  return inner;
}

struct StructInfo {
  std::string name;
  std::vector<Type> fields;
  std::vector<std::string> field_types;
  std::vector<std::string> type_params;
  bool is_abstract{false};
};

[[nodiscard]] std::vector<std::string_view> split_type(const std::string_view text, const char separator) {
  std::vector<std::string_view> parts;
  std::size_t start = 0;
  bool quoted = false;
  for (std::size_t index = 0; index < text.size(); ++index) {
    if (text[index] == '"') {
      quoted = !quoted;
    } else if (!quoted && text[index] == separator) {
      parts.push_back(text.substr(start, index - start));
      start = index + 1;
    }
  }
  parts.push_back(text.substr(start));
  return parts;
}

[[nodiscard]] Type named_type(const std::string_view name, const std::vector<StructInfo>& structs, const int depth = 0) {
  if (depth > 8) {
    return kError;
  }
  if (name.empty()) {
    return kAny;
  }
  if (name.find('|') != std::string_view::npos && name.find('&') != std::string_view::npos) {
    return kError;
  }
  if (name.find('|') != std::string_view::npos || name.find('&') != std::string_view::npos) {
    const char separator = name.find('|') != std::string_view::npos ? '|' : '&';
    Type combined;
    combined.kind = separator == '|' ? Type::Kind::Union : Type::Kind::Intersect;
    for (const std::string_view part : split_type(name, separator)) {
      const Type inner = named_type(part, structs, depth + 1);
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
  if (name == "int") {
    return kInt;
  }
  if (name == "float" || name == "double") {
    return kFloat;
  }
  if (name == "bool") {
    return kBool;
  }
  if (name == "task") {
    return task_of(kAny);
  }
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
    std::vector<std::string_view> pieces;
    std::size_t start = 0;
    int nest = 0;
    for (std::size_t index = 0; index <= args.size(); ++index) {
      if (index == args.size() || (args[index] == ',' && nest == 0)) {
        std::string_view piece = args.substr(start, index - start);
        while (!piece.empty() && piece.front() == ' ') {
          piece.remove_prefix(1);
        }
        if (!piece.empty()) {
          pieces.push_back(piece);
        }
        start = index + 1;
      } else if (args[index] == '<') {
        ++nest;
      } else if (args[index] == '>') {
        --nest;
      }
    }
    if (bare == "array" && pieces.size() == 1) {
      Type list{Type::Kind::Struct, 65532};
      list.parts.push_back(named_type(pieces[0], structs, depth + 1));
      return list;
    }
    if (bare == "dictionary" && pieces.size() == 2) {
      Type table{Type::Kind::Struct, 65531};
      table.parts.push_back(named_type(pieces[0], structs, depth + 1));
      table.parts.push_back(named_type(pieces[1], structs, depth + 1));
      return table;
    }
    if (bare == "Option" && pieces.size() == 1) {
      Type option{Type::Kind::Enum, 65534};
      option.parts.push_back(named_type(pieces[0], structs, depth + 1));
      return option;
    }
    if (bare == "Result" && pieces.size() == 2) {
      Type result{Type::Kind::Enum, 65533};
      result.parts.push_back(named_type(pieces[0], structs, depth + 1));
      result.parts.push_back(named_type(pieces[1], structs, depth + 1));
      return result;
    }
    for (std::size_t index = 0; index < structs.size(); ++index) {
      if (structs[index].name == bare) {
        Type generic_type{Type::Kind::Struct, static_cast<std::uint16_t>(index)};
        for (const std::string_view piece : pieces) {
          generic_type.parts.push_back(named_type(piece, structs, depth + 1));
        }
        return generic_type;
      }
    }
  }
  for (std::size_t index = 0; index < structs.size(); ++index) {
    if (structs[index].name == name) {
      return Type{Type::Kind::Struct, static_cast<std::uint16_t>(index)};
    }
  }
  if (active_variants != nullptr) {
    for (std::size_t index = 0; index < active_variants->size(); ++index) {
      if ((*active_variants)[index].name == name) {
        return Type{Type::Kind::Variant, static_cast<std::uint16_t>(index)};
      }
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
        return named_type(alias.body, structs, depth + 1);
      }
    }
  }
  return kError;
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

[[nodiscard]] bool assignable(const Type destination, const Type source) {
  if (destination == kAny || source == kAny || destination == source) {
    return true;
  }
  if (destination.kind == Type::Kind::Struct && source.kind == Type::Kind::Struct &&
      derives_from(source.index, destination.index)) {
    return true;
  }
  if (destination.kind == Type::Kind::Union) {
    for (const Type& part : destination.parts) {
      if (assignable(part, source)) {
        return true;
      }
    }
    return false;
  }
  if (destination.kind == Type::Kind::Intersect) {
    for (const Type& part : destination.parts) {
      if (!assignable(part, source)) {
        return false;
      }
    }
    return !destination.parts.empty();
  }
  if (destination == kString && source.kind == Type::Kind::Literal) {
    return true;
  }
  if (destination.kind == Type::Kind::Task && source.kind == Type::Kind::Task && destination.payload == Type::Kind::Any) {
    return true;
  }
  if ((destination.kind == Type::Kind::Enum || destination.kind == Type::Kind::Struct) && destination.kind == source.kind &&
      destination.index == source.index && destination.parts.size() == source.parts.size() && !destination.parts.empty()) {
    for (std::size_t index = 0; index < destination.parts.size(); ++index) {
      if (!assignable(destination.parts[index], source.parts[index])) {
        return false;
      }
    }
    return true;
  }
  return destination == kFloat && source == kInt;
}

[[nodiscard]] bool is_text(const Type type) {
  return type == kString || type == kAny || type.kind == Type::Kind::Literal || type.kind == Type::Kind::Union;
}

[[nodiscard]] bool is_string_value(const Type type) {
  return type == kString || type == kAny || type.kind == Type::Kind::Literal;
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

[[nodiscard]] Type find_binding(const std::vector<Binding>& bindings, const std::string_view name) {
  for (const Binding& binding : bindings) {
    if (binding.name == name) {
      return binding.type;
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

[[nodiscard]] Type check_expr(const parser::Expr& expr, const std::vector<Binding>& bindings,
                              const std::vector<Signature>& signatures, const std::vector<StructInfo>& structs,
                              std::vector<Diagnostic>& diagnostics);

[[nodiscard]] Type check_numeric_binary(const parser::Expr& expr, const Type left, const Type right,
                                        const bool allow_vector, std::vector<Diagnostic>& diagnostics) {
  if (allow_vector && left == kVector3 && right == kVector3) {
    return kVector3;
  }
  if (allow_vector && ((left == kVector3 && is_numeric(right)) || (is_numeric(left) && right == kVector3))) {
    return kVector3;
  }
  if (is_numeric(left) && is_numeric(right)) {
    if (left == kFloat || right == kFloat) {
      return kFloat;
    }
    return kInt;
  }
  mismatch(expr, diagnostics);
  return kError;
}

[[nodiscard]] Type check_expr(const parser::Expr& expr, const std::vector<Binding>& bindings,
                              const std::vector<Signature>& signatures, const std::vector<StructInfo>& structs,
                              std::vector<Diagnostic>& diagnostics) {
  switch (expr.kind) {
    case parser::Expr::Kind::String: {
      Type literal;
      literal.kind = Type::Kind::Literal;
      literal.literal = expr.value;
      return literal;
    }
    case parser::Expr::Kind::Number:
      if (expr.null_literal) {
        return kAny;
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
    case parser::Expr::Kind::Name: {
      const Type type = find_binding(bindings, expr.value);
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
      const Type object = expr.left == nullptr ? kError : check_expr(*expr.left, bindings, signatures, structs, diagnostics);
      if (object.kind == Type::Kind::Struct) {
        if (object.index >= structs.size() || expr.slot >= structs[object.index].fields.size()) {
          mismatch(expr, diagnostics);
          return kError;
        }
        return structs[object.index].fields[expr.slot];
      }
      if (object != kVector3 && object != kAny) {
        mismatch(expr, diagnostics);
        return kError;
      }
      return kFloat;
    }
    case parser::Expr::Kind::MoveFrom: {
      const Type type = find_binding(bindings, expr.value);
      if (type == kError) {
        mismatch(expr, diagnostics);
        return kError;
      }
      return type;
    }
    case parser::Expr::Kind::Construct: {
      if (expr.payload_ctor) {
        Type payload = kAny;
        if (expr.args.size() >= 2) {
          payload = check_expr(expr.args[1], bindings, signatures, structs, diagnostics);
        }
        Type result{Type::Kind::Enum, expr.slot};
        if (expr.slot == 65534) {
          result.parts.push_back(payload);
        } else if (expr.slot == 65533) {
          if (expr.args.size() >= 1 && expr.args[0].number == 0) {
            result.parts.push_back(kAny);
            result.parts.push_back(payload);
          } else {
            result.parts.push_back(payload);
            result.parts.push_back(kAny);
          }
        }
        return result;
      }
      if (expr.variant_ctor) {
        if (active_variants == nullptr || expr.slot >= active_variants->size() || expr.args.size() != 1) {
          mismatch(expr, diagnostics);
          return kError;
        }
        const Type arg = check_expr(expr.args[0], bindings, signatures, structs, diagnostics);
        bool matched = false;
        for (const std::string& alternative : (*active_variants)[expr.slot].alternatives) {
          if (assignable(named_type(alternative, structs), arg)) {
            matched = true;
            break;
          }
        }
        if (!matched && arg != kError) {
          mismatch(expr.args[0], diagnostics);
        }
        return Type{Type::Kind::Variant, expr.slot};
      }
      if (expr.slot >= structs.size()) {
        mismatch(expr, diagnostics);
        return kError;
      }
      const std::vector<Type>& fields = structs[expr.slot].fields;
      std::vector<Type> supplied;
      const std::size_t open = expr.value.find('<');
      if (open != std::string::npos && expr.value.back() == '>') {
        const Type generic = named_type(expr.value, structs);
        supplied = generic.parts;
      }
      for (std::size_t index = 0; index < expr.args.size() && index < fields.size(); ++index) {
        const Type arg = check_expr(expr.args[index], bindings, signatures, structs, diagnostics);
        Type expected = fields[index];
        if (index < structs[expr.slot].field_types.size()) {
          const std::string& field_type = structs[expr.slot].field_types[index];
          for (std::size_t param = 0; param < structs[expr.slot].type_params.size(); ++param) {
            if (structs[expr.slot].type_params[param] == field_type && param < supplied.size()) {
              expected = supplied[param];
            }
          }
        }
        if (!assignable(expected, arg)) {
          mismatch(expr.args[index], diagnostics);
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
      std::vector<Type> args;
      for (const parser::Expr& arg : expr.args) {
        args.push_back(check_expr(arg, bindings, signatures, structs, diagnostics));
      }
      if (signature == nullptr) {
        return kAny;
      }
      if (open != std::string::npos && expr.value.back() == '>') {
        const std::string supplied = expr.value.substr(open + 1, expr.value.size() - open - 2);
        std::vector<std::string> pieces;
        std::size_t start = 0;
        int depth = 0;
        for (std::size_t index = 0; index <= supplied.size(); ++index) {
          if (index == supplied.size() || (supplied[index] == ',' && depth == 0)) {
            std::string piece = supplied.substr(start, index - start);
            while (!piece.empty() && piece.front() == ' ') {
              piece.erase(piece.begin());
            }
            if (!piece.empty()) {
              pieces.push_back(std::move(piece));
            }
            start = index + 1;
          } else if (supplied[index] == '<') {
            ++depth;
          } else if (supplied[index] == '>') {
            --depth;
          }
        }
        for (std::size_t constraint = 0; constraint < signature->where_names.size() && constraint < signature->where_types.size();
             ++constraint) {
          const auto param = std::find(signature->type_params.begin(), signature->type_params.end(),
                                       signature->where_names[constraint]);
          if (param == signature->type_params.end()) {
            continue;
          }
          const std::size_t param_index = static_cast<std::size_t>(param - signature->type_params.begin());
          if (param_index >= pieces.size()) {
            continue;
          }
          if (!assignable(named_type(signature->where_types[constraint], structs), named_type(pieces[param_index], structs))) {
            mismatch(expr, diagnostics);
          }
        }
      }
      for (std::size_t index = 0; index < args.size() && index < signature->params.size(); ++index) {
        if (!assignable(signature->params[index], args[index])) {
          mismatch(expr.args[index], diagnostics);
        }
      }
      if (expr.thread_call) {
        return kInt;
      }
      return signature->is_async ? task_of(signature->result) : signature->result;
    }
    case parser::Expr::Kind::BitAnd:
    case parser::Expr::Kind::BitOr:
    case parser::Expr::Kind::BitXor:
    case parser::Expr::Kind::Shl:
    case parser::Expr::Kind::Shr:
    case parser::Expr::Kind::BitNot:
    case parser::Expr::Kind::Update:
      if (expr.left != nullptr) {
        (void)check_expr(*expr.left, bindings, signatures, structs, diagnostics);
      }
      if (expr.right != nullptr) {
        (void)check_expr(*expr.right, bindings, signatures, structs, diagnostics);
      }
      return kInt;
    case parser::Expr::Kind::Range:
      if (expr.left != nullptr) {
        (void)check_expr(*expr.left, bindings, signatures, structs, diagnostics);
      }
      if (expr.right != nullptr) {
        (void)check_expr(*expr.right, bindings, signatures, structs, diagnostics);
      }
      return kAny;
    case parser::Expr::Kind::Ternary: {
      if (expr.left != nullptr) {
        (void)check_expr(*expr.left, bindings, signatures, structs, diagnostics);
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
      if (inner != kBool && !is_numeric(inner)) {
        mismatch(expr, diagnostics);
        return kError;
      }
      return kBool;
    }
    case parser::Expr::Kind::And:
    case parser::Expr::Kind::Or: {
      const Type left = expr.left == nullptr ? kError : check_expr(*expr.left, bindings, signatures, structs, diagnostics);
      const Type right = expr.right == nullptr ? kError : check_expr(*expr.right, bindings, signatures, structs, diagnostics);
      if ((left != kBool && !is_numeric(left)) || (right != kBool && !is_numeric(right))) {
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
      if (!is_numeric(left) || !is_numeric(right)) {
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
      if (expr.kind == parser::Expr::Kind::Div && left == kVector3 && is_numeric(right)) {
        return kVector3;  // velocity / 2
      }
      return check_numeric_binary(expr, left, right, false, diagnostics);
    }
    case parser::Expr::Kind::Index: {
      const Type object = expr.left == nullptr ? kError : check_expr(*expr.left, bindings, signatures, structs, diagnostics);
      const bool slice = expr.right != nullptr && expr.right->kind == parser::Expr::Kind::Range;
      const Type index = expr.right == nullptr ? kError : check_expr(*expr.right, bindings, signatures, structs, diagnostics);
      const bool table = object.kind == Type::Kind::Struct && (object.index == 65531 || object.index == 65532);
      if (object != kError && object != kAny && object != kVector3 && !is_string_value(object) && !table) {
        mismatch(expr, diagnostics);
      }
      // Untyped values (`any`: JSON documents, lists from functions) may be dictionaries, so a
      // string key is fine there: `data["name"]`.
      if (!slice && !is_numeric(index) && index != kError && object != kAny &&
          !(table && object.index == 65531 && is_string_value(index))) {
        mismatch(expr, diagnostics);
      }
      if (object == kAny) {
        return kAny;  // an untyped list/value: the element can be anything
      }
      if (is_string_value(object)) {
        return kString;  // "abc"[1] is "b"
      }
      if (table && object.index == 65532 && !object.parts.empty()) {
        return object.parts[0];
      }
      if (table && object.index == 65531 && object.parts.size() > 1) {
        return object.parts[1];
      }
      return slice ? kAny : kInt;
    }
    case parser::Expr::Kind::MakeList: {
      // Lists hold any value: numbers, strings, structs, other lists.
      for (const parser::Expr& arg : expr.args) {
        (void)check_expr(arg, bindings, signatures, structs, diagnostics);
      }
      return kAny;
    }
    case parser::Expr::Kind::Sort: {
      if (!expr.args.empty()) {
        (void)check_expr(expr.args[0], bindings, signatures, structs, diagnostics);
      }
      return kAny;
    }
    case parser::Expr::Kind::ListMut: {
      for (std::size_t index = 0; index < expr.args.size(); ++index) {
        const Type type = check_expr(expr.args[index], bindings, signatures, structs, diagnostics);
        if (index == 0 && (type == kInt || type == kFloat || type == kBool)) {
          mismatch(expr.args[0], diagnostics);
        }
        const bool index_arg = (expr.number == 2 && index == 1);
        if (index_arg && type != kInt && type != kFloat && type != kAny && type != kError) {
          mismatch(expr.args[index], diagnostics);
        }
      }
      return kAny;
    }
    case parser::Expr::Kind::Len: {
      if (!expr.args.empty()) {
        const Type type = check_expr(expr.args[0], bindings, signatures, structs, diagnostics);
        if (type == kInt || type == kFloat) {
          mismatch(expr.args[0], diagnostics);
        }
      }
      return kInt;
    }
    case parser::Expr::Kind::Find: {
      for (const parser::Expr& arg : expr.args) {
        (void)check_expr(arg, bindings, signatures, structs, diagnostics);  // find works on any element type
      }
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
  if (type != kBool && !is_numeric(type) && type != kError) {
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
        bool all = !stmt.arms.empty();
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
    if (stmt.kind == parser::Stmt::Kind::Let && stmt.expr.missing) {
      const Type declared = stmt.declared_type.empty() ? kAny : named_type(stmt.declared_type, structs);
      bindings.push_back(Binding{stmt.name, declared == kError ? kAny : declared});
      continue;
    }
    if (stmt.kind == parser::Stmt::Kind::Let) {
      const Type value = check_expr(stmt.expr, bindings, signatures, structs, diagnostics);
      const Type declared = stmt.has_decltype ? check_expr(stmt.type_expr, bindings, signatures, structs, diagnostics)
                                              : named_type(stmt.declared_type, structs);
      if (declared == kError) {
        diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "unknown type"});
      } else if (!assignable(declared, value)) {
        mismatch(stmt.expr, diagnostics);
      }
      Type stored = declared == kAny ? value : declared;
      // `let mut name = "Ada";` holds a string, not the literal type "Ada": otherwise every later
      // assignment (`name = "Bob";`) was a type mismatch. Literal types stay for immutable
      // bindings and for explicitly declared unions (`let state: State = "Idle";`).
      if (declared == kAny && !stmt.immutable && stored.kind == Type::Kind::Literal) {
        stored = kString;
      }
      if (active_mode == parser::CheckMode::Strict && stmt.declared_type.empty() && !stmt.has_decltype && stored == kAny) {
        diagnostics.push_back(Diagnostic{stmt.name_location, stmt.name_span, "missing type"});
      }
      if (stmt.unpack.size() > 1) {
        for (const std::string& name : stmt.unpack) {
          bindings.push_back(Binding{name, kAny});
        }
      } else {
        bindings.push_back(Binding{stmt.name, stored == kError ? kAny : stored});
      }
      continue;
    }
    if (stmt.kind == parser::Stmt::Kind::PlaceAssign) {
      const Type value = check_expr(stmt.expr, bindings, signatures, structs, diagnostics);
      const Type destination = check_expr(stmt.place, bindings, signatures, structs, diagnostics);
      if (destination != kError && !assignable(destination, value)) {
        mismatch(stmt.expr, diagnostics);
      }
      continue;
    }
    if (stmt.kind == parser::Stmt::Kind::Assign) {
      const Type value = check_expr(stmt.expr, bindings, signatures, structs, diagnostics);
      const Type destination = find_binding(bindings, stmt.name);
      if (destination != kError && !assignable(destination, value)) {
        mismatch(stmt.expr, diagnostics);
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
      if (!assignable(return_type, value)) {
        mismatch(stmt.expr, diagnostics);
      }
      continue;
    }
    if (stmt.kind == parser::Stmt::Kind::If || stmt.kind == parser::Stmt::Kind::While) {
      check_condition(stmt.expr, bindings, signatures, structs, diagnostics);
      const std::size_t mark = bindings.size();
      if (stmt.if_let) {
        bindings.push_back(Binding{stmt.name, kAny});
      }
      check_stmts(stmt.then_body, bindings, signatures, structs, return_type, diagnostics);
      bindings.resize(mark);
      if (stmt.has_else) {
        check_stmts(stmt.else_body, bindings, signatures, structs, return_type, diagnostics);
      }
      continue;
    }
    if (stmt.kind == parser::Stmt::Kind::Break || stmt.kind == parser::Stmt::Kind::Signal ||
        stmt.kind == parser::Stmt::Kind::Connect || stmt.kind == parser::Stmt::Kind::Using) {
      continue;
    }
    if (stmt.kind == parser::Stmt::Kind::ForIn) {
      // `for (let i in 10)` counts 0..9; `for (let item in list)` walks the elements.
      const Type source = check_expr(stmt.expr, bindings, signatures, structs, diagnostics);
      const bool numeric = source == kInt || source == kFloat;
      const_cast<parser::Stmt&>(stmt).numeric_range = numeric;
      Type item = numeric ? kInt : source == kString ? kString : kAny;
      if (source.kind == Type::Kind::Struct && source.index == 65532 && !source.parts.empty()) {
        item = source.parts[0];  // array<T>: the element type
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
        if (init.kind == parser::Stmt::Kind::Let) {
          const Type value = check_expr(init.expr, bindings, signatures, structs, diagnostics);
          const Type declared = named_type(init.declared_type, structs);
          if (declared == kError) {
            diagnostics.push_back(Diagnostic{init.name_location, init.name_span, "unknown type"});
          } else if (!assignable(declared, value)) {
            mismatch(init.expr, diagnostics);
          }
          const Type stored = declared == kAny ? value : declared;
          bindings.push_back(Binding{init.name, stored == kError ? kAny : stored});
        } else {
          const Type value = check_expr(init.expr, bindings, signatures, structs, diagnostics);
          const Type destination = find_binding(bindings, init.name);
          if (destination != kError && !assignable(destination, value)) {
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
      std::vector<char> covered;
      if (enum_match && active_enums != nullptr && scrutinee.index < active_enums->size()) {
        covered.assign((*active_enums)[scrutinee.index].variants.size(), 0);
      } else if (enum_match && (scrutinee.index == 65534 || scrutinee.index == 65533)) {
      } else if (!is_numeric(scrutinee) && !is_string_value(scrutinee) && scrutinee != kBool && scrutinee != kError) {
        mismatch(stmt.expr, diagnostics);
      }
      // Text is matched against text ("play" ~> ...), numbers and bools against numbers and bools.
      const bool text_match = !enum_match && scrutinee != kAny && is_string_value(scrutinee);
      bool wildcard = false;
      for (const parser::Stmt::MatchArm& arm : stmt.arms) {
        if (arm.wildcard) {
          wildcard = true;
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
              covered[static_cast<std::size_t>(arm.pattern.number)] = 1;
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
      if (enum_match && !wildcard) {
        for (const char seen : covered) {
          if (seen == 0) {
            diagnostics.push_back(Diagnostic{stmt.expr.location, stmt.expr.span, "non-exhaustive match"});
            break;
          }
        }
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

}  // namespace

void Solver::check(const parser::Program& program, std::vector<Diagnostic>& diagnostics) {
  if (program.mode == parser::CheckMode::Nocheck) {
    return;
  }
  active_mode = program.mode;
  active_structs = &program.structs;
  active_enums = &program.enums;
  active_variants = &program.variants;
  active_aliases = &program.aliases;
  std::vector<StructInfo> structs(program.structs.size());
  for (std::size_t index = 0; index < program.structs.size(); ++index) {
    structs[index].name = program.structs[index].name;
  }
  for (std::size_t index = 0; index < program.structs.size(); ++index) {
    structs[index].type_params = program.structs[index].type_params;
    structs[index].is_abstract = program.structs[index].is_abstract;
    for (const parser::Field& field : program.structs[index].fields) {
      const bool parameter = std::find(structs[index].type_params.begin(), structs[index].type_params.end(),
                                       field.type_name) != structs[index].type_params.end();
      Type field_type = parameter ? kAny : named_type(field.type_name, structs);
      if (field_type == kError) {
        diagnostics.push_back(Diagnostic{field.name_location, field.name_span, "unknown type"});
        field_type = kAny;
      }
      structs[index].fields.push_back(field_type);
      structs[index].field_types.push_back(field.type_name);
    }
  }

  std::vector<Signature> signatures;
  for (const parser::Function& function : program.functions) {
    if (function.index == 65535) {
      continue;
    }
    Signature signature;
    signature.name = function.name;
    signature.is_async = function.is_async;
    signature.type_params = function.type_params;
    signature.where_names = function.where_names;
    signature.where_types = function.where_types;
    signature.result = named_type(function.return_type, structs);
    for (const std::string& param_type : function.param_types) {
      const bool parameter = std::find(function.type_params.begin(), function.type_params.end(), param_type) !=
                             function.type_params.end();
      const Type type = parameter ? kAny : named_type(param_type, structs);
      if (!parameter && type == kError) {
        diagnostics.push_back(Diagnostic{function.name_location, function.name_span, "unknown type"});
        signature.params.push_back(kAny);
      } else {
        signature.params.push_back(type);
      }
    }
    signatures.push_back(std::move(signature));
  }

  for (const parser::Function& function : program.functions) {
    if (function.index == 65535) {
      continue;
    }
    const Signature* signature = find_signature(signatures, function.name);
    std::vector<Binding> bindings;
    for (std::size_t index = 0; index < function.params.size(); ++index) {
      const bool annotated = index < function.param_types.size() && !function.param_types[index].empty();
      if (active_mode == parser::CheckMode::Strict && !annotated) {
        diagnostics.push_back(Diagnostic{function.param_locations[index], function.param_spans[index], "missing type"});
      }
      const Type type = annotated ? named_type(function.param_types[index], structs) : kAny;
      bindings.push_back(Binding{function.params[index], type == kError ? kAny : type});
    }
    const Type result = signature == nullptr ? kAny : signature->result;
    check_stmts(function.body, bindings, signatures, structs, result, diagnostics);
    if (!function.is_extern && result != kAny && result != kVoid && result != kError && !always_returns(function.body)) {
      diagnostics.push_back(Diagnostic{function.name_location, function.name_span, "missing return"});
    }
  }

  std::vector<Binding> bindings;
  check_stmts(program.statements, bindings, signatures, structs, kAny, diagnostics);
  active_enums = nullptr;
  active_variants = nullptr;
  active_aliases = nullptr;
  active_structs = nullptr;
  active_mode = parser::CheckMode::Nonstrict;
}

}  // namespace clpp::typechecker
