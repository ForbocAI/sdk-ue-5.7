#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "rtk_test_mocks.h"

using namespace rtk;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRtkMiddlewareTest,
                                 "ForbocAI.Core.RTK.Middleware",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FRtkMiddlewareTest::RunTest(const FString &Parameters) {
  TArray<FString> EventLog;

  /**
   * 1. Setup Base Dispatch and GetState
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  std::function<AnyAction(const AnyAction &)> BaseDispatch =
      [&EventLog](const AnyAction &Action) {
        EventLog.Add(FString::Printf(TEXT("BaseDispatch:%s"), *Action.Type));
        return Action;
      };

  std::function<FAppMockState()> GetState = []() { return FAppMockState{}; };

  /**
   * 2. Setup Middleware A (Logging before and after)
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  Middleware<FAppMockState> MiddlewareA =
      [&EventLog](const MiddlewareApi<FAppMockState> &Api)
          -> std::function<Dispatcher(Dispatcher)> {
        return [&EventLog](Dispatcher Next) -> Dispatcher {
          return [&EventLog, Next](const AnyAction &Action) -> AnyAction {
            EventLog.Add(TEXT("MwA_Before"));
            auto Result = Next(Action);
            EventLog.Add(TEXT("MwA_After"));
            return Result;
          };
        };
      };

  /**
   * 3. Setup Listener Middleware (MwB)
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  ListenerMiddleware<FAppMockState> Listeners =
      addListener(createListenerMiddleware<FAppMockState>(), TEXT("trigger"),
                  [&EventLog](const AnyAction &Action,
                              const MiddlewareApi<FAppMockState> &Api) {
                    EventLog.Add(TEXT("Listener_Triggered"));
                  });

  /**
   * 4. Compose
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  std::vector<Middleware<FAppMockState>> Chain = {
      MiddlewareA, buildListenerMiddleware(Listeners)};
  auto EnhancedDispatch = applyMiddleware(BaseDispatch, GetState, Chain);

  /**
   * 5. Execute
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  EnhancedDispatch(
      AnyAction{TEXT("trigger"), std::make_shared<rtk::FEmptyPayload>()});

  /**
   * Validate Order
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  TestEqual("Event Log Length", EventLog.Num(), 4);
  if (EventLog.Num() == 4) {
    TestEqual("0: MwA wraps everything", EventLog[0],
              FString(TEXT("MwA_Before")));
    TestEqual("1: Base runs inside", EventLog[1],
              FString(TEXT("BaseDispatch:trigger")));
    TestEqual("2: Listener runs post-reducer inside MwB", EventLog[2],
              FString(TEXT("Listener_Triggered")));
    TestEqual("3: MwA finishes", EventLog[3], FString(TEXT("MwA_After")));
  }

  return true;
}
