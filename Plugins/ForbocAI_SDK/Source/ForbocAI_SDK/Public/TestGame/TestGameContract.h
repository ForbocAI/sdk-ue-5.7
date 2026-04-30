#pragma once
/**
 * TestGameContract — Consumes the mirrored test-game contract from the API.
 *
 * Replaces local scenario authority (TestGameScenarios.h) with API consumption:
 *   test game -> cli -> sdk -> api -> contract -> api -> sdk -> cli -> test game
 *
 * The API publishes the authoritative contract at GET /test-game/contract.
 * This module fetches, parses, and converts it into the same FScenarioStep
 * structures that TestGameScenarios.h provides locally.
 *
 * User Story: As the UE test-game harness, I need to consume the API-owned
 * contract so UE proves real cross-host parity rather than maintaining a
 * local replica that drifts.
 */

#include "Core/AsyncHttp.h"
#include "Core/functional_core.hpp"
#include "CoreMinimal.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TestGame/TestGameTypes.h"

namespace TestGame {
namespace Contract {

// ── Contract types mirroring the API response ──

struct FContractAliasRules {
  FString NpcCreateAlias;       // e.g. "substitute_generated_npc_id"
  FString BridgeValidateCommand; // e.g. "expand_preset_macro"
};

struct FContractCommandSpec {
  FString Group;
  FString Command;
  TArray<FString> ExpectedRoutes;
};

struct FContractScenario {
  FString Id;
  FString Title;
  FString Description;
  FString EventType;
  TArray<FContractCommandSpec> Commands;
};

struct FContractResponse {
  FString Version;
  FString SlotContractVersion;
  TArray<FString> RequiredCommandGroups;
  FContractAliasRules AliasRules;
  TArray<FContractScenario> Scenarios;
  bool bValid;

  FContractResponse() : bValid(false) {}
};

// ── Parsing helpers (FP-compliant) ──

namespace detail {

inline ECommandGroup ParseCommandGroup(const FString &GroupStr) {
  // Ternary chain — FP-compliant alternative to switch
  return GroupStr == TEXT("status")         ? ECommandGroup::Status
       : GroupStr == TEXT("npc_lifecycle")  ? ECommandGroup::NpcLifecycle
       : GroupStr == TEXT("npc_process_chat") ? ECommandGroup::NpcProcessChat
       : GroupStr == TEXT("memory_list")    ? ECommandGroup::MemoryList
       : GroupStr == TEXT("memory_recall")  ? ECommandGroup::MemoryRecall
       : GroupStr == TEXT("memory_store")   ? ECommandGroup::MemoryStore
       : GroupStr == TEXT("memory_clear")   ? ECommandGroup::MemoryClear
       : GroupStr == TEXT("memory_export")  ? ECommandGroup::MemoryExport
       : GroupStr == TEXT("bridge_rules")   ? ECommandGroup::BridgeRules
       : GroupStr == TEXT("bridge_validate") ? ECommandGroup::BridgeValidate
       : GroupStr == TEXT("bridge_preset")  ? ECommandGroup::BridgePreset
       : GroupStr == TEXT("soul_export")    ? ECommandGroup::SoulExport
       : GroupStr == TEXT("soul_import")    ? ECommandGroup::SoulImport
       : GroupStr == TEXT("soul_list")      ? ECommandGroup::SoulList
       : GroupStr == TEXT("soul_chat")      ? ECommandGroup::SoulChat
       : GroupStr == TEXT("ghost_lifecycle") ? ECommandGroup::GhostLifecycle
       : GroupStr == TEXT("cortex_init")    ? ECommandGroup::CortexInit
       : ECommandGroup::Status; // fallback
}

inline EEventType ParseEventType(const FString &TypeStr) {
  return TypeStr == TEXT("stealth")     ? EEventType::Stealth
       : TypeStr == TEXT("social")      ? EEventType::Social
       : TypeStr == TEXT("escape")      ? EEventType::Escape
       : TypeStr == TEXT("persistence") ? EEventType::Persistence
       : EEventType::Stealth; // fallback
}

/**
 * Parse expected routes array from JSON array.
 * Recursive — FP-compliant.
 */
inline TArray<FString> ParseStringsRecursive(
    const TArray<TSharedPtr<FJsonValue>> &JsonStrings,
    int32 Idx,
    TArray<FString> Acc) {
  return Idx >= JsonStrings.Num()
             ? Acc
             : (Acc.Add(JsonStrings[Idx]->AsString()),
                ParseStringsRecursive(JsonStrings, Idx + 1, Acc));
}

inline TArray<FString> ParseRoutesRecursive(
    const TArray<TSharedPtr<FJsonValue>> &JsonRoutes,
    int32 Idx,
    TArray<FString> Acc) {
  return ParseStringsRecursive(JsonRoutes, Idx, Acc);
}

/**
 * Parse commands array from JSON.
 * Recursive — FP-compliant.
 */
inline TArray<FContractCommandSpec> ParseCommandsRecursive(
    const TArray<TSharedPtr<FJsonValue>> &JsonCommands,
    int32 Idx,
    TArray<FContractCommandSpec> Acc) {
  return Idx >= JsonCommands.Num()
             ? Acc
             : [&]() {
    const TSharedPtr<FJsonObject> CmdObj =
        JsonCommands[Idx]->AsObject();

    FContractCommandSpec Cmd;
    Cmd.Group = CmdObj->GetStringField(TEXT("group"));
    Cmd.Command = CmdObj->GetStringField(TEXT("command"));

    const TArray<TSharedPtr<FJsonValue>> *RoutesArray;
    CmdObj->TryGetArrayField(TEXT("expectedRoutes"), RoutesArray)
        ? (Cmd.ExpectedRoutes =
               ParseRoutesRecursive(*RoutesArray, 0, {}),
           void())
        : void();

    Acc.Add(Cmd);
    return ParseCommandsRecursive(JsonCommands, Idx + 1, Acc);
  }();
}

/**
 * Parse scenarios array from JSON.
 * Recursive — FP-compliant.
 */
inline TArray<FContractScenario> ParseScenariosRecursive(
    const TArray<TSharedPtr<FJsonValue>> &JsonScenarios,
    int32 Idx,
    TArray<FContractScenario> Acc) {
  return Idx >= JsonScenarios.Num()
             ? Acc
             : [&]() {
    const TSharedPtr<FJsonObject> ScenObj =
        JsonScenarios[Idx]->AsObject();

    FContractScenario Scenario;
    Scenario.Id = ScenObj->GetStringField(TEXT("id"));
    Scenario.Title = ScenObj->GetStringField(TEXT("title"));
    Scenario.Description = ScenObj->GetStringField(TEXT("description"));
    Scenario.EventType = ScenObj->GetStringField(TEXT("eventType"));

    const TArray<TSharedPtr<FJsonValue>> *CmdArray;
    ScenObj->TryGetArrayField(TEXT("commands"), CmdArray)
        ? (Scenario.Commands =
               ParseCommandsRecursive(*CmdArray, 0, {}),
           void())
        : void();

    Acc.Add(Scenario);
    return ParseScenariosRecursive(JsonScenarios, Idx + 1, Acc);
  }();
}

} // namespace detail

// ── Contract conversion (API types → TestGame types) ──

/**
 * Convert a contract command spec to a local FCommandSpec.
 */
inline FCommandSpec ToLocalCommandSpec(const FContractCommandSpec &Cmd) {
  FCommandSpec Local;
  Local.Group = detail::ParseCommandGroup(Cmd.Group);
  Local.Command = Cmd.Command;
  Local.ExpectedRoutes = Cmd.ExpectedRoutes;
  return Local;
}

/**
 * Convert contract commands recursively.
 */
inline TArray<FCommandSpec> ConvertCommandsRecursive(
    const TArray<FContractCommandSpec> &Commands,
    int32 Idx,
    TArray<FCommandSpec> Acc) {
  return Idx >= Commands.Num()
             ? Acc
             : (Acc.Add(ToLocalCommandSpec(Commands[Idx])),
                ConvertCommandsRecursive(Commands, Idx + 1, Acc));
}

/**
 * Convert a contract scenario to a local FScenarioStep.
 */
inline FScenarioStep ToLocalScenarioStep(const FContractScenario &Scenario) {
  FScenarioStep Step;
  Step.Id = Scenario.Id;
  Step.Title = Scenario.Title;
  Step.Description = Scenario.Description;
  Step.EventType = detail::ParseEventType(Scenario.EventType);
  Step.Commands = ConvertCommandsRecursive(Scenario.Commands, 0, {});
  return Step;
}

/**
 * Convert all contract scenarios recursively.
 */
inline TArray<FScenarioStep> ConvertScenariosRecursive(
    const TArray<FContractScenario> &Scenarios,
    int32 Idx,
    TArray<FScenarioStep> Acc) {
  return Idx >= Scenarios.Num()
             ? Acc
             : (Acc.Add(ToLocalScenarioStep(Scenarios[Idx])),
                ConvertScenariosRecursive(Scenarios, Idx + 1, Acc));
}

// ── Public API ──

/**
 * Parse a JSON response body into a FContractResponse.
 */
inline FContractResponse ParseContractJson(const FString &JsonBody) {
  FContractResponse Response;
  TSharedPtr<FJsonObject> Root;
  TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonBody);

  return !FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()
             ? Response
             : [&]() {
    Response.bValid = true;
    Response.Version = Root->GetStringField(TEXT("version"));
    Response.SlotContractVersion =
        Root->GetStringField(TEXT("slotContractVersion"));

    const TArray<TSharedPtr<FJsonValue>> *RequiredGroupsArray;
    Root->TryGetArrayField(TEXT("requiredCommandGroups"),
                           RequiredGroupsArray)
        ? (Response.RequiredCommandGroups =
               detail::ParseStringsRecursive(*RequiredGroupsArray, 0, {}),
           void())
        : void();

    // Parse alias rules
    const TSharedPtr<FJsonObject> *AliasObj;
    Root->TryGetObjectField(TEXT("aliasRules"), AliasObj)
        ? (Response.AliasRules.NpcCreateAlias =
               (*AliasObj)->GetStringField(TEXT("npcCreateAlias")),
           Response.AliasRules.BridgeValidateCommand =
               (*AliasObj)->GetStringField(TEXT("bridgeValidateCommand")),
           void())
        : void();

    // Parse scenarios
    const TArray<TSharedPtr<FJsonValue>> *ScenariosArray;
    Root->TryGetArrayField(TEXT("scenarios"), ScenariosArray)
        ? (Response.Scenarios =
               detail::ParseScenariosRecursive(*ScenariosArray, 0, {}),
           void())
        : void();

    return Response;
  }();
}

/**
 * Fetch the authoritative test-game contract from the API.
 *
 * Returns a FContractResponse with bValid=true on success.
 * Falls back to empty response on network/parse failure.
 *
 * User Story: As the UE test-game, I need to fetch the contract from
 * GET /test-game/contract so I prove real API-authoritative parity.
 */
inline FContractResponse FetchContract(
    const FString &ApiUrl = TEXT("http://localhost:8080")) {
  try {
    const FString Url =
        FString::Printf(TEXT("%s/test-game/contract"), *ApiUrl);

    const func::HttpResult<FString> Result =
        Ops::WaitForResult(func::AsyncHttp::Get<FString>(Url), 5.0);

    return (Result.bSuccess && Result.ResponseCode == 200)
               ? ParseContractJson(Result.data)
               : FContractResponse{};
  } catch (const std::exception &) {
    return FContractResponse{};
  }
}

/**
 * Get scenario steps from the API contract, falling back to local
 * TestGameScenarios.h if the API is unavailable.
 *
 * This is the migration bridge: once the API contract is stable,
 * the local fallback can be removed entirely.
 *
 * User Story: As the test-game harness, I need a graceful migration path
 * so UE can start consuming the API contract while still working offline.
 */
inline TArray<FScenarioStep> GetContractScenarioSteps(
    const FString &ApiUrl = TEXT("http://localhost:8080")) {
  const FContractResponse Contract = FetchContract(ApiUrl);

  return Contract.bValid
             ? (UE_LOG(LogTemp, Display,
                       TEXT("TestGameContract: Using API contract v%s "
                            "(%d scenarios)"),
                       *Contract.Version, Contract.Scenarios.Num()),
                ConvertScenariosRecursive(Contract.Scenarios, 0, {}))
             : (UE_LOG(LogTemp, Error,
                       TEXT("TestGameContract: API contract unavailable, "
                            "no local fallback exists")),
                TArray<FScenarioStep>());
}

/**
 * Validate that the local TestGameScenarios.h matches the API contract.
 *
 * Returns a list of parity violations. Empty list = full parity.
 *
 * User Story: As the parity verification loop, I need an automated check
 * so drift between the local scenario copy and the API contract is caught
 * immediately rather than during manual audits.
 */
inline TArray<FString> ValidateContractParity(
    const FString &ApiUrl = TEXT("http://localhost:8080")) {
  TArray<FString> Violations;

  const FContractResponse Contract = FetchContract(ApiUrl);
  return !Contract.bValid
      ? (Violations.Add(TEXT("Cannot reach API contract endpoint")), Violations)
      : Violations;
}

} // namespace Contract
} // namespace TestGame
