#include "NPC/NPCModule.h"
#include "Ghost/GhostModule.h"
#include "Ghost/GhostModuleInternal.h"
#include "Ghost/GhostSlice.h"

namespace GhostInternal {

GhostTypes::AsyncResult<FGhostTestResult>
RunScenarioTest(const FAgent &Agent, const FString &Scenario) {
  return GhostTypes::AsyncResult<FGhostTestResult>::create(
      [Agent, Scenario](std::function<void(FGhostTestResult)> resolve,
                        std::function<void(std::string)> reject) {
        FString TestInput =
            FString::Printf(TEXT("Test scenario: %s"), *Scenario);

        AgentOps::Process(Agent, TestInput, TMap<FString, FString>())
            .then([Scenario, resolve](FAgentResponse Response) {
              FGhostTestResult Result;
              Result.Scenario = Scenario;
              Result.ActualResponse = Response.Dialogue;
              Result.Iteration = 1;
              Result.bPassed = !Response.Dialogue.IsEmpty();
              resolve(Result);
            })
            .catch_([reject](std::string Error) { reject(Error); })
            .execute();
      });
}

void RunTestsSequentially(const FGhost &Ghost,
                          TSharedPtr<FGhostTestReport> Report, int32 Index,
                          std::function<void(FGhostTestReport)> OnComplete,
                          std::function<void(std::string)> OnError) {
  Index >= Ghost.Config.Scenarios.Num()
      ? [&]() {
          Report->TotalTests = Report->Results.Num();
          Report->SuccessRate =
              Report->TotalTests > 0
                  ? (float)Report->PassedTests / Report->TotalTests
                  : 0.0f;

          auto summaryResult =
              GhostInternal::GenerateTestSummary(*Report);
          Report->Summary = summaryResult.isLeft ? summaryResult.left
                                                 : summaryResult.right;

          OnComplete(*Report);
        }()
      : [&]() {
          FString Scenario = Ghost.Config.Scenarios[Index];

          GhostOps::RunTest(Ghost, Scenario)
              .then([Ghost, Report, Index, OnComplete,
                     OnError](FGhostTestResult Result) {
                Report->Results.Add(Result);
                Result.bPassed ? (Report->PassedTests++, void())
                               : (Report->FailedTests++, void());

                RunTestsSequentially(Ghost, Report, Index + 1,
                                     OnComplete, OnError);
              })
              .catch_([Ghost, Report, Scenario, Index, OnComplete,
                       OnError](std::string Error) {
                FGhostTestResult Result;
                Result.Scenario = Scenario;
                Result.bPassed = false;
                Result.ErrorMessage = FString(Error.c_str());

                Report->Results.Add(Result);
                Report->FailedTests++;

                RunTestsSequentially(Ghost, Report, Index + 1,
                                     OnComplete, OnError);
              })
              .execute();
        }();
}

} // namespace GhostInternal
