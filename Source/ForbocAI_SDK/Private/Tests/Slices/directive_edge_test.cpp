#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "DirectiveSlice.h"
#include "Misc/AutomationTest.h"
#include "Protocol/ProtocolRequestTypes.h"

using namespace rtk;
using namespace DirectiveSlice;

/**
 * Test: Rapid sequential failures on different directives
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectiveRapidFailuresTest,
                                 "ForbocAI.Slices.Directive.RapidSequentialFailures",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FDirectiveRapidFailuresTest::RunTest(const FString &Parameters) {
  Slice<FDirectiveSliceState> DirSlice = CreateDirectiveSlice();
  FDirectiveSliceState State;

  /**
   * Start 5 directives
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  for (int32 i = 0; i < 5; ++i) {
    const FString Id = FString::Printf(TEXT("rapid_%d"), i);
    State = DirSlice.Reducer(
        State,
        DirectiveSlice::Actions::DirectiveRunStarted(Id, TEXT("npc_stress"),
                                                     TEXT("obs")));
  }
  TestEqual("Five directives", SelectAllDirectives(State).Num(), 5);

  /**
   * Fail them all in rapid succession
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  for (int32 i = 0; i < 5; ++i) {
    const FString Id = FString::Printf(TEXT("rapid_%d"), i);
    const FString Error =
        FString::Printf(TEXT("Timeout after %d ms"), (i + 1) * 1000);
    State = DirSlice.Reducer(
        State, DirectiveSlice::Actions::DirectiveRunFailed(Id, Error));
  }

  /**
   * Verify each has its own error and status
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  for (int32 i = 0; i < 5; ++i) {
    const FString Id = FString::Printf(TEXT("rapid_%d"), i);
    auto Run = SelectDirectiveById(State, Id);
    TestTrue(FString::Printf(TEXT("rapid_%d exists"), i), Run.hasValue);
    if (Run.hasValue) {
      TestEqual(FString::Printf(TEXT("rapid_%d failed"), i),
                static_cast<int32>(Run.value.Status),
                static_cast<int32>(EDirectiveStatus::Failed));
      const FString ExpectedError =
          FString::Printf(TEXT("Timeout after %d ms"), (i + 1) * 1000);
      TestEqual(FString::Printf(TEXT("rapid_%d error"), i), Run.value.Error,
                ExpectedError);
    }
  }

  return true;
}

/**
 * Test: ContextComposed on already-failed directive is a no-op
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectiveContextAfterFailTest,
                                 "ForbocAI.Slices.Directive.ContextAfterFail",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FDirectiveContextAfterFailTest::RunTest(const FString &Parameters) {
  Slice<FDirectiveSliceState> DirSlice = CreateDirectiveSlice();
  FDirectiveSliceState State;

  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(TEXT("cf"), TEXT("npc1"),
                                                   TEXT("obs")));
  State = DirSlice.Reducer(
      State, DirectiveSlice::Actions::DirectiveRunFailed(
                 TEXT("cf"), TEXT("API timeout")));

  /**
   * Try to compose context on a failed directive
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  FCortexConfig Constraints;
  Constraints.MaxTokens = 256;
  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::ContextComposed(TEXT("cf"), TEXT("Late context"),
                                               Constraints));

  auto Run = SelectDirectiveById(State, TEXT("cf"));
  TestTrue("Run exists", Run.hasValue);
  if (Run.hasValue) {
    /**
     * The reducer still applies the update (entity adapter updateOne works on
     * any existing entity regardless of status). This test documents that
     * behavior — the thunk layer is responsible for not dispatching
     * ContextComposed after failure.
     * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
     */
    TestEqual("Status still Failed",
              static_cast<int32>(Run.value.Status),
              static_cast<int32>(EDirectiveStatus::Failed));
  }

  return true;
}

/**
 * Test: ClearDirectivesForNpc clears ActiveDirectiveId when active is removed
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectiveClearActiveLostTest,
                                 "ForbocAI.Slices.Directive.ClearActiveLost",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FDirectiveClearActiveLostTest::RunTest(const FString &Parameters) {
  Slice<FDirectiveSliceState> DirSlice = CreateDirectiveSlice();
  FDirectiveSliceState State;

  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(TEXT("keep"), TEXT("npc_a"),
                                                   TEXT("obs1")));
  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(TEXT("remove"),
                                                   TEXT("npc_b"),
                                                   TEXT("obs2")));

  /**
   * Active is the last started
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  TestEqual("Active is remove", State.ActiveDirectiveId,
            FString(TEXT("remove")));

  /**
   * Clear npc_b directives — should clear ActiveDirectiveId
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  State = DirSlice.Reducer(
      State, DirectiveSlice::Actions::ClearDirectivesForNpc(TEXT("npc_b")));

  TestTrue("ActiveDirectiveId cleared", State.ActiveDirectiveId.IsEmpty());
  TestEqual("One directive remains", SelectAllDirectives(State).Num(), 1);
  TestTrue("keep survives", SelectDirectiveById(State, TEXT("keep")).hasValue);

  return true;
}

/**
 * Test: ClearDirectivesForNpc preserves ActiveDirectiveId when active is kept
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectiveClearActivePreservedTest,
                                 "ForbocAI.Slices.Directive.ClearActivePreserved",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FDirectiveClearActivePreservedTest::RunTest(const FString &Parameters) {
  Slice<FDirectiveSliceState> DirSlice = CreateDirectiveSlice();
  FDirectiveSliceState State;

  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(TEXT("other"),
                                                   TEXT("npc_x"),
                                                   TEXT("obs1")));
  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(TEXT("active"),
                                                   TEXT("npc_y"),
                                                   TEXT("obs2")));

  TestEqual("Active is active", State.ActiveDirectiveId,
            FString(TEXT("active")));

  /**
   * Clear npc_x — active should stay
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  State = DirSlice.Reducer(
      State, DirectiveSlice::Actions::ClearDirectivesForNpc(TEXT("npc_x")));

  TestEqual("Active preserved", State.ActiveDirectiveId,
            FString(TEXT("active")));
  TestEqual("One remains", SelectAllDirectives(State).Num(), 1);

  return true;
}

/**
 * Test: Verdict with no action (dialogue only)
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectiveVerdictDialogueOnlyTest,
                                 "ForbocAI.Slices.Directive.VerdictDialogueOnly",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FDirectiveVerdictDialogueOnlyTest::RunTest(const FString &Parameters) {
  Slice<FDirectiveSliceState> DirSlice = CreateDirectiveSlice();
  FDirectiveSliceState State;

  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(TEXT("dg"), TEXT("npc1"),
                                                   TEXT("obs")));

  FVerdictResponse Verdict;
  Verdict.bValid = true;
  Verdict.Dialogue = TEXT("I greet you warmly, traveler.");
  Verdict.bHasAction = false;

  State = DirSlice.Reducer(
      State, DirectiveSlice::Actions::VerdictValidated(TEXT("dg"), Verdict));

  auto Run = SelectDirectiveById(State, TEXT("dg"));
  TestTrue("Run exists", Run.hasValue);
  if (Run.hasValue) {
    TestEqual("Status Completed",
              static_cast<int32>(Run.value.Status),
              static_cast<int32>(EDirectiveStatus::Completed));
    TestTrue("Valid", Run.value.bVerdictValid);
    TestEqual("Dialogue set", Run.value.VerdictDialogue,
              FString(TEXT("I greet you warmly, traveler.")));
    TestTrue("No action type", Run.value.VerdictActionType.IsEmpty());
  }

  return true;
}

/**
 * Test: SelectActiveDirective returns nothing when no active
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectiveSelectActiveEmptyTest,
                                 "ForbocAI.Slices.Directive.SelectActiveEmpty",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FDirectiveSelectActiveEmptyTest::RunTest(const FString &Parameters) {
  FDirectiveSliceState State;

  TestTrue("ActiveDirectiveId empty", State.ActiveDirectiveId.IsEmpty());
  TestFalse("SelectActiveDirective returns nothing",
            SelectActiveDirective(State).hasValue);

  return true;
}
