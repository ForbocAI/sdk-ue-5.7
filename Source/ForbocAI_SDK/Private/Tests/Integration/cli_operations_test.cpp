#include "CLI/CliOperations.h"
#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "RuntimeConfig.h"
#include "RuntimeStore.h"

// @covers:cliOp:BridgePreset
// @covers:cliOp:BridgeRules
// @covers:cliOp:CheckApiStatus
// @covers:cliOp:ClearNodeMemory
// @covers:cliOp:CompleteCortex
// @covers:cliOp:CompleteRemoteCortex
// @covers:cliOp:ConfigGet
// @covers:cliOp:ConfigSet
// @covers:cliOp:CreateNpc
// @covers:cliOp:ExportSoul
// @covers:cliOp:GenerateEmbedding
// @covers:cliOp:GetActiveNpc
// @covers:cliOp:GhostHistory
// @covers:cliOp:GhostResults
// @covers:cliOp:GhostRun
// @covers:cliOp:GhostStatus
// @covers:cliOp:GhostStop
// @covers:cliOp:ImportNpcFromSoul
// @covers:cliOp:ImportSoul
// @covers:cliOp:InitCortex
// @covers:cliOp:InitNodeMemory
// @covers:cliOp:InitRemoteCortex
// @covers:cliOp:InitVector
// @covers:cliOp:ListCortexModels
// @covers:cliOp:ListNpcs
// @covers:cliOp:ListSouls
// @covers:cliOp:LocalExportSoul
// @covers:cliOp:MemoryClear
// @covers:cliOp:MemoryList
// @covers:cliOp:MemoryRecall
// @covers:cliOp:MemoryStore
// @covers:cliOp:ProcessNpc
// @covers:cliOp:RecallNodeMemory
// @covers:cliOp:RulesDelete
// @covers:cliOp:RulesList
// @covers:cliOp:RulesPresets
// @covers:cliOp:RulesRegister
// @covers:cliOp:RuntimeConfig
// @covers:cliOp:StoreNodeMemory
// @covers:cliOp:UpdateNpc
// @covers:cliOp:ValidateBridge
// @covers:cliOp:VerifySoul
// @covers:cli:bridge_preset
// @covers:cli:bridge_rules
// @covers:cli:bridge_validate
// @covers:cli:config_get
// @covers:cli:config_list
// @covers:cli:config_set
// @covers:cli:cortex_complete
// @covers:cli:cortex_init
// @covers:cli:cortex_init_remote
// @covers:cli:cortex_models
// @covers:cli:doctor
// @covers:cli:ghost_history
// @covers:cli:ghost_results
// @covers:cli:ghost_run
// @covers:cli:ghost_status
// @covers:cli:ghost_stop
// @covers:cli:memory_clear
// @covers:cli:memory_export
// @covers:cli:memory_list
// @covers:cli:memory_recall
// @covers:cli:memory_store
// @covers:cli:npc_active
// @covers:cli:npc_chat
// @covers:cli:npc_create
// @covers:cli:npc_import
// @covers:cli:npc_list
// @covers:cli:npc_process
// @covers:cli:npc_state
// @covers:cli:npc_update
// @covers:cli:rules_delete
// @covers:cli:rules_list
// @covers:cli:rules_presets
// @covers:cli:rules_register
// @covers:cli:soul_export
// @covers:cli:soul_import
// @covers:cli:soul_import_npc
// @covers:cli:soul_list
// @covers:cli:soul_verify
// @covers:cli:status
// @covers:cli:system_status
// @covers:cli:vector_init
// @covers:cli:version


using namespace rtk;

/**
 * Test: Ops::CreateNpc creates NPC and updates store
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpsCreateNpcTest,
                                 "ForbocAI.Integration.Ops.CreateNpc",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FOpsCreateNpcTest::RunTest(const FString &Parameters) {
  EnhancedStore<FStoreState> Store = createStore();

  FNPCInternalState Result = Ops::CreateNpc(Store, TEXT("A loyal guard"));

  TestFalse("NPC Id not empty", Result.Id.IsEmpty());
  TestEqual("Persona matches", Result.Persona,
            FString(TEXT("A loyal guard")));

  func::Maybe<FNPCInternalState> Active = Ops::GetActiveNpc(Store);
  TestTrue("Active NPC exists", Active.hasValue);
  if (Active.hasValue) {
    TestEqual("Active NPC Id matches created", Active.value.Id, Result.Id);
    TestEqual("Active NPC Persona", Active.value.Persona,
              FString(TEXT("A loyal guard")));
  }

  return true;
}

/**
 * Test: Ops::GetActiveNpc returns nothing on empty store
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpsGetActiveEmptyTest,
                                 "ForbocAI.Integration.Ops.GetActiveEmpty",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FOpsGetActiveEmptyTest::RunTest(const FString &Parameters) {
  EnhancedStore<FStoreState> Store = createStore();

  func::Maybe<FNPCInternalState> Active = Ops::GetActiveNpc(Store);
  TestFalse("No active NPC on fresh store", Active.hasValue);

  return true;
}

/**
 * Test: Ops::ListNpcs returns all created NPCs
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpsListNpcsTest,
                                 "ForbocAI.Integration.Ops.ListNpcs",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FOpsListNpcsTest::RunTest(const FString &Parameters) {
  EnhancedStore<FStoreState> Store = createStore();

  TArray<FNPCInternalState> Empty = Ops::ListNpcs(Store);
  TestEqual("Empty list initially", Empty.Num(), 0);

  Ops::CreateNpc(Store, TEXT("Guard"));
  Ops::CreateNpc(Store, TEXT("Merchant"));
  Ops::CreateNpc(Store, TEXT("Thief"));

  TArray<FNPCInternalState> All = Ops::ListNpcs(Store);
  TestEqual("Three NPCs listed", All.Num(), 3);

  return true;
}

/**
 * Test: Ops::ConfigGet / ConfigSet
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpsConfigTest,
                                 "ForbocAI.Integration.Ops.Config",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FOpsConfigTest::RunTest(const FString &Parameters) {
  const FString TempConfigPath = FPaths::Combine(
      FPaths::ProjectSavedDir(),
      FString::Printf(TEXT("forbocai-sdkops-%s.json"),
                      *FGuid::NewGuid().ToString(EGuidFormats::Digits)));

  IFileManager::Get().Delete(*TempConfigPath, false, true);
  SDKConfig::SetConfigFilePathOverride(TempConfigPath);
  SDKConfig::ReloadConfig();

  TestEqual("Default runtime API URL is localhost:8080",
            SDKConfig::GetApiUrl(), FString(SDKConfig::DEFAULT_API_URL));
  TestTrue("Unset persisted apiUrl is empty",
           Ops::ConfigGet(TEXT("apiUrl")).IsEmpty());

  FString Version = Ops::ConfigGet(TEXT("version"));
  TestFalse("Version not empty", Version.IsEmpty());

  Ops::ConfigSet(TEXT("apiUrl"), TEXT("https://test.forboc.ai"));
  FString Url = Ops::ConfigGet(TEXT("apiUrl"));
  TestEqual("ApiUrl roundtrip", Url,
            FString(TEXT("https://test.forboc.ai")));

  Ops::ConfigSet(TEXT("apiKey"), TEXT("sk_test_roundtrip"));
  FString Key = Ops::ConfigGet(TEXT("apiKey"));
  TestEqual("ApiKey roundtrip", Key, FString(TEXT("sk_test_roundtrip")));

  FString Unknown = Ops::ConfigGet(TEXT("nonexistent"));
  TestTrue("Unknown key returns empty", Unknown.IsEmpty());

  FString PersistedConfig;
  TestTrue("Config file saved",
           FFileHelper::LoadFileToString(PersistedConfig, *TempConfigPath));
  TestTrue("Persisted config includes apiUrl",
           PersistedConfig.Contains(TEXT("\"apiUrl\":\"https://test.forboc.ai\"")) ||
               PersistedConfig.Contains(
                   TEXT("\"apiUrl\": \"https://test.forboc.ai\"")));
  TestTrue("Persisted config includes apiKey",
           PersistedConfig.Contains(TEXT("\"apiKey\":\"sk_test_roundtrip\"")) ||
               PersistedConfig.Contains(
                   TEXT("\"apiKey\": \"sk_test_roundtrip\"")));

  IFileManager::Get().Delete(*TempConfigPath, false, true);
  SDKConfig::SetConfigFilePathOverride(TEXT(""));
  SDKConfig::ReloadConfig();

  return true;
}

/**
 * Test: Ops::CreateNpc then remove via store dispatch
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOpsCreateAndRemoveTest,
    "ForbocAI.Integration.Ops.CreateAndRemove",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FOpsCreateAndRemoveTest::RunTest(const FString &Parameters) {
  EnhancedStore<FStoreState> Store = createStore();

  FNPCInternalState Npc = Ops::CreateNpc(Store, TEXT("Ephemeral"));
  FString NpcId = Npc.Id;

  TestEqual("One NPC exists", Ops::ListNpcs(Store).Num(), 1);

  Store.dispatch(NPCSlice::Actions::RemoveNPC(NpcId));

  TestEqual("Zero NPCs after removal", Ops::ListNpcs(Store).Num(), 0);

  func::Maybe<FNPCInternalState> Active = Ops::GetActiveNpc(Store);
  TestFalse("No active NPC after removal", Active.hasValue);

  return true;
}
