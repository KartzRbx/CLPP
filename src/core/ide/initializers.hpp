#pragma once

#include "core/parser/ast.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace clpp::ide::detail {

// Lexical slots survive a half-written initializer that the parser cannot retain.
// Values, calls, generic arguments and nested blocks each get their own frame.
struct InitializerIndex {
  static constexpr std::size_t none = static_cast<std::size_t>(-1);
  struct Context {
    std::string type;
    const parser::Function* scope{nullptr};
    std::unordered_map<std::string, std::size_t> used;
  };
  struct Position { std::size_t context{none}; std::size_t key{none}; };
  std::vector<Context> contexts;
  std::vector<std::size_t> keys;
  std::vector<std::size_t> slots;

  InitializerIndex(const parser::Program& program, const std::vector<Token>& tokens)
      : keys(tokens.size(), none), slots(tokens.size(), none) {
    std::unordered_set<std::string> types, generic_names;
    for (const auto& data : program.structs) types.insert(data.name);
    for (const auto& alias : program.aliases) types.insert(alias.name);
    for (const auto& name : program.resolved_types) types.insert(name.spelling);
    generic_names = types;
    for (const auto& function : program.functions) generic_names.insert(function.name);
    for (const auto& name : program.resolved_functions) generic_names.insert(name.spelling);
    for (const char* name : {"array", "dict", "optional", "task", "tuple", "signal", "number", "float", "string", "bool"})
      generic_names.insert(name);

    const auto qualified_start = [&](std::size_t index) {
      while (index >= 2 && tokens[index - 1].type == TokenType::Dot &&
             tokens[index - 2].type == TokenType::Identifier) index -= 2;
      return index;
    };
    const auto spelling = [&](std::size_t begin, std::size_t end) {
      std::string name;
      for (std::size_t index = begin; index <= end; ++index) name += tokens[index].lexeme;
      return name;
    };
    const auto constructed_type = [&](std::size_t open) -> std::string {
      if (open == 0) return {};
      std::size_t name = open - 1;
      if (tokens[name].type == TokenType::Greater) {
        std::size_t depth = 1;
        while (name > 0 && depth != 0) {
          --name;
          if (tokens[name].type == TokenType::Greater) ++depth;
          else if (tokens[name].type == TokenType::Less) --depth;
        }
        if (depth != 0 || name == 0) return {};
        --name;
      }
      if (tokens[name].type != TokenType::Identifier) return {};
      const auto begin = qualified_start(name);
      if (!types.contains(spelling(begin, name))) return {};
      if (begin > 0 && tokens[begin - 1].type == TokenType::Arrow) return {}; // lambda body
      for (std::size_t index = begin; index > 0;) {
        const auto type = tokens[--index].type;
        if (type == TokenType::KwDef || type == TokenType::KwEnum) return {};
        if (type == TokenType::Semicolon || type == TokenType::OpenBrace || type == TokenType::CloseBrace) break;
      }
      return spelling(begin, open - 1);
    };

    const auto location_key = [](const SourceLocation location) {
      return (static_cast<std::uint64_t>(location.line) << 32) | location.column;
    };
    std::unordered_map<std::uint64_t, const parser::Function*> functions;
    for (const auto& function : program.functions)
      if (!function.imported && !function.module_init && function.name_location.line != 0)
        functions.emplace(location_key(function.name_location), &function);

    struct Frame {
      TokenType kind;
      std::size_t context{none};
      const parser::Function* scope{nullptr};
      bool key_slot{false};
    };
    std::vector<Frame> frames;
    const parser::Function* pending_function = nullptr;
    for (std::size_t index = 0; index < tokens.size(); ++index) {
      const auto& token = tokens[index];
      if (const auto function = functions.find(location_key(token.location)); function != functions.end())
        pending_function = function->second;
      if (!frames.empty() && frames.back().context != none && frames.back().key_slot) {
        const auto context = frames.back().context;
        slots[index] = context;
        if (token.type == TokenType::Identifier) {
          keys[index] = context;
          if (index + 1 < tokens.size() && tokens[index + 1].type == TokenType::Equal)
            ++contexts[context].used[std::string(token.lexeme)];
          frames.back().key_slot = false;
        }
      }
      if (token.type == TokenType::OpenBrace || token.type == TokenType::OpenParen || token.type == TokenType::OpenBracket) {
        const auto inherited_scope = frames.empty() ? nullptr : frames.back().scope;
        Frame frame{token.type, none, inherited_scope, false};
        if (token.type == TokenType::OpenBrace) {
          if (pending_function) { frame.scope = pending_function; pending_function = nullptr; }
          auto type = constructed_type(index);
          if (!type.empty()) {
            frame.context = contexts.size();
            frame.key_slot = true;
            contexts.push_back(Context{std::move(type), frame.scope, {}});
          }
        }
        frames.push_back(frame);
      } else if (token.type == TokenType::Less && index > 0) {
        const auto begin = qualified_start(index - 1);
        const bool nested = !frames.empty() && frames.back().kind == TokenType::Less;
        const bool type_test = tokens[index - 1].lexeme == "is" && index >= 2 && tokens[index - 2].type == TokenType::Colon;
        if (nested || type_test || generic_names.contains(spelling(begin, index - 1)))
          frames.push_back(Frame{TokenType::Less, none, frames.empty() ? nullptr : frames.back().scope, false});
      } else if (token.type == TokenType::Greater) {
        if (!frames.empty() && frames.back().kind == TokenType::Less) frames.pop_back();
      } else if (token.type == TokenType::CloseBrace || token.type == TokenType::CloseParen || token.type == TokenType::CloseBracket) {
        const auto open = token.type == TokenType::CloseBrace ? TokenType::OpenBrace :
                          token.type == TokenType::CloseParen ? TokenType::OpenParen : TokenType::OpenBracket;
        for (std::size_t depth = frames.size(); depth > 0; --depth)
          if (frames[depth - 1].kind == open) { frames.resize(depth - 1); break; }
      } else if (token.type == TokenType::Comma && !frames.empty() && frames.back().context != none) {
        frames.back().key_slot = true;
      } else if (token.type == TokenType::Semicolon) {
        pending_function = nullptr;
      }
    }
  }

  [[nodiscard]] Position at(const std::vector<Token>& tokens, std::uint32_t line, std::uint32_t column) const {
    for (std::size_t index = 0; index < tokens.size(); ++index) {
      const auto& token = tokens[index];
      if (keys[index] != none && token.location.line == line && column >= token.location.column &&
          column <= token.location.column + token.lexeme.size()) return Position{keys[index], index};
      if (token.location.line > line || (token.location.line == line && token.location.column >= column))
        return Position{slots[index], none};
    }
    return {};
  }
};

}  // namespace clpp::ide::detail
