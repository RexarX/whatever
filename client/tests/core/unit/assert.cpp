#include <doctest/doctest.h>

#include <client/core/assert.hpp>

TEST_SUITE("client::Assert") {
  TEST_CASE("CLIENT_ASSERT: True condition with message") {
    // Should not trigger assertion
    CHECK_NOTHROW(CLIENT_ASSERT(true, "This should not abort"));
    CHECK_NOTHROW(CLIENT_ASSERT(1 == 1, "Math works"));
    CHECK_NOTHROW(CLIENT_ASSERT(42 > 0, "Positive number"));
  }

  TEST_CASE("CLIENT_ASSERT: True condition without message") {
    // Should not trigger assertion
    CHECK_NOTHROW(CLIENT_ASSERT(true));
    CHECK_NOTHROW(CLIENT_ASSERT(1 == 1));
    CHECK_NOTHROW(CLIENT_ASSERT(42 > 0));
  }

  TEST_CASE("CLIENT_INVARIANT: True condition") {
    // Should not trigger
    CHECK_NOTHROW(CLIENT_INVARIANT(true, "Invariant holds"));
    CHECK_NOTHROW(CLIENT_INVARIANT(1 == 1, "Math invariant"));
    CHECK_NOTHROW(CLIENT_INVARIANT(42 > 0, "Positive invariant"));
  }

  TEST_CASE("CLIENT_VERIFY: True condition") {
    // Should not trigger
    CHECK_NOTHROW(CLIENT_VERIFY(true, "Verification passed"));
    CHECK_NOTHROW(CLIENT_VERIFY(1 == 1, "Math verification"));
    CHECK_NOTHROW(CLIENT_VERIFY(42 > 0, "Positive verification"));
  }

  TEST_CASE("CLIENT_VERIFY_LOGGER: True condition") {
    // Should not trigger
    CHECK_NOTHROW(CLIENT_VERIFY_LOGGER("test_logger", true, "Verification passed"));
    CHECK_NOTHROW(CLIENT_VERIFY_LOGGER("test_logger", 1 == 1, "Math verification"));
    CHECK_NOTHROW(CLIENT_VERIFY_LOGGER("test_logger", 42 > 0, "Positive verification"));
  }

  TEST_CASE("kEnableAssert: Flag") {
#ifdef CLIENT_ENABLE_ASSERTS
    CHECK(client::details::kEnableAssert);
#else
    CHECK_FALSE(client::details::kEnableAssert);
#endif
  }

  TEST_CASE("Assert: Macros compile in debug and release") {
    // This test mainly checks that macros compile correctly
    // and don't produce warnings in release mode

    bool condition = true;

    CLIENT_ASSERT(condition);
    CLIENT_ASSERT(condition, "message");
    CLIENT_INVARIANT(condition, "invariant");
    CLIENT_VERIFY(condition, "verify");
    CLIENT_VERIFY_LOGGER("logger", condition, "verify with logger");

    // In release builds, these should compile to minimal/no-op code
    // In debug builds, they should perform the actual checks
    CHECK(true);  // Just to make doctest happy
  }

  // Note: We cannot test actual assertion failures in unit tests
  // as they would abort the test process. Assertion failures should
  // be tested manually or in separate integration tests with
  // subprocess management.

  TEST_CASE("AbortWithStacktrace: Function exists") {
    // We can't actually call it (would abort), but we can verify it exists
    // by taking its address
    auto func_ptr = &client::AbortWithStacktrace;
    CHECK_NE(func_ptr, nullptr);
  }

  TEST_CASE("Assert: Works independently of logger") {
    // Assertions should work even before logger is initialized
    // or if logger fails. This is verified by the fact that
    // assert.hpp doesn't directly include logger.hpp

    CLIENT_ASSERT(true);
    CLIENT_ASSERT(true, "Works without logger dependency");

    CHECK(true);  // Test passes if we get here
  }

}  // TEST_SUITE
