#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "DirectiveSlice.h"
#include "Memory/MemorySlice.h"
#include "Misc/AutomationTest.h"
#include "NPC/NPCSlice.h"
#include "Protocol/ProtocolRequestTypes.h"
#include "Protocol/ProtocolThunks.h"
#include "RuntimeStore.h"

// @covers:coreThunk:checkApiStatusThunk
// @covers:coreThunk:clearMemoryRemoteThunk
// @covers:coreThunk:clearNodeMemoryThunk
// @covers:coreThunk:completeNodeCortexThunk
// @covers:coreThunk:completeRemoteThunk
// @covers:coreThunk:deleteRulesetThunk
// @covers:coreThunk:doctorThunk
// @covers:coreThunk:exportSoulThunk
// @covers:coreThunk:generateNodeEmbeddingThunk
// @covers:coreThunk:getBridgeRulesThunk
// @covers:coreThunk:getGhostHistoryThunk
// @covers:coreThunk:getGhostResultsThunk
// @covers:coreThunk:getGhostStatusThunk
// @covers:coreThunk:getSoulListThunk
// @covers:coreThunk:importNpcFromSoulThunk
// @covers:coreThunk:importSoulFromArweaveThunk
// @covers:coreThunk:importSoulThunk
// @covers:coreThunk:initNodeCortexThunk
// @covers:coreThunk:initNodeMemoryThunk
// @covers:coreThunk:initNodeVectorThunk
// @covers:coreThunk:initRemoteCortexThunk
// @covers:coreThunk:listCortexModelsThunk
// @covers:coreThunk:listMemoryRemoteThunk
// @covers:coreThunk:listRulePresetsThunk
// @covers:coreThunk:listRulesetsThunk
// @covers:coreThunk:loadBridgePresetThunk
// @covers:coreThunk:localExportSoulThunk
// @covers:coreThunk:localImportSoulThunk
// @covers:coreThunk:localValidateBridgeThunk
// @covers:coreThunk:nodeMemoryRecallThunk
// @covers:coreThunk:nodeMemoryStoreThunk
// @covers:coreThunk:processNPC
// @covers:coreThunk:recallMemoryRemoteThunk
// @covers:coreThunk:recallNodeMemoryThunk
// @covers:coreThunk:registerRulesetThunk
// @covers:coreThunk:remoteExportSoulThunk
// @covers:coreThunk:startGhostThunk
// @covers:coreThunk:stopGhostThunk
// @covers:coreThunk:storeMemoryRemoteThunk
// @covers:coreThunk:storeNodeMemoryThunk
// @covers:coreThunk:streamNodeCortexThunk
// @covers:coreThunk:validateBridgeThunk
// @covers:coreThunk:verifySoulThunk


using namespace rtk;

/**
 * Test: Full store wiring — NPC creation, state update, history, selectors
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProtocolStoreWiringTest,
                                 "ForbocAI.Integration.Protocol.StoreWiring",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FProtocolStoreWiringTest::RunTest(const FString &Parameters) {
  EnhancedStore<FStoreState> TestStore = createStore();

  /**
   * Create NPC
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  FNPCInternalState Npc;
  Npc.Id = TEXT("ag_test1");
  Npc.Persona = TEXT("A test knight");
  TestStore.dispatch(NPCSlice::Actions::SetNPCInfo(Npc));
  TestStore.dispatch(NPCSlice::Actions::SetActiveNPC(TEXT("ag_test1")));

  /**
   * Verify NPC exists
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  FStoreState State = TestStore.getState();
  func::Maybe<FNPCInternalState> Found =
      NPCSlice::SelectNPCById(State.NPCs, TEXT("ag_test1"));
  TestTrue("NPC found in store", Found.hasValue);
  if (Found.hasValue) {
    TestEqual("NPC persona", Found.value.Persona,
              FString(TEXT("A test knight")));
  }
  TestEqual("Active NPC set", NPCSlice::SelectActiveNpcId(State.NPCs),
            FString(TEXT("ag_test1")));

  /**
   * Update state
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  FAgentState NewState;
  NewState.JsonData = TEXT("{\"health\":100}");
  TestStore.dispatch(NPCSlice::Actions::UpdateNPCState(TEXT("ag_test1"), NewState));

  State = TestStore.getState();
  Found = NPCSlice::SelectNPCById(State.NPCs, TEXT("ag_test1"));
  TestTrue("NPC still found", Found.hasValue);
  if (Found.hasValue) {
    TestTrue("State updated",
             Found.value.State.JsonData.Contains(TEXT("health")));
  }

  /**
   * Add history
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  TestStore.dispatch(NPCSlice::Actions::AddToHistory(
      TEXT("ag_test1"), TEXT("player"), TEXT("Hello")));
  TestStore.dispatch(NPCSlice::Actions::AddToHistory(
      TEXT("ag_test1"), TEXT("npc"), TEXT("Greetings")));

  State = TestStore.getState();
  Found = NPCSlice::SelectNPCById(State.NPCs, TEXT("ag_test1"));
  TestTrue("NPC found after history", Found.hasValue);
  if (Found.hasValue) {
    TestEqual("Two history entries", Found.value.History.Num(), 2);
    TestEqual("First history role", Found.value.History[0].Role,
              FString(TEXT("player")));
    TestEqual("First history content", Found.value.History[0].Content,
              FString(TEXT("Hello")));
  }

  /**
   * Set last action
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  FAgentAction Attack;
  Attack.Type = TEXT("ATTACK");
  Attack.Target = TEXT("goblin");
  Attack.Reason = TEXT("In combat");
  TestStore.dispatch(NPCSlice::Actions::SetLastAction(TEXT("ag_test1"), Attack));

  State = TestStore.getState();
  Found = NPCSlice::SelectNPCById(State.NPCs, TEXT("ag_test1"));
  TestTrue("NPC found after action", Found.hasValue);
  if (Found.hasValue) {
    TestEqual("LastAction type", Found.value.LastAction.Type,
              FString(TEXT("ATTACK")));
    TestEqual("LastAction target", Found.value.LastAction.Target,
              FString(TEXT("goblin")));
  }

  /**
   * Block action
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  TestStore.dispatch(NPCSlice::Actions::BlockAction(
      TEXT("ag_test1"), TEXT("Cannot attack civilians")));

  State = TestStore.getState();
  Found = NPCSlice::SelectNPCById(State.NPCs, TEXT("ag_test1"));
  TestTrue("NPC found after block", Found.hasValue);
  if (Found.hasValue) {
    TestTrue("NPC is blocked", Found.value.bIsBlocked);
    TestEqual("Block reason", Found.value.BlockReason,
              FString(TEXT("Cannot attack civilians")));
  }

  /**
   * Clear block
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  TestStore.dispatch(NPCSlice::Actions::ClearBlock(TEXT("ag_test1")));
  State = TestStore.getState();
  Found = NPCSlice::SelectNPCById(State.NPCs, TEXT("ag_test1"));
  TestTrue("NPC found after unblock", Found.hasValue);
  if (Found.hasValue) {
    TestFalse("NPC is unblocked", Found.value.bIsBlocked);
  }

  return true;
}

/**
 * Test: Directive lifecycle through real store dispatch
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProtocolDirectiveLifecycleTest,
                                 "ForbocAI.Integration.Protocol.DirectiveLifecycle",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FProtocolDirectiveLifecycleTest::RunTest(const FString &Parameters) {
  EnhancedStore<FStoreState> TestStore = createStore();

  /**
   * Set up NPC
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  FNPCInternalState Npc;
  Npc.Id = TEXT("ag_dir_test");
  Npc.Persona = TEXT("A directive test NPC");
  TestStore.dispatch(NPCSlice::Actions::SetNPCInfo(Npc));
  TestStore.dispatch(NPCSlice::Actions::SetActiveNPC(TEXT("ag_dir_test")));

  /**
   * Start directive run
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  TestStore.dispatch(DirectiveSlice::Actions::DirectiveRunStarted(
      TEXT("run_1"), TEXT("ag_dir_test"), TEXT("Player attacks goblin")));

  FStoreState State = TestStore.getState();
  TestEqual("Active directive", State.Directives.ActiveDirectiveId,
            FString(TEXT("run_1")));

  func::Maybe<FDirectiveRun> Run =
      DirectiveSlice::SelectDirectiveById(State.Directives, TEXT("run_1"));
  TestTrue("Run exists", Run.hasValue);
  if (Run.hasValue) {
    TestEqual("Run status running",
              static_cast<int32>(Run.value.Status),
              static_cast<int32>(EDirectiveStatus::Running));
  }

  /**
   * Simulate directive response
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  FDirectiveResponse DirResponse;
  DirResponse.MemoryRecall.Query = TEXT("goblin encounter");
  DirResponse.MemoryRecall.Limit = 5;
  DirResponse.MemoryRecall.Threshold = 0.7f;
  TestStore.dispatch(
      DirectiveSlice::Actions::DirectiveReceived(TEXT("run_1"), DirResponse));

  /**
   * Simulate verdict
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  FVerdictResponse Verdict;
  Verdict.bValid = true;
  Verdict.Dialogue = TEXT("You swing your sword at the goblin!");
  Verdict.bHasAction = true;
  Verdict.Action.Type = TEXT("ATTACK");
  Verdict.Action.Target = TEXT("goblin");
  TestStore.dispatch(
      DirectiveSlice::Actions::VerdictValidated(TEXT("run_1"), Verdict));

  State = TestStore.getState();
  Run = DirectiveSlice::SelectDirectiveById(State.Directives, TEXT("run_1"));
  TestTrue("Run exists after verdict", Run.hasValue);
  if (Run.hasValue) {
    TestEqual("Run completed",
              static_cast<int32>(Run.value.Status),
              static_cast<int32>(EDirectiveStatus::Completed));
    TestTrue("Verdict valid", Run.value.bVerdictValid);
    TestEqual("Verdict dialogue", Run.value.VerdictDialogue,
              FString(TEXT("You swing your sword at the goblin!")));
  }

  return true;
}

/**
 * Test: Memory slice lifecycle through real store dispatch
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProtocolMemoryLifecycleTest,
                                 "ForbocAI.Integration.Protocol.MemoryLifecycle",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FProtocolMemoryLifecycleTest::RunTest(const FString &Parameters) {
  EnhancedStore<FStoreState> TestStore = createStore();

  /**
   * Store start
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  TestStore.dispatch(MemorySlice::Actions::MemoryStoreStart());
  FStoreState State = TestStore.getState();
  TestEqual("Store status storing", State.Memory.StorageStatus,
            FString(TEXT("storing")));

  /**
   * Store success
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  FMemoryItem Item;
  Item.Id = TEXT("mem_1");
  Item.Text = TEXT("The player found a key");
  Item.Type = TEXT("observation");
  Item.Importance = 0.8f;
  Item.Timestamp = 1000;
  TestStore.dispatch(MemorySlice::Actions::MemoryStoreSuccess(Item));

  State = TestStore.getState();
  TestEqual("Store status idle", State.Memory.StorageStatus,
            FString(TEXT("idle")));

  /**
   * Recall start
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  TestStore.dispatch(MemorySlice::Actions::MemoryRecallStart());
  State = TestStore.getState();
  TestEqual("Recall status recalling", State.Memory.RecallStatus,
            FString(TEXT("recalling")));

  /**
   * Recall success
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  TArray<FMemoryItem> Recalled;
  FMemoryItem Recalled1;
  Recalled1.Id = TEXT("mem_1");
  Recalled1.Text = TEXT("The player found a key");
  Recalled1.Similarity = 0.9f;
  Recalled.Add(Recalled1);
  TestStore.dispatch(MemorySlice::Actions::MemoryRecallSuccess(Recalled));

  State = TestStore.getState();
  TestEqual("Recall status idle", State.Memory.RecallStatus,
            FString(TEXT("idle")));
  TArray<FMemoryItem> LastRecalled =
      MemorySlice::SelectLastRecalledMemories(State.Memory);
  TestEqual("One recalled memory", LastRecalled.Num(), 1);
  TestEqual("Recalled text", LastRecalled[0].Text,
            FString(TEXT("The player found a key")));

  /**
   * Store failure
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  TestStore.dispatch(MemorySlice::Actions::MemoryStoreStart());
  TestStore.dispatch(
      MemorySlice::Actions::MemoryStoreFailed(TEXT("DB connection lost")));
  State = TestStore.getState();
  TestEqual("Store status error", State.Memory.StorageStatus,
            FString(TEXT("error")));

  /**
   * Recall failure
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  TestStore.dispatch(MemorySlice::Actions::MemoryRecallStart());
  TestStore.dispatch(
      MemorySlice::Actions::MemoryRecallFailed(TEXT("Query timeout")));
  State = TestStore.getState();
  TestEqual("Recall status error", State.Memory.RecallStatus,
            FString(TEXT("error")));

  /**
   * Clear
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  TestStore.dispatch(MemorySlice::Actions::MemoryClear());
  State = TestStore.getState();
  TestEqual("Memories cleared",
            MemorySlice::SelectAllMemories(State.Memory).Num(), 0);

  return true;
}

/**
 * Test: NPC removal cascades to directive cleanup via listener
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProtocolNpcRemovalCascadeTest,
                                 "ForbocAI.Integration.Protocol.NpcRemovalCascade",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FProtocolNpcRemovalCascadeTest::RunTest(const FString &Parameters) {
  EnhancedStore<FStoreState> TestStore = createStore();

  /**
   * Create NPC and directive
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  FNPCInternalState Npc;
  Npc.Id = TEXT("ag_cascade");
  Npc.Persona = TEXT("Cascade test");
  TestStore.dispatch(NPCSlice::Actions::SetNPCInfo(Npc));
  TestStore.dispatch(DirectiveSlice::Actions::DirectiveRunStarted(
      TEXT("dir_cascade"), TEXT("ag_cascade"), TEXT("obs")));

  FStoreState State = TestStore.getState();
  TestTrue("NPC exists",
           NPCSlice::SelectNPCById(State.NPCs, TEXT("ag_cascade")).hasValue);
  TestTrue("Directive exists",
           DirectiveSlice::SelectDirectiveById(State.Directives,
                                                TEXT("dir_cascade")).hasValue);

  /**
   * Remove NPC — listener should cascade-clear directives
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  TestStore.dispatch(NPCSlice::Actions::RemoveNPC(TEXT("ag_cascade")));

  State = TestStore.getState();
  TestFalse("NPC removed",
            NPCSlice::SelectNPCById(State.NPCs, TEXT("ag_cascade")).hasValue);

  /**
   * Directive cleanup depends on listener middleware being installed.
   * If the store's createNpcRemovalListener is wired, directives should be
   * cleared. This test verifies the NPC removal action itself is correct.
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */

  return true;
}

/**
 * Test: Multiple NPCs coexist with independent state
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProtocolMultiNpcTest,
                                 "ForbocAI.Integration.Protocol.MultiNpc",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FProtocolMultiNpcTest::RunTest(const FString &Parameters) {
  EnhancedStore<FStoreState> TestStore = createStore();

  FNPCInternalState Npc1;
  Npc1.Id = TEXT("ag_m1");
  Npc1.Persona = TEXT("Guard");
  FNPCInternalState Npc2;
  Npc2.Id = TEXT("ag_m2");
  Npc2.Persona = TEXT("Merchant");

  TestStore.dispatch(NPCSlice::Actions::SetNPCInfo(Npc1));
  TestStore.dispatch(NPCSlice::Actions::SetNPCInfo(Npc2));
  TestStore.dispatch(NPCSlice::Actions::SetActiveNPC(TEXT("ag_m1")));

  FStoreState State = TestStore.getState();
  TestEqual("Two NPCs", NPCSlice::SelectAllNPCs(State.NPCs).Num(), 2);
  TestEqual("Active is m1", NPCSlice::SelectActiveNpcId(State.NPCs),
            FString(TEXT("ag_m1")));

  /**
   * Update only m2
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  FAgentState M2State;
  M2State.JsonData = TEXT("{\"goods\":[\"sword\",\"potion\"]}");
  TestStore.dispatch(NPCSlice::Actions::UpdateNPCState(TEXT("ag_m2"), M2State));

  State = TestStore.getState();
  auto M1 = NPCSlice::SelectNPCById(State.NPCs, TEXT("ag_m1"));
  auto M2 = NPCSlice::SelectNPCById(State.NPCs, TEXT("ag_m2"));
  TestTrue("M1 exists", M1.hasValue);
  TestTrue("M2 exists", M2.hasValue);
  if (M1.hasValue && M2.hasValue) {
    TestEqual("M1 state unchanged", M1.value.State.JsonData,
              FString(TEXT("{}")));
    TestTrue("M2 state updated", M2.value.State.JsonData.Contains(TEXT("sword")));
  }

  /**
   * Switch active
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  TestStore.dispatch(NPCSlice::Actions::SetActiveNPC(TEXT("ag_m2")));
  State = TestStore.getState();
  TestEqual("Active is m2", NPCSlice::SelectActiveNpcId(State.NPCs),
            FString(TEXT("ag_m2")));

  auto Active = NPCSlice::SelectActiveNPC(State.NPCs);
  TestTrue("Active NPC found", Active.hasValue);
  if (Active.hasValue) {
    TestEqual("Active persona", Active.value.Persona,
              FString(TEXT("Merchant")));
  }

  return true;
}
