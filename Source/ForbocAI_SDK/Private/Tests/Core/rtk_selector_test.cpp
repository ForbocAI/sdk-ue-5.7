#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "rtk_test_mocks.h"

using namespace rtk;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRtkSelectorTest, "ForbocAI.Core.RTK.Selector",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FRtkSelectorTest::RunTest(const FString &Parameters) {
  struct FTestState {
    int32 A;
    int32 B;

    bool operator==(const FTestState &Other) const {
      return A == Other.A && B == Other.B;
    }
  };

  auto SelectA = [](const FTestState &State) { return State.A; };
  auto SelectB = [](const FTestState &State) { return State.B; };

  int32 Computations = 0;

  auto ComplexSelector = createSelector<FTestState, int32>(
      std::make_tuple(SelectA, SelectB), [&Computations](int32 A, int32 B) {
        Computations++;
        return A + B;
      });

  FTestState State1{10, 20};

  /**
   * First call computes
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  TestEqual("Initial computation", ComplexSelector(State1), 30);
  TestEqual("Computations count = 1", Computations, 1);

  /**
   * Second call with same state (by value equality) hits cache
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  FTestState State2{10, 20};
  TestEqual("Cache hit computation", ComplexSelector(State2), 30);
  TestEqual("Computations count still = 1", Computations, 1);

  /**
   * Third call with new state computes
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  FTestState State3{10, 25};
  TestEqual("New state computation", ComplexSelector(State3), 35);
  TestEqual("Computations count = 2", Computations, 2);

  return true;
}
