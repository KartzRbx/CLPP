#include <catch2/catch_test_macros.hpp>

TEST_CASE("intentional reporter failure") {
  INFO("Failure reports must remain valid under sanitizer container annotations");
  CHECK(false);
}
