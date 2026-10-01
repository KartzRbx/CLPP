#include "clpp/vm.hpp"

#include "clpp/register_ir.hpp"
#include "clpp/stdlib.hpp"
#include "core/vm/gc.hpp"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cmath>
#include <condition_variable>
#include <memory>
#include <cstring>
#include <mutex>
#include <iostream>
#include <ostream>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace clpp {

namespace {

[[nodiscard]] std::string format_number(const double number) {
  // Whole numbers print as whole numbers (4499998500000, not 4.4999985e+12) as long as a double
  // holds them exactly; everything else prints the shortest text that reads back the same value.
  if (std::isfinite(number) && number == std::trunc(number) && std::fabs(number) < 9007199254740992.0) {
    return std::to_string(static_cast<long long>(number));
  }
  char buffer[64];
  const std::to_chars_result result =
      std::to_chars(buffer, buffer + sizeof(buffer), number, std::chars_format::general);
  if (result.ec != std::errc{}) {
    return {};
  }
  return std::string(buffer, result.ptr);
}

[[nodiscard]] std::string format_value(const Value& value, bool nested = false);

// post() shows values the way they are written: lists as [1, 2], dictionaries as {gold: 1},
// struct values as (Ada, 100), vectors as (1, 2, 3). Strings inside a collection are quoted.
[[nodiscard]] std::string format_value(const Value& value, const bool nested) {
  if (value.is_string()) {
    return nested ? "\"" + value.text + "\"" : value.text;
  }
  if (value.is_vector()) {
    std::string text = "(" + format_number(value.number) + ", " + format_number(value.y);
    if (value.dims >= 3) {
      text += ", " + format_number(value.z);
    }
    if (value.dims >= 4) {
      text += ", " + format_number(value.w);
    }
    return text + ")";
  }
  if (value.is_struct() && value.number == 65534 && value.fields.size() == 1) {
    return format_value(value.fields[0], nested);  // variant box
  }
  if (value.is_struct()) {
    const bool dictionary = value.number == 65535;
    const bool list = value.number == 0;
    std::string text = dictionary ? "{" : list ? "[" : "(";
    const std::size_t step = dictionary ? 2 : 1;
    for (std::size_t index = 0; index < value.fields.size(); index += step) {
      if (index != 0) {
        text += ", ";
      }
      if (dictionary && index + 1 < value.fields.size()) {
        text += format_value(value.fields[index], false) + ": " + format_value(value.fields[index + 1], true);
      } else {
        text += format_value(value.fields[index], true);
      }
    }
    return text + (dictionary ? "}" : list ? "]" : ")");
  }
  return format_number(value.number);
}

[[nodiscard]] bool values_equal(const Value& left, const Value& right) {
  if (left.kind != right.kind) {
    return false;
  }
  switch (left.kind) {
    case Value::Kind::Number:
      return left.number == right.number;
    case Value::Kind::String:
      return left.text == right.text;
    case Value::Kind::Vector:
      return left.number == right.number && left.y == right.y && left.z == right.z && left.w == right.w;
    case Value::Kind::Struct:
      if (left.number != right.number || left.fields.size() != right.fields.size()) {
        return false;
      }
      for (std::size_t index = 0; index < left.fields.size(); ++index) {
        if (!values_equal(left.fields[index], right.fields[index])) {
          return false;
        }
      }
      return true;
    default:
      return left.table == right.table && left.number == right.number;
  }
}

[[nodiscard]] bool is_truthy(const Value& value) {
  if (value.is_string()) {
    return !value.text.empty();
  }
  return value.number != 0;
}

struct ThreadSlot {
  std::mutex mu;
  std::condition_variable cv;
  bool done{false};
  Value result{Value::number_of(0)};
  std::string error;
  std::thread worker;

  ThreadSlot() = default;
  ThreadSlot(const ThreadSlot&) = delete;
  ThreadSlot& operator=(const ThreadSlot&) = delete;
  // The worker lambda owns a reference to this slot. When the join side has already dropped
  // its reference, the last one dies on the worker thread itself, and a thread cannot join
  // itself (std::system_error "Resource deadlock avoided"). Detach in that case: the thread
  // is already finishing. Otherwise join, as std::jthread would (std::thread is used because
  // libc++ still ships jthread as experimental).
  ~ThreadSlot() {
    if (!worker.joinable()) {
      return;
    }
    if (worker.get_id() == std::this_thread::get_id()) {
      worker.detach();
    } else {
      worker.join();
    }
  }
};

std::mutex g_runtime_mu;
std::vector<std::shared_ptr<ThreadSlot>> g_threads;
std::vector<std::unique_ptr<std::atomic<long long>>> g_atomics;
std::vector<std::unique_ptr<std::mutex>> g_mutexes;

[[nodiscard]] long long make_atomic(const long long initial) {
  std::lock_guard<std::mutex> lock(g_runtime_mu);
  g_atomics.push_back(std::make_unique<std::atomic<long long>>(initial));
  return static_cast<long long>(g_atomics.size() - 1);
}

[[nodiscard]] bool atomic_fetch(const long long id, const long long delta, long long& previous) {
  std::atomic<long long>* cell = nullptr;
  {
    std::lock_guard<std::mutex> lock(g_runtime_mu);
    if (id < 0 || static_cast<std::size_t>(id) >= g_atomics.size()) {
      return false;
    }
    cell = g_atomics[static_cast<std::size_t>(id)].get();
  }
  previous = cell->fetch_add(delta);
  return true;
}

[[nodiscard]] bool atomic_read(const long long id, long long& value) {
  std::atomic<long long>* cell = nullptr;
  {
    std::lock_guard<std::mutex> lock(g_runtime_mu);
    if (id < 0 || static_cast<std::size_t>(id) >= g_atomics.size()) {
      return false;
    }
    cell = g_atomics[static_cast<std::size_t>(id)].get();
  }
  value = cell->load();
  return true;
}

[[nodiscard]] long long make_mutex() {
  std::lock_guard<std::mutex> lock(g_runtime_mu);
  g_mutexes.push_back(std::make_unique<std::mutex>());
  return static_cast<long long>(g_mutexes.size() - 1);
}

[[nodiscard]] std::mutex* mutex_at(const long long id) {
  std::lock_guard<std::mutex> lock(g_runtime_mu);
  if (id < 0 || static_cast<std::size_t>(id) >= g_mutexes.size()) {
    return nullptr;
  }
  return g_mutexes[static_cast<std::size_t>(id)].get();
}

}  // namespace

void VirtualMachine::set_output(std::ostream& out) { m_out = &out; }

void VirtualMachine::set_error_output(std::ostream& err) { m_err = &err; }

void VirtualMachine::isolate() { m_isolated = true; }

void VirtualMachine::load(const BytecodeChunk& chunk) { m_chunk = chunk; }

bool VirtualMachine::read_u16(std::uint16_t& index) {
  if (m_ip + 1 >= m_chunk.code.size()) {
    return false;
  }
  const auto lo = static_cast<std::uint16_t>(m_chunk.code[m_ip]);
  ++m_ip;
  const auto hi = static_cast<std::uint16_t>(m_chunk.code[m_ip]);
  ++m_ip;
  index = static_cast<std::uint16_t>(lo | static_cast<std::uint16_t>(hi << 8));
  return true;
}

std::string_view VirtualMachine::error() const { return m_error; }

void VirtualMachine::locate_error(const RegChunk& lowered) {
  m_error_location = {};
  if (m_error.empty() || lowered.origin.empty()) {
    return;
  }
  const std::size_t index = m_ip == 0 ? 0 : std::min(m_ip - 1, lowered.origin.size() - 1);
  const std::uint16_t pc = lowered.origin[index];
  int best = -1;
  for (const SourceMapEntry& entry : m_chunk.source_map) {
    if (entry.pc <= pc && entry.location.line != 0 && static_cast<int>(entry.pc) >= best) {
      best = entry.pc;
      m_error_location = entry.location;
    }
  }
}

std::vector<Value> VirtualMachine::take_frame(const RegChunk& lowered, const std::size_t function) {
  const std::size_t size = function < lowered.function_frame.size() ? lowered.function_frame[function] : 256;
  std::vector<Value> frame;
  if (!m_frame_pool.empty()) {
    frame = std::move(m_frame_pool.back());
    m_frame_pool.pop_back();
  }
  frame.assign(size, Value::number_of(0));
  return frame;
}

void VirtualMachine::give_frame(std::vector<Value>&& frame) {
  if (m_frame_pool.size() < 64 && frame.capacity() <= 256) {
    m_frame_pool.push_back(std::move(frame));
  }
}

void VirtualMachine::set_max_call_depth(const std::size_t depth) { m_max_call_depth = depth == 0 ? 1 : depth; }

void VirtualMachine::collect_garbage() {
  for (const std::unique_ptr<Table>& object : m_heap) {
    if (object != nullptr) {
      object->marked = false;
    }
  }
  vm::mark_value(m_self);
  for (Value& value : m_stack) {
    vm::mark_value(value);
  }
  for (Value& value : m_locals) {
    vm::mark_value(value);
  }
  for (CallFrame& frame : m_calls) {
    for (Value& value : frame.stack) {
      vm::mark_value(value);
    }
    for (Value& value : frame.locals) {
      vm::mark_value(value);
    }
  }
  vm::sweep(m_heap);
}

bool VirtualMachine::recover() {
  if (m_traps.empty()) {
    return false;
  }
  const Trap trap = m_traps.back();
  m_traps.pop_back();
  while (m_calls.size() > trap.frames) {
    CallFrame frame = std::move(m_calls.back());
    m_calls.pop_back();
    m_stack = std::move(frame.stack);
    m_locals = std::move(frame.locals);
  }
  m_ip = trap.ip;
  m_recovered = std::string(m_error);
  m_error.clear();
  return true;
}

bool VirtualMachine::execute_registers(const RegChunk& lowered) {
  const auto reg = [&](const std::uint8_t index) -> Value* {
    if (index >= m_stack.size()) {
      m_error = "runtime error";
      return nullptr;
    }
    return &m_stack[index];
  };
  const auto run_now = [&](const std::uint16_t index, const std::vector<Value>& args, Value& out) {
    VirtualMachine box;
    box.load(m_chunk);
    if (m_out != nullptr) {
      box.set_output(*m_out);
    }
    if (!box.run_function(index, args, out)) {
      m_error = std::string(box.error().empty() ? "runtime error" : box.error());
      return false;
    }
    return true;
  };
  const auto launch = [&](const std::uint16_t index, std::vector<Value> args) {
    const BytecodeChunk chunk = m_chunk;
    std::ostream* const out = m_out;
    const auto state = std::make_shared<ThreadSlot>();
    state->worker = std::thread([state, chunk, args = std::move(args), index, out]() {
      VirtualMachine worker;
      worker.load(chunk);
      if (out != nullptr) {
        worker.set_output(*out);
      }
      Value result = Value::number_of(0);
      std::string error;
      if (!worker.run_function(index, args, result)) {
        error = std::string(worker.error().empty() ? "runtime error" : worker.error());
      }
      {
        std::lock_guard<std::mutex> lock(state->mu);
        state->result = std::move(result);
        state->error = std::move(error);
        state->done = true;
      }
      state->cv.notify_one();
    });
    std::lock_guard<std::mutex> lock(g_runtime_mu);
    g_threads.push_back(state);
    return static_cast<long long>(g_threads.size() - 1);
  };
  const auto finish = [&](const long long handle, Value& out) {
    std::shared_ptr<ThreadSlot> state;
    {
      std::lock_guard<std::mutex> lock(g_runtime_mu);
      if (handle < 0 || static_cast<std::size_t>(handle) >= g_threads.size()) {
        m_error = "runtime error";
        return false;
      }
      state = g_threads[static_cast<std::size_t>(handle)];
    }
    {
      std::unique_lock<std::mutex> lock(state->mu);
      state->cv.wait(lock, [&]() { return state->done; });
    }
    if (!state->error.empty()) {
      m_error = state->error;
      return false;
    }
    out = state->result;
    return true;
  };

  while (m_ip < lowered.code.size()) {
    const std::uint32_t word = lowered.code[m_ip];
    ++m_ip;
    const auto op = static_cast<RegOp>(reg_op(word));
    const std::uint8_t a = reg_a(word);
    const std::uint8_t b = reg_b(word);
    const std::uint8_t c = reg_c(word);
    const std::uint16_t d = reg_d(word);
    Value* dest = reg(a);
    Value* left = reg(b);
    Value* right = reg(c);
    if (dest == nullptr || left == nullptr || right == nullptr) {
      return false;
    }

    switch (op) {
      case RegOp::Nop:
        break;
      case RegOp::Halt:
        if (!m_deferred.empty()) {
          const std::uint16_t function_index = m_deferred.front();
          m_deferred.erase(m_deferred.begin());
          if (function_index >= m_chunk.functions.size() || function_index >= lowered.function_entry.size() ||
              m_chunk.functions[function_index].arity != 0) {
            m_error = "runtime error";
            return false;
          }
          CallFrame frame;
          frame.ip = m_ip - 1;
          frame.stack = m_stack;
          m_calls.push_back(std::move(frame));
          m_stack.assign(256, Value::number_of(0));
          m_ip = lowered.function_entry[function_index];
          break;
        }
        return true;
      case RegOp::LoadK:
        if (d >= m_chunk.constants.size()) {
          m_error = "runtime error";
          return false;
        }
        *dest = m_chunk.constants[d];
        break;
      case RegOp::Move:
        *dest = *left;
        break;
      case RegOp::Clear:
        if (dest->is_string()) {
          *dest = Value::string_of("");
        } else if (dest->is_struct() || dest->is_task()) {
          *dest = Value::struct_of({});
        } else if (dest->is_vector()) {
          *dest = Value::vector_of(0, 0, 0, 0);
        } else {
          dest->set_number(0);
        }
        break;
      case RegOp::Add:
      case RegOp::IntAdd:
      case RegOp::Sub:
      case RegOp::Mul:
      case RegOp::Div:
      case RegOp::Mod: {
        if (op == RegOp::Add && left->is_vector() && right->is_vector()) {
          *dest = Value::vector_of(left->number + right->number, left->y + right->y, left->z + right->z, left->w + right->w,
                                   std::max(left->dims, right->dims));
          break;
        }
        if (op == RegOp::Sub && left->is_vector() && right->is_vector()) {
          *dest = Value::vector_of(left->number - right->number, left->y - right->y, left->z - right->z, left->w - right->w,
                                   std::max(left->dims, right->dims));
          break;
        }
        if ((op == RegOp::Mul || op == RegOp::Div) && left->is_vector() && right->is_number()) {
          if (op == RegOp::Div && right->number == 0) {
            m_error = "division by zero";
            if (recover()) {
              continue;
            }
            return false;
          }
          const double factor = op == RegOp::Mul ? right->number : 1.0 / right->number;
          *dest = Value::vector_of(left->number * factor, left->y * factor, left->z * factor, left->w * factor, left->dims);
          break;
        }
        if (op == RegOp::Mul && left->is_number() && right->is_vector()) {
          *dest = Value::vector_of(right->number * left->number, right->y * left->number, right->z * left->number,
                                   right->w * left->number, right->dims);
          break;
        }
        if (!left->is_number() || !right->is_number()) {
          m_error = "type error";
          return false;
        }
        if ((op == RegOp::Div || op == RegOp::Mod) && right->number == 0) {
          m_error = "division by zero";
          if (recover()) {
            continue;
          }
          return false;
        }
        double result = 0;
        if (op == RegOp::Add || op == RegOp::IntAdd) {
          result = left->number + right->number;
        } else if (op == RegOp::Sub) {
          result = left->number - right->number;
        } else if (op == RegOp::Mul) {
          result = left->number * right->number;
        } else if (op == RegOp::Div) {
          result = left->number / right->number;
        } else {
          result = std::fmod(left->number, right->number);
        }
        dest->set_number(result);
        break;
      }
      case RegOp::BitAnd:
      case RegOp::BitOr:
      case RegOp::BitXor:
      case RegOp::Shl:
      case RegOp::Shr: {
        if (left == nullptr || right == nullptr || !left->is_number() || !right->is_number()) {
          m_error = "type error";
          return false;
        }
        const auto lhs = static_cast<long long>(left->number);
        const auto rhs = static_cast<long long>(right->number);
        long long result = 0;
        if (op == RegOp::BitAnd) {
          result = lhs & rhs;
        } else if (op == RegOp::BitOr) {
          result = lhs | rhs;
        } else if (op == RegOp::BitXor) {
          result = lhs ^ rhs;
        } else if (op == RegOp::Shl) {
          result = lhs << (rhs & 63);
        } else {
          result = lhs >> (rhs & 63);
        }
        dest->set_number(static_cast<double>(result));
        break;
      }
      case RegOp::BitNot: {
        if (left == nullptr || !left->is_number()) {
          m_error = "type error";
          return false;
        }
        dest->set_number(static_cast<double>(~static_cast<long long>(left->number)));
        break;
      }
      case RegOp::Range: {
        if (left == nullptr || right == nullptr || !left->is_number() || !right->is_number()) {
          m_error = "type error";
          return false;
        }
        std::vector<Value> values;
        const int lo = static_cast<int>(left->number);
        const int hi = static_cast<int>(right->number);
        for (int cursor = lo; cursor < hi && values.size() < 4096; ++cursor) {
          values.push_back(Value::number_of(cursor));
        }
        *dest = Value::struct_of(std::move(values));
        break;
      }
      case RegOp::Slice: {
        if (static_cast<std::size_t>(a) + 3 > m_stack.size()) {
          m_error = "runtime error";
          return false;
        }
        const Value object = m_stack[a];
        const Value start = m_stack[static_cast<std::size_t>(a) + 1];
        const Value bound = m_stack[static_cast<std::size_t>(a) + 2];
        if (!start.is_number() || !bound.is_number()) {
          m_error = "type error";
          return false;
        }
        const int lo = static_cast<int>(start.number);
        const int hi = static_cast<int>(bound.number);
        if (object.is_string()) {
          const int size = static_cast<int>(object.text.size());
          const int from = lo < 0 ? 0 : lo;
          const int to = hi > size ? size : hi;
          *dest = Value::string_of(from >= to ? std::string{} : object.text.substr(static_cast<std::size_t>(from), static_cast<std::size_t>(to - from)));
          break;
        }
        if (object.is_struct()) {
          std::vector<Value> values;
          const int size = static_cast<int>(object.fields.size());
          const int from = lo < 0 ? 0 : lo;
          const int to = hi > size ? size : hi;
          for (int cursor = from; cursor < to; ++cursor) {
            values.push_back(object.fields[static_cast<std::size_t>(cursor)]);
          }
          *dest = Value::struct_of(std::move(values));
          break;
        }
        m_error = "type error";
        return false;
      }
      case RegOp::PushError:
        *dest = Value::string_of(m_recovered);
        break;
      case RegOp::Concat: {
        // Any value joins as text: "pos " .: Vector3(1, 2, 3) is "pos (1, 2, 3)".
        *dest = Value::string_of(format_value(*left) + format_value(*right));
        break;
      }
      case RegOp::Not:
        dest->set_number(is_truthy(*left) ? 0 : 1);
        break;
      case RegOp::Eq:
      case RegOp::NotEq:
        if (left->is_string() && right->is_string()) {
          const bool same = left->text == right->text;
          dest->set_number((op == RegOp::Eq ? same : !same) ? 1 : 0);
          break;
        }
        if (!left->is_number() || !right->is_number()) {
          // structs, lists and vectors compare by value, field by field
          const bool same = values_equal(*left, *right);
          dest->set_number((op == RegOp::Eq ? same : !same) ? 1 : 0);
          break;
        }
        [[fallthrough]];
      case RegOp::Less:
      case RegOp::LessEq:
      case RegOp::Greater:
      case RegOp::GreaterEq: {
        if (!left->is_number() || !right->is_number()) {
          m_error = "type error";
          return false;
        }
        bool result = false;
        if (op == RegOp::Eq) {
          result = left->number == right->number;
        } else if (op == RegOp::NotEq) {
          result = left->number != right->number;
        } else if (op == RegOp::Less) {
          result = left->number < right->number;
        } else if (op == RegOp::LessEq) {
          result = left->number <= right->number;
        } else if (op == RegOp::Greater) {
          result = left->number > right->number;
        } else {
          result = left->number >= right->number;
        }
        dest->set_number(result ? 1 : 0);
        break;
      }
      case RegOp::Jump:
        m_ip = d;
        break;
      case RegOp::JumpIfFalse:
        if (!is_truthy(*dest)) {
          m_ip = d;
        }
        break;
      case RegOp::Post:
        if (m_out != nullptr) {
          (*m_out) << format_value(*dest) << '\n';
        }
        break;
      case RegOp::Warn:
        if (m_err != nullptr) {
          (*m_err) << "warning: " << format_value(*dest) << '\n';
        }
        break;
      case RegOp::Report:
        // report(x) stops the program with x as the error message; try/catch and pcall catch it.
        m_error = format_value(*dest);
        if (recover()) {
          continue;
        }
        return false;
      case RegOp::Tag:
        if (!dest->is_struct()) {
          m_error = "type error";
          return false;
        }
        dest->number = d;
        break;
      case RegOp::VCall: {
        m_marked_depth = static_cast<std::size_t>(-1);
        if (!m_stack[a].is_struct() || m_stack[a].number < 1) {
          m_error = "type error";
          return false;
        }
        const auto type_index = static_cast<std::uint16_t>(m_stack[a].number) - 1;
        if (type_index >= m_chunk.vtables.size() || d >= m_chunk.vtables[type_index].size()) {
          m_error = "type error";
          return false;
        }
        const std::uint16_t callee = m_chunk.vtables[type_index][d];
        if (callee >= m_chunk.functions.size() || callee >= lowered.function_entry.size()) {
          m_error = "runtime error";
          return false;
        }
        const FunctionBytecode& function = m_chunk.functions[callee];
        if (static_cast<std::size_t>(a) + function.arity > m_stack.size()) {
          m_error = "runtime error";
          return false;
        }
        if (m_calls.size() >= m_max_call_depth) {
          m_error = "stack overflow";
          return false;
        }
        std::vector<Value> frame = take_frame(lowered, callee);
        for (std::uint16_t arg = 0; arg < function.arity; ++arg) {
          frame[arg] = m_stack[static_cast<std::size_t>(a) + arg];
        }
        m_calls.push_back(CallFrame{m_ip, std::move(m_stack), {}, a, function.arity});
        m_stack = std::move(frame);
        m_ip = lowered.function_entry[callee];
        break;
      }
      case RegOp::Call: {
        if (m_calls.size() >= m_max_call_depth) {
          m_error = "stack overflow";
          return false;
        }
        if (d >= m_chunk.functions.size() || d >= lowered.function_entry.size()) {
          m_error = "runtime error";
          return false;
        }
        const FunctionBytecode& function = m_chunk.functions[d];
        if (static_cast<std::size_t>(a) + function.arity > m_stack.size()) {
          m_error = "runtime error";
          return false;
        }
        // The callee gets a frame sized to what it uses (not 256 registers), recycled from a pool.
        std::vector<Value> frame = take_frame(lowered, d);
        for (std::uint16_t arg = 0; arg < function.arity; ++arg) {
          frame[arg] = m_stack[static_cast<std::size_t>(a) + arg];
        }
        m_calls.push_back(CallFrame{m_ip, std::move(m_stack), {}, a, function.arity});
        m_stack = std::move(frame);
        m_ip = lowered.function_entry[d];
        break;
      }
      case RegOp::Schedule: {
        if (d >= m_chunk.functions.size()) {
          m_error = "runtime error";
          return false;
        }
        const FunctionBytecode& function = m_chunk.functions[d];
        if (static_cast<std::size_t>(a) + function.arity > m_stack.size()) {
          m_error = "runtime error";
          return false;
        }
        std::vector<Value> args(function.arity);
        for (std::uint16_t arg = 0; arg < function.arity; ++arg) {
          args[arg] = m_stack[static_cast<std::size_t>(a) + arg];
        }
        Value task;
        task.kind = Value::Kind::Task;
        task.number = d;
        task.y = 0;
        task.fields = std::move(args);
        *dest = std::move(task);
        break;
      }
      case RegOp::CoCreate: {
        Coroutine created;
        created.function = d;
        m_coroutines.push_back(std::move(created));
        dest->set_number(static_cast<double>(m_coroutines.size()));
        break;
      }
      case RegOp::Defer:
        m_deferred.push_back(d);
        dest->set_number(0);
        break;
      case RegOp::CoResume: {
        if (m_resumer.live || !left->is_number() || left->number < 1) {
          m_error = "type error";
          return false;
        }
        const auto index = static_cast<std::size_t>(left->number) - 1;
        if (index >= m_coroutines.size() || m_coroutines[index].dead) {
          m_error = "type error";
          return false;
        }
        Coroutine& routine = m_coroutines[index];
        if (routine.function >= lowered.function_entry.size() || m_chunk.functions[routine.function].arity != 0) {
          m_error = "runtime error";
          return false;
        }
        m_resumer.live = true;
        m_resumer.ip = m_ip;
        m_resumer.stack = m_stack;
        m_resumer.frames = m_calls;
        m_resumer.base = a;
        m_active = static_cast<int>(index);
        if (!routine.started) {
          routine.started = true;
          m_calls.clear();
          CallFrame root;
          root.co_root = true;
          m_calls.push_back(std::move(root));
          m_stack.assign(256, Value::number_of(0));
          m_ip = lowered.function_entry[routine.function];
        } else {
          m_calls = routine.frames;
          m_stack = routine.stack;
          m_ip = routine.ip;
        }
        break;
      }
      case RegOp::CoYield: {
        if (m_active < 0 || !m_resumer.live || static_cast<std::size_t>(m_active) >= m_coroutines.size()) {
          m_error = "type error";
          return false;
        }
        const Value yielded = *left;
        dest->set_number(0);
        Coroutine& routine = m_coroutines[static_cast<std::size_t>(m_active)];
        routine.started = true;
        routine.ip = m_ip;
        routine.stack = m_stack;
        routine.frames = m_calls;
        m_ip = m_resumer.ip;
        m_stack = std::move(m_resumer.stack);
        m_calls = std::move(m_resumer.frames);
        if (m_resumer.base >= m_stack.size()) {
          m_error = "runtime error";
          return false;
        }
        m_stack[m_resumer.base] = yielded;
        m_resumer.live = false;
        m_active = -1;
        break;
      }
      case RegOp::Actor: {
        if (d >= m_chunk.functions.size() || m_chunk.functions[d].arity != 1) {
          m_error = "runtime error";
          return false;
        }
        const Value argument = *dest;
        const BytecodeChunk chunk = m_chunk;
        Value result = Value::number_of(0);
        std::string failure;
        std::thread worker([&] {
          VirtualMachine box;
          box.isolate();
          box.load(chunk);
          if (!box.run_function(d, {argument}, result)) {
            failure = std::string(box.error());
          }
        });
        worker.join();
        if (!failure.empty()) {
          m_error = failure.empty() ? "sandbox" : failure;
          return false;
        }
        *dest = std::move(result);
        break;
      }
      case RegOp::Return: {
        Value result = *dest;
        if (!m_calls.empty() && m_calls.back().co_root) {
          m_calls.pop_back();
          if (m_active >= 0 && static_cast<std::size_t>(m_active) < m_coroutines.size()) {
            m_coroutines[static_cast<std::size_t>(m_active)].dead = true;
          }
          if (!m_resumer.live) {
            m_error = "runtime error";
            return false;
          }
          m_ip = m_resumer.ip;
          m_stack = std::move(m_resumer.stack);
          m_calls = std::move(m_resumer.frames);
          if (m_resumer.base >= m_stack.size()) {
            m_error = "runtime error";
            return false;
          }
          m_stack[m_resumer.base] = std::move(result);
          m_resumer.live = false;
          m_active = -1;
          break;
        }
        if (m_calls.empty()) {
          return true;
        }
        CallFrame frame = std::move(m_calls.back());
        m_calls.pop_back();
        m_ip = frame.ip;
        give_frame(std::move(m_stack));
        m_stack = std::move(frame.stack);
        if (frame.base >= m_stack.size()) {
          m_error = "runtime error";
          return false;
        }
        m_stack[frame.base] = std::move(result);
        break;
      }
      case RegOp::MakeVector: {
        if (static_cast<std::size_t>(a) + c > m_stack.size() || c < 2 || c > 4) {
          m_error = "type error";
          return false;
        }
        double parts[4] = {0, 0, 0, 0};
        for (std::uint8_t index = 0; index < c; ++index) {
          const Value& part = m_stack[static_cast<std::size_t>(a) + index];
          if (!part.is_number()) {
            m_error = "type error";
            return false;
          }
          parts[index] = part.number;
        }
        *dest = Value::vector_of(parts[0], parts[1], parts[2], parts[3], c);
        break;
      }
      case RegOp::MakeBuffer:
        if (!left->is_number() || left->number < 0) {
          m_error = "type error";
          return false;
        }
        *dest = Value::buffer_of(left->number);
        break;
      case RegOp::BufferWrite:
        if (static_cast<std::size_t>(a) + 2 >= m_stack.size() || !m_stack[a].is_buffer() || !m_stack[a + 1].is_number() ||
            !m_stack[static_cast<std::size_t>(a) + 2].is_string()) {
          m_error = "type error";
          return false;
        }
        dest->set_number(0);
        break;
      case RegOp::BufferSize:
        if (!left->is_buffer()) {
          m_error = "type error";
          return false;
        }
        dest->set_number(left->number);
        break;
      case RegOp::GetField: {
        const Value object = *left;
        if (object.is_struct()) {
          if (c >= object.fields.size()) {
            m_error = "type error";
            return false;
          }
          *dest = object.fields[c];
          break;
        }
        if (!object.is_vector() || c > 3) {
          m_error = "type error";
          return false;
        }
        double component = object.number;
        if (c == 1) {
          component = object.y;
        } else if (c == 2) {
          component = object.z;
        } else if (c == 3) {
          component = object.w;
        }
        dest->set_number(component);
        break;
      }
      case RegOp::MakeStruct: {
        if (static_cast<std::size_t>(a) + d > m_stack.size()) {
          m_error = "runtime error";
          return false;
        }
        std::vector<Value> fields;
        fields.reserve(d);
        for (std::uint16_t index = 0; index < d; ++index) {
          fields.push_back(m_stack[static_cast<std::size_t>(a) + index]);
        }
        *dest = Value::struct_of(std::move(fields));
        break;
      }
      case RegOp::GetSelf:
      case RegOp::SetSelf: {
        if (d >= m_chunk.constants.size() || !m_chunk.constants[d].is_string() || m_self.table == nullptr) {
          m_error = "runtime error";
          return false;
        }
        const std::string& name = m_chunk.constants[d].text;
        if (op == RegOp::SetSelf) {
          bool replaced = false;
          for (std::pair<std::string, Value>& entry : m_self.table->entries) {
            if (entry.first == name) {
              entry.second = *dest;
              replaced = true;
              break;
            }
          }
          if (!replaced) {
            m_self.table->entries.emplace_back(name, *dest);
          }
          collect_garbage();
          break;
        }
        Value found = Value::number_of(0);
        for (const std::pair<std::string, Value>& entry : m_self.table->entries) {
          if (entry.first == name) {
            found = entry.second;
            break;
          }
        }
        *dest = std::move(found);
        break;
      }
      case RegOp::MakeTask:
        *dest = Value::task_of(*left);
        break;
      case RegOp::Await: {
        if (left == nullptr || !left->is_task()) {
          m_error = "type error";
          return false;
        }
        const Value task = *left;
        if (task.y >= 2) {
          if (task.fields.empty()) {
            m_error = "type error";
            return false;
          }
          *dest = task.fields.front();
          break;
        }
        const double function_limit = static_cast<double>(m_chunk.functions.size());
        Value result = Value::number_of(0);
        if (task.y >= 1) {
          if (!finish(static_cast<long long>(task.z), result)) {
            return false;
          }
        } else if (task.number < 0 || task.number >= function_limit ||
                   !run_now(static_cast<std::uint16_t>(task.number), task.fields, result)) {
          if (m_error.empty()) {
            m_error = "runtime error";
          }
          return false;
        }
        *dest = std::move(result);
        break;
      }
      case RegOp::Spawn: {
        if (left == nullptr || !left->is_task()) {
          m_error = "type error";
          return false;
        }
        Value task = *left;
        if (task.y == 0) {
          if (task.number < 0 || task.number >= static_cast<double>(m_chunk.functions.size())) {
            m_error = "runtime error";
            return false;
          }
          const long long handle = launch(static_cast<std::uint16_t>(task.number), task.fields);
          task.y = 1;
          task.z = static_cast<double>(handle);
        }
        *dest = std::move(task);
        break;
      }
      case RegOp::Parallel: {
        if (d < 2 || static_cast<std::size_t>(a) + d > m_stack.size()) {
          m_error = "runtime error";
          return false;
        }
        std::vector<Value> tasks(d);
        for (std::uint16_t index = 0; index < d; ++index) {
          tasks[index] = m_stack[static_cast<std::size_t>(a) + index];
          if (!tasks[index].is_task()) {
            m_error = "type error";
            return false;
          }
        }
        std::vector<long long> handles(d, -2);
        for (std::uint16_t index = 0; index < d; ++index) {
          Value& task = tasks[index];
          if (task.y >= 2) {
            continue;
          }
          if (task.y == 0) {
            if (task.number < 0 || task.number >= static_cast<double>(m_chunk.functions.size())) {
              m_error = "runtime error";
              return false;
            }
            handles[index] = launch(static_cast<std::uint16_t>(task.number), task.fields);
          } else {
            handles[index] = static_cast<long long>(task.z);
          }
        }
        // parallel(a(), b(), ...) waits for all and gives their results as a list, in order.
        std::vector<Value> results;
        results.reserve(d);
        for (std::uint16_t index = 0; index < d; ++index) {
          Value result = Value::number_of(0);
          if (handles[index] == -2) {
            if (tasks[index].fields.empty()) {
              m_error = "type error";
              return false;
            }
            result = tasks[index].fields.front();
          } else if (!finish(handles[index], result)) {
            if (recover()) {
              break;
            }
            return false;
          }
          results.push_back(std::move(result));
        }
        if (results.size() != d) {
          continue;  // an error was caught
        }
        *dest = Value::struct_of(std::move(results));
        break;
      }
      case RegOp::FileSize:
        if (m_isolated) {
          m_error = "sandbox";
          return false;
        }
        if (!left->is_string()) {
          m_error = "type error";
          return false;
        }
        dest->set_number(stdlib::file_size(left->text));
        break;
      case RegOp::Env:
        if (m_isolated) {
          m_error = "sandbox";
          return false;
        }
        if (!left->is_string()) {
          m_error = "type error";
          return false;
        }
        *dest = Value::string_of(stdlib::env_value(left->text));
        break;
      case RegOp::HttpHost:
        if (m_isolated) {
          m_error = "sandbox";
          return false;
        }
        if (!left->is_string()) {
          m_error = "type error";
          return false;
        }
        *dest = Value::string_of(stdlib::http_host(left->text));
        break;
      case RegOp::GetIndex: {
        // References, not copies: `list[i]` used to copy the whole list on every read.
        const Value& object = *left;
        const Value& index = *right;
        if (object.is_struct() && index.is_string() && object.number == 65535) {
          Value found = Value::number_of(0);
          for (std::size_t cursor = 0; cursor + 1 < object.fields.size(); cursor += 2) {
            const Value& key = object.fields[cursor];
            if (key.is_string() && key.text == index.text) {
              found = object.fields[cursor + 1];
              break;
            }
          }
          *dest = std::move(found);
          break;
        }
        if (!index.is_number()) {
          m_error = "type error";
          return false;
        }
        const int at = index.number < 0 ? -1 : static_cast<int>(index.number);
        if (object.is_struct()) {
          if (at < 0 || static_cast<std::size_t>(at) >= object.fields.size()) {
            m_error = "index out of range: " + format_number(index.number) + " (size " +
                      std::to_string(object.fields.size()) + ")";
            if (recover()) {
              continue;
            }
            return false;
          }
          Value element = object.fields[static_cast<std::size_t>(at)];
          *dest = std::move(element);
          break;
        }
        if (object.is_string()) {
          // One character (a UTF-8 code point), not the byte's number.
          std::size_t offset = 0;
          int remaining = at;
          const std::string& text = object.text;
          while (offset < text.size() && remaining > 0) {
            const auto lead = static_cast<unsigned char>(text[offset]);
            offset += lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
            --remaining;
          }
          if (at < 0 || offset >= text.size()) {
            m_error = "index out of range: " + format_number(index.number);
            if (recover()) {
              continue;
            }
            return false;
          }
          const auto lead = static_cast<unsigned char>(text[offset]);
          const std::size_t width = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
          Value character = Value::string_of(text.substr(offset, width));
          *dest = std::move(character);
          break;
        }
        if (!object.is_vector()) {
          m_error = "type error";
          return false;
        }
        double component = 0;
        if (at == 0) {
          component = object.number;
        } else if (at == 1) {
          component = object.y;
        } else if (at == 2) {
          component = object.z;
        } else if (at == 3) {
          component = object.w;
        }
        dest->set_number(component);
        break;
      }
      case RegOp::SpawnThread: {
        if (d >= m_chunk.functions.size() || d >= lowered.function_entry.size()) {
          m_error = "runtime error";
          return false;
        }
        const FunctionBytecode& function = m_chunk.functions[d];
        if (static_cast<std::size_t>(a) + function.arity > m_stack.size()) {
          m_error = "runtime error";
          return false;
        }
        std::vector<Value> args(function.arity);
        for (std::uint16_t arg = 0; arg < function.arity; ++arg) {
          args[arg] = m_stack[static_cast<std::size_t>(a) + arg];
        }
        const BytecodeChunk chunk = m_chunk;
        const auto state = std::make_shared<ThreadSlot>();
        const std::uint16_t index = d;
        state->worker = std::thread([state, chunk, args, index]() {
          VirtualMachine worker;
          worker.load(chunk);
          Value result = Value::number_of(0);
          std::string error;
          if (!worker.run_function(index, args, result)) {
            error = std::string(worker.error());
          }
          {
            std::lock_guard<std::mutex> lock(state->mu);
            state->result = std::move(result);
            state->error = std::move(error);
            state->done = true;
          }
          state->cv.notify_one();
        });
        long long handle = 0;
        {
          std::lock_guard<std::mutex> lock(g_runtime_mu);
          g_threads.push_back(state);
          handle = static_cast<long long>(g_threads.size() - 1);
        }
        dest->set_number(static_cast<double>(handle));
        break;
      }
      case RegOp::Join: {
        const long long handle = static_cast<long long>(left->number);
        std::shared_ptr<ThreadSlot> state;
        {
          std::lock_guard<std::mutex> lock(g_runtime_mu);
          if (handle < 0 || static_cast<std::size_t>(handle) >= g_threads.size()) {
            m_error = "runtime error";
            return false;
          }
          state = g_threads[static_cast<std::size_t>(handle)];
        }
        {
          std::unique_lock<std::mutex> lock(state->mu);
          state->cv.wait(lock, [&]() { return state->done; });
        }
        if (!state->error.empty()) {
          m_error = state->error;
          return false;
        }
        *dest = state->result;
        break;
      }
      case RegOp::AtomicNew: {
        if (!left->is_number()) {
          m_error = "type error";
          return false;
        }
        dest->set_number(static_cast<double>(make_atomic(static_cast<long long>(left->number))));
        break;
      }
      case RegOp::FetchAdd: {
        if (!left->is_number() || !right->is_number()) {
          m_error = "type error";
          return false;
        }
        long long previous = 0;
        if (!atomic_fetch(static_cast<long long>(left->number), static_cast<long long>(right->number), previous)) {
          m_error = "runtime error";
          return false;
        }
        dest->set_number(static_cast<double>(previous));
        break;
      }
      case RegOp::AtomicLoad: {
        if (!left->is_number()) {
          m_error = "type error";
          return false;
        }
        long long value = 0;
        if (!atomic_read(static_cast<long long>(left->number), value)) {
          m_error = "runtime error";
          return false;
        }
        dest->set_number(static_cast<double>(value));
        break;
      }
      case RegOp::MutexNew:
        dest->set_number(static_cast<double>(make_mutex()));
        break;
      case RegOp::Lock:
      case RegOp::Unlock: {
        if (!left->is_number()) {
          m_error = "type error";
          return false;
        }
        std::mutex* cell = mutex_at(static_cast<long long>(left->number));
        if (cell == nullptr) {
          m_error = "runtime error";
          return false;
        }
        if (op == RegOp::Lock) {
          cell->lock();
        } else {
          cell->unlock();
        }
        dest->set_number(0);
        break;
      }
      case RegOp::ListSort: {
        if (!left->is_struct()) {
          m_error = "type error";
          return false;
        }
        std::vector<double> numbers;
        numbers.reserve(left->fields.size());
        for (const Value& field : left->fields) {
          if (!field.is_number()) {
            m_error = "type error";
            return false;
          }
          numbers.push_back(field.number);
        }
        std::sort(numbers.begin(), numbers.end());
        std::vector<Value> fields;
        fields.reserve(numbers.size());
        for (const double number : numbers) {
          fields.push_back(Value::number_of(number));
        }
        *dest = Value::struct_of(std::move(fields));
        break;
      }
      case RegOp::ListFind: {
        if (!left->is_struct()) {
          m_error = "type error";
          return false;
        }
        int found = -1;
        for (std::size_t index = 0; index < left->fields.size(); ++index) {
          if (values_equal(left->fields[index], *right)) {
            found = static_cast<int>(index);
            break;
          }
        }
        dest->set_number(found);
        break;
      }
      case RegOp::ListLen:
      case RegOp::IterLen: {
        const Value& source = *left;
        if (op == RegOp::IterLen && source.is_number()) {
          dest->set_number(source.number < 0 ? 0 : std::floor(source.number));
          break;
        }
        double count = 0;
        if (source.is_struct()) {
          // dictionaries keep key, value, key, value... and are tagged with 65535
          count = static_cast<double>(source.number == 65535 ? source.fields.size() / 2 : source.fields.size());
        } else if (source.is_string()) {
          for (const char byte : source.text) {
            if ((static_cast<unsigned char>(byte) & 0xC0) != 0x80) {
              count += 1;
            }
          }
        } else {
          m_error = op == RegOp::ListLen ? "len expects a list, array, dictionary or string"
                                         : "for-in expects a number or a collection";
          if (recover()) {
            continue;
          }
          return false;
        }
        dest->set_number(count);
        break;
      }
      case RegOp::IterAt: {
        const Value& source = *left;
        const auto at = static_cast<std::size_t>(right->number);
        if (source.is_number()) {
          dest->set_number(right->number);
          break;
        }
        if (source.is_struct()) {
          const bool dictionary = source.number == 65535;
          Value element = source.fields[dictionary ? at * 2 : at];
          *dest = std::move(element);
          break;
        }
        // string: the at-th UTF-8 character
        const std::string& text = source.text;
        std::size_t offset = 0;
        for (std::size_t skipped = 0; skipped < at && offset < text.size(); ++skipped) {
          const auto lead = static_cast<unsigned char>(text[offset]);
          offset += lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
        }
        const auto lead = static_cast<unsigned char>(offset < text.size() ? text[offset] : 0);
        const std::size_t width = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
        Value character = Value::string_of(offset < text.size() ? text.substr(offset, width) : std::string());
        *dest = std::move(character);
        break;
      }
      case RegOp::SetField: {
        Value& object = *dest;
        const Value& value = *left;  // B: the new value
        if (object.is_struct()) {
          if (c >= object.fields.size()) {
            m_error = "unknown field";
            return false;
          }
          object.fields[c] = value;
          break;
        }
        if (object.is_vector() && c <= 3 && value.is_number()) {
          (c == 0 ? object.number : c == 1 ? object.y : c == 2 ? object.z : object.w) = value.number;
          break;
        }
        m_error = "type error";
        return false;
      }
      case RegOp::SetIndex: {
        Value& object = *dest;
        const Value& index = *left;   // B
        const Value& value = *right;  // C
        if (object.is_struct() && object.number == 65535 && index.is_string()) {
          bool replaced = false;
          for (std::size_t cursor = 0; cursor + 1 < object.fields.size(); cursor += 2) {
            if (object.fields[cursor].is_string() && object.fields[cursor].text == index.text) {
              object.fields[cursor + 1] = value;
              replaced = true;
              break;
            }
          }
          if (!replaced) {
            object.fields.push_back(index);
            object.fields.push_back(value);
          }
          break;
        }
        if (!index.is_number()) {
          m_error = "type error";
          return false;
        }
        const int at = index.number < 0 ? -1 : static_cast<int>(index.number);
        if (object.is_struct()) {
          if (at < 0 || static_cast<std::size_t>(at) >= object.fields.size()) {
            m_error = "index out of range: " + format_number(index.number) + " (size " +
                      std::to_string(object.fields.size()) + ")";
            if (recover()) {
              continue;
            }
            return false;
          }
          object.fields[static_cast<std::size_t>(at)] = value;
          break;
        }
        if (object.is_vector() && at >= 0 && at <= 3 && value.is_number()) {
          (at == 0 ? object.number : at == 1 ? object.y : at == 2 ? object.z : object.w) = value.number;
          break;
        }
        m_error = object.is_string() ? "strings cannot be changed in place; build a new one with .:" : "type error";
        if (recover()) {
          continue;
        }
        return false;
      }
      case RegOp::ListPush:
      case RegOp::ListPop:
      case RegOp::ListInsert:
      case RegOp::ListRemove: {
        Value& list = *dest;  // the variable's register
        const bool dictionary = list.is_struct() && list.number == 65535;
        if (!list.is_struct() || (dictionary && op != RegOp::ListRemove) || (!dictionary && list.number != 0)) {
          m_error = "push/pop/insert/remove expect a list";
          if (recover()) {
            continue;
          }
          return false;
        }
        if (op == RegOp::ListPush) {
          list.fields.push_back(*left);
          break;
        }
        if (op == RegOp::ListPop) {
          if (list.fields.empty()) {
            m_error = "pop from an empty list";
            if (recover()) {
              continue;
            }
            return false;
          }
          Value last = std::move(list.fields.back());
          list.fields.pop_back();
          *left = std::move(last);  // B: the result register
          break;
        }
        if (dictionary) {
          // remove(dict, key): drop the pair, give back the value (or 0 when the key is missing)
          Value removed = Value::number_of(0);
          for (std::size_t cursor = 0; cursor + 1 < list.fields.size(); cursor += 2) {
            if (values_equal(list.fields[cursor], *left)) {
              removed = std::move(list.fields[cursor + 1]);
              list.fields.erase(list.fields.begin() + static_cast<std::ptrdiff_t>(cursor),
                                list.fields.begin() + static_cast<std::ptrdiff_t>(cursor) + 2);
              break;
            }
          }
          *left = std::move(removed);
          break;
        }
        if (!left->is_number()) {
          m_error = "type error";
          return false;
        }
        const double position = left->number;
        const std::size_t limit = op == RegOp::ListInsert ? list.fields.size() + 1 : list.fields.size();
        if (position < 0 || static_cast<std::size_t>(position) >= limit) {
          m_error = "index out of range: " + format_number(position) + " (size " + std::to_string(list.fields.size()) + ")";
          if (recover()) {
            continue;
          }
          return false;
        }
        const auto at = static_cast<std::ptrdiff_t>(position);
        if (op == RegOp::ListInsert) {
          list.fields.insert(list.fields.begin() + at, *right);
          break;
        }
        Value removed = std::move(list.fields[static_cast<std::size_t>(at)]);
        list.fields.erase(list.fields.begin() + at);
        *left = std::move(removed);
        break;
      }
      case RegOp::MarkSelf:
        m_marked_self = *dest;  // A = 0: self
        m_marked_depth = m_calls.size();
        break;
      case RegOp::SelfBack:
        if (m_marked_depth == m_calls.size() + 1) {
          *dest = std::move(m_marked_self);
          m_marked_self = Value{};
        }
        m_marked_depth = static_cast<std::size_t>(-1);
        break;
      case RegOp::IntDiv: {
        if (!left->is_number() || !right->is_number()) {
          m_error = "type error";
          return false;
        }
        if (right->number == 0) {
          m_error = "division by zero";
          if (recover()) {
            continue;
          }
          return false;
        }
        dest->set_number(std::trunc(left->number / right->number));
        break;
      }
      case RegOp::Std: {
        if (b > 8 || static_cast<std::size_t>(a) + b > m_stack.size()) {
          m_error = "runtime error";
          return false;
        }
        Value copied[8];
        for (std::uint8_t index = 0; index < b; ++index) {
          copied[index] = m_stack[static_cast<std::size_t>(a) + index];
        }
        if (!stdlib::std_apply(c, b == 0 ? nullptr : copied, b, *dest, m_error)) {
          if (recover()) {  // library errors are catchable like any other
            continue;
          }
          return false;
        }
        break;
      }
      case RegOp::Axiom: {
        if (b > 8 || static_cast<std::size_t>(a) + b > m_stack.size()) {
          m_error = "runtime error";
          return false;
        }
        Value copied[8];
        for (std::uint8_t index = 0; index < b; ++index) {
          copied[index] = m_stack[static_cast<std::size_t>(a) + index];
        }
        if (!stdlib::axiom_apply(c, copied, b, *dest, m_error)) {
          if (recover()) {  // library errors are catchable like any other
            continue;
          }
          return false;
        }
        break;
      }
      case RegOp::CCall: {
        if (d != 0 || !dest->is_string()) {
          m_error = "type error";
          return false;
        }
        dest->set_number(static_cast<double>(std::strlen(dest->text.c_str())));
        break;
      }
      case RegOp::Protect:
        m_traps.push_back(Trap{d, 0, m_calls.size()});
        break;
      case RegOp::EndTry:
        if (!m_traps.empty()) {
          m_traps.pop_back();
        }
        break;
    }
  }
  return m_error.empty();
}

bool VirtualMachine::run_function(const std::uint16_t index, const std::vector<Value>& args, Value& out) {
  m_error.clear();
  m_calls.clear();
  m_traps.clear();
  m_heap.clear();
  m_self = Value::table_of(vm::allocate(m_heap));
  const RegChunk lowered = lower_registers(m_chunk);
  if (!lowered.error.empty() || index >= m_chunk.functions.size() || index >= lowered.function_entry.size()) {
    if (m_error.empty()) {
      m_error = lowered.error.empty() ? "runtime error" : lowered.error;
    }
    return false;
  }
  const FunctionBytecode& function = m_chunk.functions[index];
  if (args.size() != function.arity) {
    m_error = "runtime error";
    return false;
  }
  m_calls.push_back(CallFrame{lowered.code.size(), std::vector<Value>(256, Value::number_of(0)), {}, 0, function.arity});
  m_stack.assign(256, Value::number_of(0));
  for (std::size_t arg = 0; arg < args.size(); ++arg) {
    m_stack[arg] = args[arg];
  }
  m_ip = lowered.function_entry[index];
  if (!execute_registers(lowered)) {
    return false;
  }
  if (m_stack.empty()) {
    m_error = "runtime error";
    return false;
  }
  out = m_stack[0];
  return m_error.empty();
}

bool VirtualMachine::run() {
  m_ip = m_chunk.entry;
  m_stack.clear();
  m_calls.clear();
  m_traps.clear();
  m_error.clear();
  m_coroutines.clear();
  m_deferred.clear();
  m_resumer = {};
  m_active = -1;
  m_locals.assign(m_chunk.local_count, Value::number_of(0));
  m_heap.clear();
  m_self = Value::table_of(vm::allocate(m_heap));
  collect_garbage();
  const RegChunk lowered = lower_registers(m_chunk);
  if (!lowered.error.empty()) {
    m_error = lowered.error;
    return false;
  }
  m_stack.assign(256, Value::number_of(0));
  m_locals.clear();
  m_ip = lowered.entry;
  const bool ok = execute_registers(lowered);
  if (!ok) {
    locate_error(lowered);
  }
  return ok;
}

}  // namespace clpp
