#pragma once

#include <iosfwd>

namespace clpp {

[[nodiscard]] int run_repl(std::istream& in, std::ostream& out, std::ostream& err);

}  // namespace clpp
