/**
 * G.6: Local ghost test execution thunk — test logic in thunk, module dispatches
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */

#include "Core/ThunkDetail.h"
#include "Ghost/GhostModuleInternal.h"
#include "Ghost/GhostSlice.h"
#include "Ghost/GhostThunks.h"

namespace rtk {

ThunkAction<FGhostTestResult, FStoreState>
runLocalGhostTestThunk(const FAgent &Agent, const FString &Scenario) {
  return [Agent, Scenario](std::function<AnyAction(const AnyAction &)> Dispatch,
                           std::function<FStoreState()> GetState)
             -> func::AsyncResult<FGhostTestResult> {
    return Scenario.IsEmpty()
               ? detail::RejectAsync<FGhostTestResult>(
                     TCHAR_TO_UTF8(*FString(TEXT("Scenario cannot be empty"))))
               : (Dispatch(GhostSlice::Actions::GhostSessionStarted(
                      Scenario, TEXT("running"))),
                  func::AsyncChain::then<FGhostTestResult, FGhostTestResult>(
        GhostInternal::RunScenarioTest(Agent, Scenario),
        [Dispatch](const FGhostTestResult &Result) {
          FGhostTestReport Report;
          Report.Results.Add(Result);
          Report.TotalTests = 1;
          Report.PassedTests = Result.bPassed ? 1 : 0;
          Report.FailedTests = Result.bPassed ? 0 : 1;
          Report.SuccessRate = Result.bPassed ? 1.0f : 0.0f;
          Report.Summary = Result.bPassed ? TEXT("Passed") : TEXT("Failed");
          Dispatch(GhostSlice::Actions::GhostSessionCompleted(Report));
          return detail::ResolveAsync(Result);
        })
        .catch_([Dispatch, Scenario](std::string Error) {
          Dispatch(GhostSlice::Actions::GhostSessionFailed(
              Scenario, FString(UTF8_TO_TCHAR(Error.c_str()))));
        }));
  };
}

} // namespace rtk
