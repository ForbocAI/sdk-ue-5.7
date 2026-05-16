#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "DirectiveSlice.h"
#include "Misc/AutomationTest.h"
#include "Protocol/ProtocolRequestTypes.h"

using namespace rtk;
using namespace DirectiveSlice;

/**
 * Test: Full happy path — Started → Received → ContextComposed → Validated
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectiveHappyPathTest,
                                 "ForbocAI.Slices.Directive.HappyPath",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FDirectiveHappyPathTest::RunTest(const FString &Parameters) {
  Slice<FDirectiveSliceState> DirSlice = CreateDirectiveSlice();
  FDirectiveSliceState State;

  /**
   * Step 1: Start directive
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(
          TEXT("hp_1"), TEXT("npc_knight"), TEXT("Player attacks goblin")));

  func::Maybe<FDirectiveRun> Run = SelectDirectiveById(State, TEXT("hp_1"));
  TestTrue("Run created", Run.hasValue);
  if (Run.hasValue) {
    TestEqual("Status Running",
              static_cast<int32>(Run.value.Status),
              static_cast<int32>(EDirectiveStatus::Running));
    TestEqual("NpcId set", Run.value.NpcId, FString(TEXT("npc_knight")));
    TestEqual("Observation set", Run.value.Observation,
              FString(TEXT("Player attacks goblin")));
  }
  TestEqual("Active directive", State.ActiveDirectiveId,
            FString(TEXT("hp_1")));

  /**
   * Step 2: Directive received (memory recall instruction)
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  FDirectiveResponse DirResponse;
  DirResponse.MemoryRecall.Query = TEXT("goblin encounters");
  DirResponse.MemoryRecall.Limit = 5;
  DirResponse.MemoryRecall.Threshold = 0.6f;

  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveReceived(TEXT("hp_1"), DirResponse));

  Run = SelectDirectiveById(State, TEXT("hp_1"));
  TestTrue("Run still exists", Run.hasValue);
  if (Run.hasValue) {
    TestEqual("MemoryRecallQuery set", Run.value.MemoryRecallQuery,
              FString(TEXT("goblin encounters")));
    TestEqual("MemoryRecallLimit", Run.value.MemoryRecallLimit, 5);
  }

  /**
   * Step 3: Context composed
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  FCortexConfig Constraints;
  Constraints.MaxTokens = 256;
  Constraints.Temperature = 0.7f;

  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::ContextComposed(
          TEXT("hp_1"), TEXT("You are a knight facing a goblin..."),
          Constraints));

  Run = SelectDirectiveById(State, TEXT("hp_1"));
  TestTrue("Run exists after context", Run.hasValue);
  if (Run.hasValue) {
    TestEqual("ContextPrompt set", Run.value.ContextPrompt,
              FString(TEXT("You are a knight facing a goblin...")));
  }

  /**
   * Step 4: Verdict validated (valid)
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  FVerdictResponse Verdict;
  Verdict.bValid = true;
  Verdict.Dialogue = TEXT("You swing your sword at the goblin!");
  Verdict.bHasAction = true;
  Verdict.Action.Type = TEXT("ATTACK");
  Verdict.Action.Target = TEXT("goblin");
  Verdict.Action.Reason = TEXT("Combat engagement");

  State = DirSlice.Reducer(
      State, DirectiveSlice::Actions::VerdictValidated(TEXT("hp_1"), Verdict));

  Run = SelectDirectiveById(State, TEXT("hp_1"));
  TestTrue("Run exists after verdict", Run.hasValue);
  if (Run.hasValue) {
    TestEqual("Status Completed",
              static_cast<int32>(Run.value.Status),
              static_cast<int32>(EDirectiveStatus::Completed));
    TestTrue("Verdict valid", Run.value.bVerdictValid);
    TestEqual("Dialogue matches", Run.value.VerdictDialogue,
              FString(TEXT("You swing your sword at the goblin!")));
    TestEqual("Action type", Run.value.VerdictActionType,
              FString(TEXT("ATTACK")));
  }

  return true;
}

/**
 * Test: Multiple concurrent directives maintain independent state
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectiveMultipleTest,
                                 "ForbocAI.Slices.Directive.MultipleConcurrent",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FDirectiveMultipleTest::RunTest(const FString &Parameters) {
  Slice<FDirectiveSliceState> DirSlice = CreateDirectiveSlice();
  FDirectiveSliceState State;

  /**
   * Start two directives for different NPCs
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(TEXT("d_a"), TEXT("npc_1"),
                                                   TEXT("obs_a")));
  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(TEXT("d_b"), TEXT("npc_2"),
                                                   TEXT("obs_b")));

  TestEqual("Two directives", SelectAllDirectives(State).Num(), 2);
  TestEqual("Active is last started", State.ActiveDirectiveId,
            FString(TEXT("d_b")));

  /**
   * Fail first, second stays running
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunFailed(TEXT("d_a"),
                                                  TEXT("API timeout")));

  auto A = SelectDirectiveById(State, TEXT("d_a"));
  auto B = SelectDirectiveById(State, TEXT("d_b"));
  TestTrue("d_a exists", A.hasValue);
  TestTrue("d_b exists", B.hasValue);
  if (A.hasValue && B.hasValue) {
    TestEqual("d_a Failed",
              static_cast<int32>(A.value.Status),
              static_cast<int32>(EDirectiveStatus::Failed));
    TestEqual("d_b still Running",
              static_cast<int32>(B.value.Status),
              static_cast<int32>(EDirectiveStatus::Running));
  }

  return true;
}

/**
 * Test: ClearDirectivesForNpc removes only matching NPC directives
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectiveClearForNpcTest,
                                 "ForbocAI.Slices.Directive.ClearForNpcSelective",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FDirectiveClearForNpcTest::RunTest(const FString &Parameters) {
  Slice<FDirectiveSliceState> DirSlice = CreateDirectiveSlice();
  FDirectiveSliceState State;

  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(TEXT("d1"),
                                                   TEXT("npc_target"),
                                                   TEXT("obs1")));
  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(TEXT("d2"),
                                                   TEXT("npc_keep"),
                                                   TEXT("obs2")));
  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(TEXT("d3"),
                                                   TEXT("npc_target"),
                                                   TEXT("obs3")));

  TestEqual("Three directives", SelectAllDirectives(State).Num(), 3);

  /**
   * Clear only npc_target directives
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  State = DirSlice.Reducer(
      State, DirectiveSlice::Actions::ClearDirectivesForNpc(TEXT("npc_target")));

  TestEqual("One directive remains", SelectAllDirectives(State).Num(), 1);
  TestFalse("d1 removed", SelectDirectiveById(State, TEXT("d1")).hasValue);
  TestTrue("d2 kept", SelectDirectiveById(State, TEXT("d2")).hasValue);
  TestFalse("d3 removed", SelectDirectiveById(State, TEXT("d3")).hasValue);

  return true;
}
