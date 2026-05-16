#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "NPC/NPCSlice.h"
#include "NPC/NPCSliceActions.h"

using namespace rtk;
using namespace NPCSlice;

/**
 * Test: SetNPCInfo dispatches and updates state
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPCSliceSetInfoTest,
                                 "ForbocAI.Slices.NPC.SetNPCInfo",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FNPCSliceSetInfoTest::RunTest(const FString &Parameters) {
  Slice<FNPCSliceState> NpcSlice = CreateNPCSlice();

  FNPCSliceState State;
  FNPCInternalState Info;
  Info.Id = TEXT("npc_001");
  Info.Persona = TEXT("A brave knight");

  State = NpcSlice.Reducer(State, NPCSlice::Actions::SetNPCInfo(Info));

  TestEqual("ActiveNpcId set", State.ActiveNpcId, FString(TEXT("npc_001")));

  func::Maybe<FNPCInternalState> Found = SelectNPCById(State, TEXT("npc_001"));
  TestTrue("NPC found in entities", Found.hasValue);
  if (Found.hasValue) {
    TestEqual("NPC Id matches", Found.value.Id, FString(TEXT("npc_001")));
    TestEqual("NPC Persona matches", Found.value.Persona,
              FString(TEXT("A brave knight")));
    TestTrue("StateLog has initial entry", Found.value.StateLog.Num() > 0);
  }

  /**
   * Add a second NPC
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  FNPCInternalState Info2;
  Info2.Id = TEXT("npc_002");
  Info2.Persona = TEXT("A sly rogue");
  State = NpcSlice.Reducer(State, NPCSlice::Actions::SetNPCInfo(Info2));

  TestEqual("ActiveNpcId updated to second", State.ActiveNpcId,
            FString(TEXT("npc_002")));

  TArray<FNPCInternalState> AllNpcs = SelectAllNPCs(State);
  TestEqual("Two NPCs in state", AllNpcs.Num(), 2);

  return true;
}

/**
 * Test: RemoveNPC dispatches and clears state
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPCSliceRemoveTest,
                                 "ForbocAI.Slices.NPC.RemoveNPC",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FNPCSliceRemoveTest::RunTest(const FString &Parameters) {
  Slice<FNPCSliceState> NpcSlice = CreateNPCSlice();
  FNPCSliceState State;

  FNPCInternalState Info;
  Info.Id = TEXT("npc_rm");
  Info.Persona = TEXT("Doomed NPC");
  State = NpcSlice.Reducer(State, NPCSlice::Actions::SetNPCInfo(Info));

  TestTrue("NPC exists before removal",
           SelectNPCById(State, TEXT("npc_rm")).hasValue);
  TestEqual("ActiveNpcId is npc_rm", State.ActiveNpcId,
            FString(TEXT("npc_rm")));

  State = NpcSlice.Reducer(State, NPCSlice::Actions::RemoveNPC(TEXT("npc_rm")));

  TestFalse("NPC removed from entities",
            SelectNPCById(State, TEXT("npc_rm")).hasValue);
  TestTrue("ActiveNpcId cleared", State.ActiveNpcId.IsEmpty());

  TArray<FNPCInternalState> AllNpcs = SelectAllNPCs(State);
  TestEqual("No NPCs remain", AllNpcs.Num(), 0);

  return true;
}

/**
 * Test: Selectors — SelectActiveNPC, SelectAllNPCs, SelectNPCById
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNPCSliceSelectorsTest,
                                 "ForbocAI.Slices.NPC.Selectors",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FNPCSliceSelectorsTest::RunTest(const FString &Parameters) {
  Slice<FNPCSliceState> NpcSlice = CreateNPCSlice();
  FNPCSliceState State;

  /**
   * Empty state selectors
   * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
   */
  func::Maybe<FNPCInternalState> EmptyActive = SelectActiveNPC(State);
  TestFalse("No active NPC on empty state", EmptyActive.hasValue);
  TestEqual("SelectAllNPCs empty", SelectAllNPCs(State).Num(), 0);
  TestFalse("SelectNPCById returns nothing on empty",
            SelectNPCById(State, TEXT("ghost")).hasValue);

  /**
   * Add NPCs
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  FNPCInternalState A;
  A.Id = TEXT("sel_a");
  A.Persona = TEXT("Alpha");
  State = NpcSlice.Reducer(State, NPCSlice::Actions::SetNPCInfo(A));

  FNPCInternalState B;
  B.Id = TEXT("sel_b");
  B.Persona = TEXT("Beta");
  State = NpcSlice.Reducer(State, NPCSlice::Actions::SetNPCInfo(B));

  /**
   * Active should be last set
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  func::Maybe<FNPCInternalState> Active = SelectActiveNPC(State);
  TestTrue("Active NPC exists", Active.hasValue);
  if (Active.hasValue) {
    TestEqual("Active is sel_b", Active.value.Id, FString(TEXT("sel_b")));
  }

  /**
   * SelectNPCById for first NPC
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  func::Maybe<FNPCInternalState> FoundA = SelectNPCById(State, TEXT("sel_a"));
  TestTrue("sel_a found", FoundA.hasValue);
  if (FoundA.hasValue) {
    TestEqual("sel_a persona", FoundA.value.Persona, FString(TEXT("Alpha")));
  }

  /**
   * SelectAllNPCs
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  TestEqual("Two NPCs total", SelectAllNPCs(State).Num(), 2);

  /**
   * SetActiveNPC to sel_a
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  State = NpcSlice.Reducer(State, NPCSlice::Actions::SetActiveNPC(TEXT("sel_a")));
  TestEqual("Active switched to sel_a", SelectActiveNpcId(State),
            FString(TEXT("sel_a")));

  return true;
}
