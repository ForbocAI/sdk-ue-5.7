#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "DirectiveSlice.h"
#include "Misc/AutomationTest.h"
#include "Protocol/ProtocolRequestTypes.h"

using namespace rtk;
using namespace DirectiveSlice;

/**
 * Test: DirectiveRunStarted creates run and sets ActiveDirectiveId
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectiveSliceRunStartedTest,
                                 "ForbocAI.Slices.Directive.RunStarted",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FDirectiveSliceRunStartedTest::RunTest(const FString &Parameters) {
  Slice<FDirectiveSliceState> DirSlice = CreateDirectiveSlice();
  FDirectiveSliceState State;

  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(
          TEXT("dir_1"), TEXT("npc_a"), TEXT("Player said hello")));

  TestEqual("ActiveDirectiveId set", State.ActiveDirectiveId,
            FString(TEXT("dir_1")));

  func::Maybe<FDirectiveRun> Found =
      SelectDirectiveById(State, TEXT("dir_1"));
  TestTrue("Directive run found", Found.hasValue);
  if (Found.hasValue) {
    TestEqual("Run Id", Found.value.Id, FString(TEXT("dir_1")));
    TestEqual("Run NpcId", Found.value.NpcId, FString(TEXT("npc_a")));
    TestEqual("Observation", Found.value.Observation,
              FString(TEXT("Player said hello")));
    TestEqual("Status Running",
              static_cast<int32>(Found.value.Status),
              static_cast<int32>(EDirectiveStatus::Running));
  }

  func::Maybe<FDirectiveRun> Active = SelectActiveDirective(State);
  TestTrue("SelectActiveDirective returns run", Active.hasValue);
  if (Active.hasValue) {
    TestEqual("Active run Id", Active.value.Id, FString(TEXT("dir_1")));
  }

  return true;
}

/**
 * Test: DirectiveReceived updates run with memory recall
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectiveSliceDirectiveReceivedTest,
                                 "ForbocAI.Slices.Directive.DirectiveReceived",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FDirectiveSliceDirectiveReceivedTest::RunTest(const FString &Parameters) {
  Slice<FDirectiveSliceState> DirSlice = CreateDirectiveSlice();
  FDirectiveSliceState State;

  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(
          TEXT("dir_recv"), TEXT("npc_b"), TEXT("Where is the key?")));

  FDirectiveResponse Response;
  Response.MemoryRecall.Query = TEXT("key location");
  Response.MemoryRecall.Limit = 5;
  Response.MemoryRecall.Threshold = 0.8f;

  State = DirSlice.Reducer(
      State, DirectiveSlice::Actions::DirectiveReceived(TEXT("dir_recv"),
                                                        Response));

  func::Maybe<FDirectiveRun> Found =
      SelectDirectiveById(State, TEXT("dir_recv"));
  TestTrue("Run still exists", Found.hasValue);
  if (Found.hasValue) {
    TestEqual("MemoryRecallQuery", Found.value.MemoryRecallQuery,
              FString(TEXT("key location")));
    TestEqual("MemoryRecallLimit", Found.value.MemoryRecallLimit, 5);
    TestEqual("MemoryRecallThreshold", Found.value.MemoryRecallThreshold, 0.8f);
  }

  return true;
}

/**
 * Test: VerdictValidated completes run
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectiveSliceVerdictValidatedTest,
                                 "ForbocAI.Slices.Directive.VerdictValidated",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FDirectiveSliceVerdictValidatedTest::RunTest(const FString &Parameters) {
  Slice<FDirectiveSliceState> DirSlice = CreateDirectiveSlice();
  FDirectiveSliceState State;

  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(
          TEXT("dir_v"), TEXT("npc_c"), TEXT("Greet the player")));

  FVerdictResponse Verdict;
  Verdict.bValid = true;
  Verdict.Dialogue = TEXT("Hello, traveler!");
  Verdict.bHasAction = false;

  State = DirSlice.Reducer(
      State, DirectiveSlice::Actions::VerdictValidated(TEXT("dir_v"),
                                                       Verdict));

  func::Maybe<FDirectiveRun> Found = SelectDirectiveById(State, TEXT("dir_v"));
  TestTrue("Run exists", Found.hasValue);
  if (Found.hasValue) {
    TestEqual("Status Completed",
              static_cast<int32>(Found.value.Status),
              static_cast<int32>(EDirectiveStatus::Completed));
    TestEqual("VerdictDialogue", Found.value.VerdictDialogue,
              FString(TEXT("Hello, traveler!")));
    TestTrue("bVerdictValid", Found.value.bVerdictValid);
  }

  return true;
}

/**
 * Test: DirectiveRunFailed marks run as failed
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectiveSliceRunFailedTest,
                                 "ForbocAI.Slices.Directive.RunFailed",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FDirectiveSliceRunFailedTest::RunTest(const FString &Parameters) {
  Slice<FDirectiveSliceState> DirSlice = CreateDirectiveSlice();
  FDirectiveSliceState State;

  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(
          TEXT("dir_fail"), TEXT("npc_d"), TEXT("Do something")));
  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunFailed(TEXT("dir_fail"),
                                                  TEXT("API timeout")));

  func::Maybe<FDirectiveRun> Found =
      SelectDirectiveById(State, TEXT("dir_fail"));
  TestTrue("Run exists", Found.hasValue);
  if (Found.hasValue) {
    TestEqual("Status Failed",
              static_cast<int32>(Found.value.Status),
              static_cast<int32>(EDirectiveStatus::Failed));
    TestEqual("Error", Found.value.Error, FString(TEXT("API timeout")));
  }

  return true;
}

/**
 * Test: ClearDirectivesForNpc removes runs for given NPC
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectiveSliceClearForNpcTest,
                                 "ForbocAI.Slices.Directive.ClearForNpc",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FDirectiveSliceClearForNpcTest::RunTest(const FString &Parameters) {
  Slice<FDirectiveSliceState> DirSlice = CreateDirectiveSlice();
  FDirectiveSliceState State;

  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(TEXT("d1"), TEXT("npc_x"),
                                                   TEXT("Obs 1")));
  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(TEXT("d2"), TEXT("npc_y"),
                                                   TEXT("Obs 2")));
  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(TEXT("d3"), TEXT("npc_x"),
                                                   TEXT("Obs 3")));

  TestEqual("Three directives", SelectAllDirectives(State).Num(), 3);
  TestEqual("Active is d3", SelectActiveDirectiveId(State),
            FString(TEXT("d3")));

  State = DirSlice.Reducer(
      State, DirectiveSlice::Actions::ClearDirectivesForNpc(TEXT("npc_x")));

  TArray<FDirectiveRun> Remaining = SelectAllDirectives(State);
  TestEqual("One directive remains", Remaining.Num(), 1);
  TestEqual("Remaining is d2", Remaining[0].Id, FString(TEXT("d2")));
  TestEqual("Active cleared (was d3)", SelectActiveDirectiveId(State),
            FString());

  return true;
}

/**
 * Test: Selectors — SelectDirectiveById, SelectAllDirectives,
 * SelectActiveDirectiveId, SelectActiveDirective
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDirectiveSliceSelectorsTest,
                                 "ForbocAI.Slices.Directive.Selectors",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FDirectiveSliceSelectorsTest::RunTest(const FString &Parameters) {
  Slice<FDirectiveSliceState> DirSlice = CreateDirectiveSlice();
  FDirectiveSliceState State;

  /**
   * Empty state
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  TestFalse("No active directive on empty",
            SelectActiveDirective(State).hasValue);
  TestEqual("SelectActiveDirectiveId empty",
            SelectActiveDirectiveId(State).IsEmpty(), true);
  TestEqual("SelectAllDirectives empty", SelectAllDirectives(State).Num(), 0);
  TestFalse("SelectDirectiveById returns nothing on empty",
            SelectDirectiveById(State, TEXT("ghost")).hasValue);

  /**
   * Add runs
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(TEXT("sel_1"), TEXT("n1"),
                                                   TEXT("A")));
  State = DirSlice.Reducer(
      State,
      DirectiveSlice::Actions::DirectiveRunStarted(TEXT("sel_2"), TEXT("n2"),
                                                   TEXT("B")));

  func::Maybe<FDirectiveRun> Active = SelectActiveDirective(State);
  TestTrue("Active exists", Active.hasValue);
  if (Active.hasValue) {
    TestEqual("Active is sel_2", Active.value.Id, FString(TEXT("sel_2")));
  }
  TestEqual("SelectActiveDirectiveId sel_2",
            SelectActiveDirectiveId(State), FString(TEXT("sel_2")));
  TestEqual("SelectAllDirectives two", SelectAllDirectives(State).Num(), 2);

  func::Maybe<FDirectiveRun> Found1 =
      SelectDirectiveById(State, TEXT("sel_1"));
  TestTrue("sel_1 found", Found1.hasValue);
  if (Found1.hasValue) {
    TestEqual("sel_1 NpcId", Found1.value.NpcId, FString(TEXT("n1")));
  }

  return true;
}
