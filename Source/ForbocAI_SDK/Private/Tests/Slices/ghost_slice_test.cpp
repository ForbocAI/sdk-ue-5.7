#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "Ghost/GhostSlice.h"
#include "Misc/AutomationTest.h"

using namespace rtk;
using namespace GhostSlice;

/**
 * Test: GhostSessionStarted sets session and status
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGhostSliceSessionStartedTest,
                                 "ForbocAI.Slices.Ghost.SessionStarted",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FGhostSliceSessionStartedTest::RunTest(const FString &Parameters) {
  Slice<FGhostSliceState> GSlice = CreateGhostSlice();
  FGhostSliceState State;

  TestEqual("Initial status idle", State.Status, FString(TEXT("idle")));

  State = GSlice.Reducer(
      State, GhostSlice::Actions::GhostSessionStarted(TEXT("sess_01"), TEXT("running")));

  TestEqual("ActiveSessionId set", State.ActiveSessionId,
            FString(TEXT("sess_01")));
  TestEqual("Status running", State.Status, FString(TEXT("running")));
  TestEqual("Progress zero", State.Progress, 0.0f);
  TestFalse("No results yet", State.bHasResults);
  TestTrue("Error empty", State.Error.IsEmpty());

  return true;
}

/**
 * Test: GhostProgress updates progress
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGhostSliceProgressTest,
                                 "ForbocAI.Slices.Ghost.Progress",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FGhostSliceProgressTest::RunTest(const FString &Parameters) {
  Slice<FGhostSliceState> GSlice = CreateGhostSlice();
  FGhostSliceState State;

  State = GSlice.Reducer(
      State, GhostSlice::Actions::GhostSessionStarted(TEXT("sess_p"), TEXT("running")));
  State = GSlice.Reducer(
      State,
      GhostSlice::Actions::GhostSessionProgress(TEXT("sess_p"), TEXT("running"), 0.5f));

  TestEqual("Progress updated to 0.5", State.Progress, 0.5f);
  TestEqual("SessionId preserved", State.ActiveSessionId,
            FString(TEXT("sess_p")));
  TestEqual("Status still running", State.Status, FString(TEXT("running")));

  return true;
}

/**
 * Test: GhostSessionCompleted sets results
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGhostSliceCompletedTest,
                                 "ForbocAI.Slices.Ghost.SessionCompleted",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FGhostSliceCompletedTest::RunTest(const FString &Parameters) {
  Slice<FGhostSliceState> GSlice = CreateGhostSlice();
  FGhostSliceState State;

  State = GSlice.Reducer(
      State, GhostSlice::Actions::GhostSessionStarted(TEXT("sess_c"), TEXT("running")));

  FGhostTestReport Report;
  FGhostTestResult Result;
  Result.Scenario = TEXT("combat_test");
  Result.bPassed = true;
  Report.Results.Add(Result);

  State = GSlice.Reducer(State, GhostSlice::Actions::GhostSessionCompleted(Report));

  TestEqual("Status completed", State.Status, FString(TEXT("completed")));
  TestEqual("Progress 1.0", State.Progress, 1.0f);
  TestTrue("Has results", State.bHasResults);
  TestEqual("Results count", State.Results.Results.Num(), 1);
  TestFalse("Not loading", State.bLoading);

  return true;
}

/**
 * Test: GhostSessionFailed sets error
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGhostSliceFailedTest,
                                 "ForbocAI.Slices.Ghost.SessionFailed",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FGhostSliceFailedTest::RunTest(const FString &Parameters) {
  Slice<FGhostSliceState> GSlice = CreateGhostSlice();
  FGhostSliceState State;

  State = GSlice.Reducer(
      State, GhostSlice::Actions::GhostSessionStarted(TEXT("sess_f"), TEXT("running")));
  State = GSlice.Reducer(
      State,
      GhostSlice::Actions::GhostSessionFailed(TEXT("sess_f"), TEXT("Scenario crash")));

  TestEqual("Status failed", State.Status, FString(TEXT("failed")));
  TestEqual("Error set", State.Error, FString(TEXT("Scenario crash")));
  TestEqual("SessionId preserved", State.ActiveSessionId,
            FString(TEXT("sess_f")));
  TestFalse("Not loading", State.bLoading);

  return true;
}

/**
 * Test: GhostHistoryLoaded populates history
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGhostSliceHistoryTest,
                                 "ForbocAI.Slices.Ghost.HistoryLoaded",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FGhostSliceHistoryTest::RunTest(const FString &Parameters) {
  Slice<FGhostSliceState> GSlice = CreateGhostSlice();
  FGhostSliceState State;

  TArray<FGhostHistoryEntry> History;
  FGhostHistoryEntry Entry;
  Entry.SessionId = TEXT("old_sess");
  History.Add(Entry);

  State = GSlice.Reducer(State, GhostSlice::Actions::GhostHistoryLoaded(History));

  TestEqual("History count", State.History.Num(), 1);
  TestEqual("History entry sessionId", State.History[0].SessionId,
            FString(TEXT("old_sess")));

  return true;
}
