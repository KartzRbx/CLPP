#pragma once

#include "clpp/core/lexer/token.hpp"
#include "core/parser/ast.hpp"

#include <vector>
#include <string>
#include <optional>

namespace clpp::typechecker {

struct LocalType {
  std::string name;
  std::string type;
  bool global{false};
};

// Resolve a single expected callback signature without guessing dynamic or ambiguous types.
[[nodiscard]] std::optional<std::string> array_initializer_type(
    const parser::Program& program, const std::string& expected, const parser::Function* scope);

[[nodiscard]] std::optional<std::vector<std::string>> callback_parameters(
    const parser::Program& program, const std::string& expected, const parser::Function* scope);

// Context for arguments, using explicit specialization or evidence from already bound arguments.
[[nodiscard]] std::vector<std::string> argument_types(
    const parser::Program& program, const parser::Expr& expression,
    const std::vector<LocalType>& locals, const parser::Function* scope);

// Query a bound expression using the same semantic types and inference as final checking.
[[nodiscard]] std::optional<std::string> expression_type(const parser::Program& program, const parser::Expr& expression,
                                                        const std::vector<LocalType>& locals,
                                                        const parser::Function* scope = nullptr);

// Query the actual array element, dictionary key or string character type for binding a loop.
[[nodiscard]] std::optional<std::string> iteration_type(const parser::Program& program, const parser::Expr& expression,
                                                       const std::vector<LocalType>& locals,
                                                       const parser::Function* scope = nullptr);

[[nodiscard]] std::optional<std::string> member_owner_type(const parser::Program& program, const std::string& spelling,
                                                          const parser::Function* scope, const std::string& member = {});
[[nodiscard]] std::optional<std::string> resource_type(const parser::Program& program, const std::string& spelling,
                                                      const parser::Function* scope);

[[nodiscard]] std::optional<std::string> data_base_type(const parser::Program& program, const std::string& spelling,
                                                       const parser::StructDecl& owner);

[[nodiscard]] std::optional<std::string> default_owner_type(const parser::Program& program,
    const std::string& constructed, const std::string& owner, const parser::Function* scope);

class Solver {
 public:
  void check(const parser::Program& program, std::vector<Diagnostic>& diagnostics);
};

}  // namespace clpp::typechecker
