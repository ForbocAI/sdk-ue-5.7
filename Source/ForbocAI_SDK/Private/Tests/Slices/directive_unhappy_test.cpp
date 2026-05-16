#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "DirectiveSlice.h"
#include "Misc/AutomationTest.h"
#include "Protocol/ProtocolRequestTypes.h"

using namespace rtk;
using namespace DirectiveSlice;

/**
 * Test: RunFailed on a non-existent directive is a no-op
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectiveFailNonExistentTest,
                                 "ForbocAI.Slices.Directive.FailNonExistent",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FDirectiveFailNonExistentTest::RunTest(const FString &Parameters) {
  Slice<FDirectiveSliceState> DirSlice = CreateDirectiveSlice();
  FDirectiveSliceState State;

  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunFailed(TEXT("does_not_exist"),
                                                  TEXT("phantom error")));

  TestEqual("No directives created", SelectAllDirectives(State).Num(), 0);
  TestFalse("Non-existent id still not found",
            SelectDirectiveById(State, TEXT("does_not_exist")).hasValue);
  return true;
}

/**
 * Test: VerdictValidated on a non-existent directive is a no-op
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectiveVerdictNonExistentTest,
                                 "ForbocAI.Slices.Directive.VerdictNonExistent",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FDirectiveVerdictNonExistentTest::RunTest(const FString &Parameters) {
  Slice<FDirectiveSliceState> DirSlice = CreateDirectiveSlice();
  FDirectiveSliceState State;

  FVerdictResponse Verdict;
  Verdict.bValid = true;
  Verdict.Dialogue = TEXT("orphan verdict");
  Verdict.bHasAction = false;

  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::VerdictValidated(TEXT("ghost_id"), Verdict));

  TestEqual("No directives created", SelectAllDirectives(State).Num(), 0);
  return true;
}

/**
 * Test: DirectiveReceived on a non-existent directive is a no-op
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectiveReceivedNonExistentTest,
                                 "ForbocAI.Slices.Directive.ReceivedNonExistent",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FDirectiveReceivedNonExistentTest::RunTest(const FString &Parameters) {
  Slice<FDirectiveSliceState> DirSlice = CreateDirectiveSlice();
  FDirectiveSliceState State;

  FDirectiveResponse Response;
  Response.MemoryRecall.Query = TEXT("lost context");

  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveReceived(TEXT("missing_id"), Response));

  TestEqual("No directives created", SelectAllDirectives(State).Num(), 0);
  return true;
}

/**
 * Test: Duplicate DirectiveRunStarted with same ID overwrites
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectiveDuplicateStartTest,
                                 "ForbocAI.Slices.Directive.DuplicateStart",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FDirectiveDuplicateStartTest::RunTest(const FString &Parameters) {
  Slice<FDirectiveSliceState> DirSlice = CreateDirectiveSlice();
  FDirectiveSliceState State;

  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(TEXT("dup"), TEXT("npc1"),
                                                   TEXT("First")));
  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(TEXT("dup"), TEXT("npc2"),
                                                   TEXT("Second")));

  func::Maybe<FDirectiveRun> Found = SelectDirectiveById(State, TEXT("dup"));
  TestTrue("Directive exists", Found.hasValue);
  if (Found.hasValue) {
    TestEqual("Overwritten NpcId", Found.value.NpcId, FString(TEXT("npc2")));
    TestEqual("Overwritten Observation", Found.value.Observation,
              FString(TEXT("Second")));
    TestEqual("Status reset to Running",
              static_cast<int32>(Found.value.Status),
              static_cast<int32>(EDirectiveStatus::Running));
  }

  return true;
}

/**
 * Test: Failed directive then re-started with same ID resets state
 * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectiveFailThenRestartTest,
                                 "ForbocAI.Slices.Directive.FailThenRestart",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FDirectiveFailThenRestartTest::RunTest(const FString &Parameters) {
  Slice<FDirectiveSliceState> DirSlice = CreateDirectiveSlice();
  FDirectiveSliceState State;

  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(TEXT("r1"), TEXT("npc_a"),
                                                   TEXT("Try 1")));
  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunFailed(TEXT("r1"),
                                                  TEXT("Timeout")));

  func::Maybe<FDirectiveRun> Failed = SelectDirectiveById(State, TEXT("r1"));
  TestTrue("Failed exists", Failed.hasValue);
  if (Failed.hasValue) {
    TestEqual("Status Failed",
              static_cast<int32>(Failed.value.Status),
              static_cast<int32>(EDirectiveStatus::Failed));
  }

  /**
   * Re-start same ID
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(TEXT("r1"), TEXT("npc_a"),
                                                   TEXT("Try 2")));

  func::Maybe<FDirectiveRun> Restarted = SelectDirectiveById(State, TEXT("r1"));
  TestTrue("Restarted exists", Restarted.hasValue);
  if (Restarted.hasValue) {
    TestEqual("Status Running again",
              static_cast<int32>(Restarted.value.Status),
              static_cast<int32>(EDirectiveStatus::Running));
    TestEqual("Observation updated", Restarted.value.Observation,
              FString(TEXT("Try 2")));
    TestTrue("Error cleared", Restarted.value.Error.IsEmpty());
  }

  return true;
}

/**
 * Test: ClearDirectivesForNpc with no matching NPCs is a no-op
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectiveClearNoMatchTest,
                                 "ForbocAI.Slices.Directive.ClearNoMatch",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FDirectiveClearNoMatchTest::RunTest(const FString &Parameters) {
  Slice<FDirectiveSliceState> DirSlice = CreateDirectiveSlice();
  FDirectiveSliceState State;

  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(TEXT("d1"), TEXT("npc_a"),
                                                   TEXT("obs")));

  State = DirSlice.Reducer(
      State, DirectiveSlice::Actions::ClearDirectivesForNpc(
                 TEXT("npc_nonexistent")));

  TestEqual("Original directive remains", SelectAllDirectives(State).Num(), 1);
  TestTrue("d1 still found", SelectDirectiveById(State, TEXT("d1")).hasValue);

  return true;
}

/**
 * Test: VerdictValidated with invalid verdict sets bVerdictValid false
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectiveInvalidVerdictTest,
                                 "ForbocAI.Slices.Directive.InvalidVerdict",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FDirectiveInvalidVerdictTest::RunTest(const FString &Parameters) {
  Slice<FDirectiveSliceState> DirSlice = CreateDirectiveSlice();
  FDirectiveSliceState State;

  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(TEXT("iv"), TEXT("npc1"),
                                                   TEXT("obs")));

  FVerdictResponse Verdict;
  Verdict.bValid = false;
  Verdict.Dialogue = TEXT("Action blocked by bridge");
  Verdict.bHasAction = true;
  Verdict.Action.Type = TEXT("ATTACK");
  Verdict.Action.Target = TEXT("civilian");

  State = DirSlice.Reducer(
      State, DirectiveSlice::Actions::VerdictValidated(TEXT("iv"), Verdict));

  func::Maybe<FDirectiveRun> Found = SelectDirectiveById(State, TEXT("iv"));
  TestTrue("Run exists", Found.hasValue);
  if (Found.hasValue) {
    TestFalse("bVerdictValid is false", Found.value.bVerdictValid);
    TestEqual("VerdictDialogue set", Found.value.VerdictDialogue,
              FString(TEXT("Action blocked by bridge")));
  }

  return true;
}
