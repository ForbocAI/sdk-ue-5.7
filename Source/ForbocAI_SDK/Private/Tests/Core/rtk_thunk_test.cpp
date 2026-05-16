#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "rtk_test_mocks.h"

using namespace rtk;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRtkAsyncThunkTest,
                                 "ForbocAI.Core.RTK.AsyncThunk",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FRtkAsyncThunkTest::RunTest(const FString &Parameters) {
  /**
   * Build a mock async thunk
   * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
   */
  auto TestThunk = createAsyncThunk<int, FString, FAppMockState>(
      TEXT("test/fetchData"),
      [](const FString &Arg,
         const ThunkApi<FAppMockState> &Api) -> func::AsyncResult<int> {
        return func::AsyncResult<int>::create(
            [Arg](std::function<void(int)> Resolve,
                  std::function<void(std::string)> Reject) {
              if (Arg == TEXT("fail")) {
                Reject("Mock error");
              } else {
                Resolve(42);
              }
            });
      });

  TArray<FString> DispatchedActions;
  std::function<AnyAction(const AnyAction &)> MockDispatch =
      [&DispatchedActions](const AnyAction &Action) {
        DispatchedActions.Add(Action.Type);
        return Action;
      };

  std::function<FAppMockState()> MockGetState = []() {
    return FAppMockState{};
  };

  /**
   * Test Success Path
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  auto ThunkActionSuccess = TestThunk(TEXT("success"));
  auto ResultSuccess = ThunkActionSuccess(MockDispatch, MockGetState);
  ResultSuccess.execute();

  TestEqual("Dispatched pending first (success)", DispatchedActions[0],
            FString(TEXT("test/fetchData/pending")));
  TestEqual("Dispatched fulfilled second (success)", DispatchedActions[1],
            FString(TEXT("test/fetchData/fulfilled")));
  DispatchedActions.Empty();

  /**
   * Test Failure Path
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  auto ThunkActionFail = TestThunk(TEXT("fail"));
  auto ResultFail = ThunkActionFail(MockDispatch, MockGetState);
  ResultFail.execute();

  TestEqual("Dispatched pending first (fail)", DispatchedActions[0],
            FString(TEXT("test/fetchData/pending")));
  TestEqual("Dispatched rejected second (fail)", DispatchedActions[1],
            FString(TEXT("test/fetchData/rejected")));

  return true;
}
