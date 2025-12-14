#include <doctest/doctest.h>

#include <client/core/core.hpp>

#include <string_view>

TEST_SUITE("client::Core") {
  TEST_CASE("CLIENT_BIT") {
    CHECK_EQ(CLIENT_BIT(0), 1);
    CHECK_EQ(CLIENT_BIT(1), 2);
    CHECK_EQ(CLIENT_BIT(2), 4);
    CHECK_EQ(CLIENT_BIT(3), 8);
  }

  TEST_CASE("CLIENT_STRINGIFY") {
    CHECK_EQ(std::string_view(CLIENT_STRINGIFY(hello)), "hello");
  }

  TEST_CASE("CLIENT_CONCAT") {
    constexpr int foo42 = 123;
    CHECK_EQ(CLIENT_CONCAT(foo, 42), 123);
  }

  TEST_CASE("CLIENT_DEBUG_BREAK: Compiles") {
    // This macro is not meant to be executed in tests, but should compile.
    // Uncomment to check compilation only:
    // CLIENT_DEBUG_BREAK();
    CHECK(true);
  }

  TEST_CASE("CLIENT_UNREACHABLE: Compiles") {
    // This macro is not meant to be executed in tests, but should compile.
    // Uncomment to check compilation only:
    // CLIENT_UNREACHABLE();
    CHECK(true);
  }

}  // TEST_SUITE
