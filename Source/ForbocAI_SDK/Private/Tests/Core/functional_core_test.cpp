/**
 * Tests for functional_core.hpp §19 Dispatcher, §20 multi_match, §21 from_nullable
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */

#include "Core/functional_core.hpp"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

/**
 * Test: Dispatcher — key lookup returns just(handler())
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDispatcherKeyLookupTest,
    "ForbocAI.Core.FunctionalCore.Dispatcher.KeyLookup",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FDispatcherKeyLookupTest::RunTest(const FString &Parameters) {
  std::vector<std::pair<std::string, std::function<int()>>> entries;
  entries.push_back(std::make_pair(std::string("a"), []() { return 1; }));
  entries.push_back(std::make_pair(std::string("b"), []() { return 2; }));
  entries.push_back(std::make_pair(std::string("c"), []() { return 3; }));

  auto d = func::createDispatcher<std::string, int>(entries);

  auto resultA = func::dispatch(d, std::string("a"));
  TestTrue("Key 'a' found", resultA.hasValue);
  TestEqual("Key 'a' returns 1", resultA.value, 1);

  auto resultB = func::dispatch(d, std::string("b"));
  TestTrue("Key 'b' found", resultB.hasValue);
  TestEqual("Key 'b' returns 2", resultB.value, 2);

  auto resultC = func::dispatch(d, std::string("c"));
  TestTrue("Key 'c' found", resultC.hasValue);
  TestEqual("Key 'c' returns 3", resultC.value, 3);

  return true;
}

/**
 * Test: Dispatcher — missing key returns nothing
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDispatcherMissingKeyTest,
    "ForbocAI.Core.FunctionalCore.Dispatcher.MissingKey",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FDispatcherMissingKeyTest::RunTest(const FString &Parameters) {
  std::vector<std::pair<std::string, std::function<int()>>> entries;
  entries.push_back(std::make_pair(std::string("a"), []() { return 1; }));

  auto d = func::createDispatcher<std::string, int>(entries);

  auto result = func::dispatch(d, std::string("z"));
  TestFalse("Missing key returns nothing", result.hasValue);

  return true;
}

/**
 * Test: Dispatcher — has() and keys()
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDispatcherHasAndKeysTest,
    "ForbocAI.Core.FunctionalCore.Dispatcher.HasAndKeys",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FDispatcherHasAndKeysTest::RunTest(const FString &Parameters) {
  std::vector<std::pair<std::string, std::function<int()>>> entries;
  entries.push_back(std::make_pair(std::string("x"), []() { return 10; }));
  entries.push_back(std::make_pair(std::string("y"), []() { return 20; }));

  auto d = func::createDispatcher<std::string, int>(entries);

  TestTrue("has('x') is true", func::has(d, std::string("x")));
  TestTrue("has('y') is true", func::has(d, std::string("y")));
  TestFalse("has('z') is false", func::has(d, std::string("z")));

  auto k = func::keys(d);
  TestEqual("keys() has 2 entries", static_cast<int>(k.size()), 2);

  return true;
}

/**
 * Test: multi_match — predicate matching
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMultiMatchPredicateTest,
    "ForbocAI.Core.FunctionalCore.MultiMatch.Predicate",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FMultiMatchPredicateTest::RunTest(const FString &Parameters) {
  std::vector<func::MatchCase<int, std::string>> cases;
  cases.push_back(func::when<int, std::string>(
      [](const int &v) { return v < 0; },
      [](const int &) { return std::string("negative"); }));
  cases.push_back(func::when<int, std::string>(
      [](const int &v) { return v == 0; },
      [](const int &) { return std::string("zero"); }));
  cases.push_back(func::when<int, std::string>(
      [](const int &v) { return v > 0; },
      [](const int &) { return std::string("positive"); }));

  auto neg = func::multi_match<int, std::string>(-5, cases);
  TestTrue("Negative matched", neg.hasValue);
  TestEqual("Negative result", neg.value, std::string("negative"));

  auto zero = func::multi_match<int, std::string>(0, cases);
  TestTrue("Zero matched", zero.hasValue);
  TestEqual("Zero result", zero.value, std::string("zero"));

  auto pos = func::multi_match<int, std::string>(42, cases);
  TestTrue("Positive matched", pos.hasValue);
  TestEqual("Positive result", pos.value, std::string("positive"));

  return true;
}

/**
 * Test: multi_match — wildcard catches all
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMultiMatchWildcardTest,
    "ForbocAI.Core.FunctionalCore.MultiMatch.Wildcard",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FMultiMatchWildcardTest::RunTest(const FString &Parameters) {
  std::vector<func::MatchCase<int, std::string>> cases;
  cases.push_back(func::when<int, std::string>(
      func::equals<int>(1),
      [](const int &) { return std::string("one"); }));
  cases.push_back(func::when<int, std::string>(
      func::wildcard<int>(),
      [](const int &) { return std::string("default"); }));

  auto one = func::multi_match<int, std::string>(1, cases);
  TestTrue("Exact match found", one.hasValue);
  TestEqual("Exact match result", one.value, std::string("one"));

  auto other = func::multi_match<int, std::string>(99, cases);
  TestTrue("Wildcard matched", other.hasValue);
  TestEqual("Wildcard result", other.value, std::string("default"));

  return true;
}

/**
 * Test: multi_match — no match returns nothing
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMultiMatchNoMatchTest,
    "ForbocAI.Core.FunctionalCore.MultiMatch.NoMatch",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FMultiMatchNoMatchTest::RunTest(const FString &Parameters) {
  std::vector<func::MatchCase<int, std::string>> cases;
  cases.push_back(func::when<int, std::string>(
      func::equals<int>(1),
      [](const int &) { return std::string("one"); }));

  auto result = func::multi_match<int, std::string>(99, cases);
  TestFalse("No match returns nothing", result.hasValue);

  return true;
}

/**
 * Test: multi_match — value equality via equals()
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMultiMatchEqualsTest,
    "ForbocAI.Core.FunctionalCore.MultiMatch.Equals",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FMultiMatchEqualsTest::RunTest(const FString &Parameters) {
  std::vector<func::MatchCase<std::string, int>> cases;
  cases.push_back(func::when<std::string, int>(
      func::equals<std::string>(std::string("alpha")),
      [](const std::string &) { return 1; }));
  cases.push_back(func::when<std::string, int>(
      func::equals<std::string>(std::string("beta")),
      [](const std::string &) { return 2; }));
  cases.push_back(func::when<std::string, int>(
      func::equals<std::string>(std::string("gamma")),
      [](const std::string &) { return 3; }));

  auto alpha = func::multi_match<std::string, int>(std::string("alpha"), cases);
  TestTrue("alpha matched", alpha.hasValue);
  TestEqual("alpha returns 1", alpha.value, 1);

  auto beta = func::multi_match<std::string, int>(std::string("beta"), cases);
  TestTrue("beta matched", beta.hasValue);
  TestEqual("beta returns 2", beta.value, 2);

  auto miss = func::multi_match<std::string, int>(std::string("delta"), cases);
  TestFalse("delta not matched", miss.hasValue);

  return true;
}

/**
 * Test: from_nullable — pointer
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFromNullablePtrTest,
    "ForbocAI.Core.FunctionalCore.FromNullable.Pointer",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FFromNullablePtrTest::RunTest(const FString &Parameters) {
  int val = 42;
  int *p = &val;
  auto m = func::from_nullable(p);
  TestTrue("Non-null pointer is just", m.hasValue);
  TestEqual("Value from pointer", m.value, 42);

  int *null_p = nullptr;
  auto n = func::from_nullable(null_p);
  TestFalse("Null pointer is nothing", n.hasValue);

  return true;
}

/**
 * Test: from_nullable_value — explicit validity flag
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFromNullableValueTest,
    "ForbocAI.Core.FunctionalCore.FromNullable.Value",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FFromNullableValueTest::RunTest(const FString &Parameters) {
  auto valid = func::from_nullable_value(std::string("hello"), true);
  TestTrue("Valid value is just", valid.hasValue);
  TestEqual("Valid value content", valid.value, std::string("hello"));

  auto invalid = func::from_nullable_value(std::string(""), false);
  TestFalse("Invalid value is nothing", invalid.hasValue);

  return true;
}

/**
 * Test: require_just — extracts or throws
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRequireJustTest,
    "ForbocAI.Core.FunctionalCore.RequireJust",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FRequireJustTest::RunTest(const FString &Parameters) {
  auto j = func::just(42);
  int extracted = func::require_just(j, "should not throw");
  TestEqual("require_just extracts value", extracted, 42);

  auto n = func::nothing<int>();
  bool threw = false;
  try {
    func::require_just(n, "expected failure");
  } catch (const std::runtime_error &e) {
    threw = true;
    TestEqual("Error message matches", std::string(e.what()),
              std::string("expected failure"));
  }
  TestTrue("require_just throws on nothing", threw);

  return true;
}
