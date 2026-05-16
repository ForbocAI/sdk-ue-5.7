#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "RuntimeStore.h"

using namespace rtk;

/**
 * Test: StoreReducer processes NPC creation across all slices
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStoreNPCCreationTest,
                                 "ForbocAI.Integration.Store.NPCCreation",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FStoreNPCCreationTest::RunTest(const FString &Parameters) {
  FStoreState State;

  FNPCInternalState Info;
  Info.Id = TEXT("int_npc_1");
  Info.Persona = TEXT("Test NPC");

  State = StoreReducer(State, NPCSlice::Actions::SetNPCInfo(Info));

  TestEqual("NPC active", State.NPCs.ActiveNpcId,
            FString(TEXT("int_npc_1")));
  TestTrue("NPC in entities",
           NPCSlice::SelectNPCById(State.NPCs, TEXT("int_npc_1")).hasValue);

  /**
   * Other slices remain at initial state
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  TestEqual("Memory idle", State.Memory.StorageStatus,
            FString(TEXT("idle")));
  TestEqual("Ghost idle", State.Ghost.Status, FString(TEXT("idle")));
  TestEqual("Bridge idle", State.Bridge.Status, FString(TEXT("idle")));
  TestEqual("Soul export idle", State.Soul.ExportStatus,
            FString(TEXT("idle")));
  TestTrue("Cortex idle",
           State.Cortex.Status == CortexSlice::ECortexEngineStatus::Idle);

  return true;
}

/**
 * Test: NPC removal middleware cascades clears
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FStoreRemovalCascadeTest,
    "ForbocAI.Integration.Store.RemovalCascade",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FStoreRemovalCascadeTest::RunTest(const FString &Parameters) {
  EnhancedStore<FStoreState> Store = createStore();

  /**
   * Create NPC
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  FNPCInternalState Info;
  Info.Id = TEXT("cascade_npc");
  Info.Persona = TEXT("Cascade test");
  Store.dispatch(NPCSlice::Actions::SetNPCInfo(Info));

  TestEqual("NPC active before removal", Store.getState().NPCs.ActiveNpcId,
            FString(TEXT("cascade_npc")));

  /**
   * Add some memory
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  FMemoryItem MemItem;
  MemItem.Id = TEXT("cascade_mem");
  MemItem.Text = TEXT("Important memory");
  MemItem.Importance = 0.9f;
  Store.dispatch(MemorySlice::Actions::MemoryStoreSuccess(MemItem));
  TestEqual("Memory exists", MemorySlice::SelectAllMemories(
                                  Store.getState().Memory)
                                  .Num(),
            1);

  /**
   * Add bridge validation
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  Store.dispatch(BridgeSlice::Actions::BridgeValidationPending());
  TestEqual("Bridge validating", Store.getState().Bridge.Status,
            FString(TEXT("validating")));

  FDirectiveRuleSet Preset;
  Preset.Id = TEXT("preset_keep");
  Store.dispatch(BridgeSlice::Actions::AddActivePreset(Preset));

  TArray<FDirectiveRuleSet> Rulesets;
  FDirectiveRuleSet Ruleset;
  Ruleset.Id = TEXT("ruleset_keep");
  Ruleset.RulesetId = TEXT("ruleset_keep");
  Rulesets.Add(Ruleset);
  Store.dispatch(BridgeSlice::Actions::SetAvailableRulesets(Rulesets));

  TArray<FString> PresetIds;
  PresetIds.Add(TEXT("preset_keep"));
  Store.dispatch(BridgeSlice::Actions::SetAvailablePresetIds(PresetIds));

  /**
   * Start ghost session
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  Store.dispatch(
      GhostSlice::Actions::GhostSessionStarted(TEXT("gs_1"), TEXT("running")));
  TestEqual("Ghost running", Store.getState().Ghost.Status,
            FString(TEXT("running")));

  /**
   * Add a directive
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  Store.dispatch(DirectiveSlice::Actions::DirectiveRunStarted(
      TEXT("dir_1"), TEXT("cascade_npc"), TEXT("observe")));

  /**
   * Remove NPC — should cascade clear
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  Store.dispatch(NPCSlice::Actions::RemoveNPC(TEXT("cascade_npc")));

  TestTrue("NPC removed",
           !NPCSlice::SelectNPCById(Store.getState().NPCs, TEXT("cascade_npc"))
                .hasValue);
  TestTrue("ActiveNpcId cleared",
           Store.getState().NPCs.ActiveNpcId.IsEmpty());

  /**
   * Cascade effects
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  TestEqual("Memory cleared after active NPC removal",
            MemorySlice::SelectAllMemories(Store.getState().Memory).Num(), 0);
  TestEqual("Bridge reset", Store.getState().Bridge.Status,
            FString(TEXT("idle")));
  TestEqual("Bridge active presets preserved",
            Store.getState().Bridge.ActivePresets.Num(), 1);
  TestEqual("Bridge rulesets preserved",
            Store.getState().Bridge.AvailableRulesets.Num(), 1);
  TestEqual("Bridge preset ids preserved",
            Store.getState().Bridge.AvailablePresetIds.Num(), 1);
  TestEqual("Ghost reset", Store.getState().Ghost.Status,
            FString(TEXT("idle")));
  TestTrue("Directive cleared",
           Store.getState().Directives.ActiveDirectiveId.IsEmpty());
  TestEqual("Soul reset", Store.getState().Soul.ExportStatus,
            FString(TEXT("idle")));

  return true;
}

/**
 * Test: Multiple NPCs, remove non-active — no cascade on memory
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FStoreRemoveNonActiveTest,
    "ForbocAI.Integration.Store.RemoveNonActive",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FStoreRemoveNonActiveTest::RunTest(const FString &Parameters) {
  EnhancedStore<FStoreState> Store = createStore();

  /**
   * Create two NPCs
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  FNPCInternalState A;
  A.Id = TEXT("npc_a");
  A.Persona = TEXT("Alpha");
  Store.dispatch(NPCSlice::Actions::SetNPCInfo(A));

  FNPCInternalState B;
  B.Id = TEXT("npc_b");
  B.Persona = TEXT("Beta");
  Store.dispatch(NPCSlice::Actions::SetNPCInfo(B));

  /**
   * Active is now B
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  TestEqual("Active is B", Store.getState().NPCs.ActiveNpcId,
            FString(TEXT("npc_b")));

  /**
   * Store memory
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  FMemoryItem Mem;
  Mem.Id = TEXT("keep_mem");
  Mem.Text = TEXT("Should survive");
  Mem.Importance = 0.5f;
  Store.dispatch(MemorySlice::Actions::MemoryStoreSuccess(Mem));

  /**
   * Remove A (not active) — memory should NOT be cleared
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  Store.dispatch(NPCSlice::Actions::RemoveNPC(TEXT("npc_a")));

  TestFalse("A removed",
            NPCSlice::SelectNPCById(Store.getState().NPCs, TEXT("npc_a"))
                .hasValue);
  TestEqual("Active still B", Store.getState().NPCs.ActiveNpcId,
            FString(TEXT("npc_b")));
  TestEqual("Memory preserved",
            MemorySlice::SelectAllMemories(Store.getState().Memory).Num(), 1);

  return true;
}

/**
 * Test: Logger helpers summarize action payloads and slice deltas
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FStoreReduxLoggerSummaryTest,
    "ForbocAI.Integration.Store.ReduxLoggerSummary",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FStoreReduxLoggerSummaryTest::RunTest(const FString &Parameters) {
  (void)Parameters;

  const rtk::AnyAction SetActiveAction =
      NPCSlice::Actions::SetActiveNPC(TEXT("logger_npc"));
  TestEqual("String payloads are preserved for logger output",
            SetActiveAction.describePayload(), FString(TEXT("logger_npc")));

  FStoreState Before;
  FStoreState After = StoreReducer(Before, SetActiveAction);
  const FString Delta = StoreInternal::DescribeStateDelta(Before, After);

  TestTrue("Delta includes NPC slice summary", Delta.Contains(TEXT("NPCs{")));
  TestTrue("Delta includes active npc id",
           Delta.Contains(TEXT("logger_npc")));
  TestEqual("No-change deltas collapse to <none>",
            StoreInternal::DescribeStateDelta(After, After),
            FString(TEXT("<none>")));

  return true;
}
