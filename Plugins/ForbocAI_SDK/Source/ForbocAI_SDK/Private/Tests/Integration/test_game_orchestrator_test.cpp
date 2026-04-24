#include "Misc/AutomationTest.h"
#include "TestGame/TestGameOrchestrator.h"

using namespace TestGame;

namespace {

int32 CountScenarioCommands() {
  int32 Count = 0;
  const TArray<FScenarioStep> Steps = GetDefaultScenarioSteps();
  for (const FScenarioStep &Step : Steps) {
    Count += Step.Commands.Num();
  }
  return Count;
}

int32 CountErrorEntries(const TArray<FTranscriptEntry> &Entries) {
  int32 Count = 0;
  for (const FTranscriptEntry &Entry : Entries) {
    Count += Entry.Status == ETranscriptStatus::Error ? 1 : 0;
  }
  return Count;
}

bool ContainsMissingGroup(const TArray<ECommandGroup> &Groups,
                          ECommandGroup Target) {
  for (ECommandGroup Group : Groups) {
    if (Group == Target) {
      return true;
    }
  }
  return false;
}

FCommandExecutor MakeExecutor(const FString &FailCommand = FString()) {
  return [FailCommand](FCommandExecutionContext &Context,
                       const FCommandSpec &Command) -> FCommandResult {
    (void)Context;
    return !FailCommand.IsEmpty() && Command.Command == FailCommand
               ? FCommandResult{ETranscriptStatus::Error, TEXT("stub failure")}
               : FCommandResult{ETranscriptStatus::Ok, TEXT("stub success")};
  };
}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTestGameRunGameSuccessTest, "ForbocAI.Integration.TestGame.RunGame.Success",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FTestGameRunGameSuccessTest::RunTest(const FString &Parameters) {
  (void)Parameters;

  const auto Result =
      RunGame(EPlayMode::Autoplay, MakeExecutor());

  TestTrue("Run completes when all commands succeed", Result.bComplete);
  TestEqual("All scenario commands are recorded", Result.Transcript.Num(),
            CountScenarioCommands());
  TestEqual("No missing groups remain", Result.MissingGroups.Num(), 0);
  TestEqual("No transcript errors remain", CountErrorEntries(Result.Transcript),
            0);
  TestTrue("Summary reports completion",
           Result.Summary.Contains(TEXT("ALL_BINDINGS_COMPLETE")));

  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTestGameRunGameTranscriptErrorFailsTest,
    "ForbocAI.Integration.TestGame.RunGame.TranscriptErrorFails",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FTestGameRunGameTranscriptErrorFailsTest::RunTest(
    const FString &Parameters) {
  (void)Parameters;
  AddExpectedError(TEXT("LOG_ERR_CRITICAL // BIT_ROT_DETECTED"),
                   EAutomationExpectedErrorFlags::Contains, 1);
  AddExpectedError(TEXT("stub failure"),
                   EAutomationExpectedErrorFlags::Contains, 1);

  const auto Result =
      RunGame(EPlayMode::Autoplay,
              MakeExecutor(TEXT("forbocai npc state doomguard")));

  TestFalse("Run fails when any transcript entry is an error", Result.bComplete);
  TestEqual("Coverage can still be complete", Result.MissingGroups.Num(), 0);
  TestEqual("Exactly one transcript error is recorded",
            CountErrorEntries(Result.Transcript), 1);
  TestTrue("Summary reports transcript errors",
           Result.Summary.Contains(TEXT("1 transcript error")));

  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTestGameRunGameFailedCommandLeavesCoverageGapTest,
    "ForbocAI.Integration.TestGame.RunGame.FailedCommandLeavesCoverageGap",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FTestGameRunGameFailedCommandLeavesCoverageGapTest::RunTest(
    const FString &Parameters) {
  (void)Parameters;
  AddExpectedError(TEXT("LOG_ERR_CRITICAL // BIT_ROT_DETECTED"),
                   EAutomationExpectedErrorFlags::Contains, 1);
  AddExpectedError(TEXT("stub failure"),
                   EAutomationExpectedErrorFlags::Contains, 1);

  const auto Result =
      RunGame(EPlayMode::Autoplay, MakeExecutor(TEXT("forbocai status")));

  TestFalse("Run fails when a unique coverage command errors", Result.bComplete);
  TestTrue("Status remains uncovered after a failed status command",
           ContainsMissingGroup(Result.MissingGroups, ECommandGroup::Status));
  TestEqual("Exactly one transcript error is recorded",
            CountErrorEntries(Result.Transcript), 1);
  TestTrue("Summary reports missing groups",
           Result.Summary.Contains(TEXT("Missing groups")));

  return true;
}
