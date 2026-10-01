#pragma once

#include "clpp/compiler.hpp"
#include "clpp/register_ir.hpp"
#include "clpp/value.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace clpp {

class VirtualMachine {
 public:
  void set_output(std::ostream& out);
  // Stream for warn(); defaults to std::cerr.
  void set_error_output(std::ostream& err);
  void isolate();
  // Nested calls allowed before "stack overflow" (default 200000). Frames live on the heap.
  void set_max_call_depth(std::size_t depth);
  void load(const BytecodeChunk& chunk);
  [[nodiscard]] bool run();
  [[nodiscard]] std::string_view error() const;
  // Source position of the instruction that failed (line 0 when unknown).
  [[nodiscard]] SourceLocation error_location() const { return m_error_location; }

 private:
  struct CallFrame {
    std::size_t ip{0};
    std::vector<Value> stack;
    std::vector<Value> locals;
    std::size_t base{0};
    std::uint16_t arity{0};
    bool co_root{false};
  };

  struct Coroutine {
    bool started{false};
    bool dead{false};
    std::uint16_t function{0};
    std::size_t ip{0};
    std::vector<Value> stack;
    std::vector<CallFrame> frames;
  };

  struct ResumePoint {
    bool live{false};
    std::size_t ip{0};
    std::vector<Value> stack;
    std::vector<CallFrame> frames;
    std::size_t base{0};
  };

  struct Trap {
    std::uint16_t ip{0};
    std::size_t depth{0};
    std::size_t frames{0};
  };

  [[nodiscard]] bool read_u16(std::uint16_t& index);
  [[nodiscard]] bool recover();
  [[nodiscard]] bool execute_registers(const RegChunk& lowered);
  [[nodiscard]] bool run_function(std::uint16_t index, const std::vector<Value>& args, Value& out);

  void collect_garbage();
  [[nodiscard]] std::vector<Value> take_frame(const RegChunk& lowered, std::size_t function);
  void give_frame(std::vector<Value>&& frame);

  BytecodeChunk m_chunk;
  std::vector<CallFrame> m_calls;
  std::vector<Trap> m_traps;
  std::size_t m_ip{0};
  std::vector<Value> m_stack;
  std::vector<Value> m_locals;
  Value m_self;
  std::vector<std::unique_ptr<Table>> m_heap;
  std::ostream* m_out{nullptr};
  std::ostream* m_err{&std::cerr};
  std::string m_error;
  std::string m_recovered;
  bool m_isolated{false};
  std::vector<Coroutine> m_coroutines;
  std::vector<std::uint16_t> m_deferred;
  ResumePoint m_resumer;
  int m_active{-1};
  std::size_t m_max_call_depth{200000};
  SourceLocation m_error_location{};
  void locate_error(const RegChunk& lowered);
  std::vector<std::vector<Value>> m_frame_pool;
  // Methods that change self hand the final self back to the caller (MarkSelf -> SelfBack).
  Value m_marked_self;
  std::size_t m_marked_depth{static_cast<std::size_t>(-1)};
};

}  // namespace clpp
