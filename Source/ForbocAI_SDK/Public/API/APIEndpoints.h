#pragma once

#include "APICodecs.h"

namespace APISlice {

namespace Endpoints {

// C++11 type alias — all endpoint factories return ThunkAction<Result, FStoreState>.
template <typename T>
using Thunk = rtk::ThunkAction<T, FStoreState>;

/**
 * Builds the endpoint thunk that fetches the NPC collection.
 * User Story: As NPC listing flows, I need a reusable endpoint thunk so the
 * SDK can load all NPCs without duplicating request wiring.
 */
inline Thunk<TArray<FAgent>> getNPCs() {
  TArray<FApiEndpointTag> Tags;
  Tags.Add(FApiEndpointTag{TEXT("NPC"), TEXT("LIST")});
  return Detail::MakeGet<TArray<FAgent>>(
      TEXT("getNPCs"), SDKConfig::GetApiUrl() + TEXT("/npcs"), Tags);
}

/**
 * Builds the endpoint thunk that fetches a single NPC by id.
 * User Story: As NPC detail flows, I need a reusable endpoint thunk so the SDK
 * can load one NPC record by identifier.
 */
inline Thunk<FAgent> getNPC(const FString &NpcId) {
  TArray<FApiEndpointTag> Tags;
  Tags.Add(FApiEndpointTag{TEXT("NPC"), NpcId});
  return Detail::MakeGet<FAgent>(TEXT("getNPC"), SDKConfig::GetApiUrl() +
                                                     TEXT("/npcs/") +
                                                     Detail::Encode(NpcId),
                                 Tags);
}

/**
 * Builds the endpoint thunk that creates or upserts an NPC.
 * User Story: As NPC authoring flows, I need a reusable endpoint thunk so NPC
 * definitions can be created or updated through one helper.
 */
inline Thunk<FAgent> postNPC(const FAgentConfig &Config) {
  TArray<FApiEndpointTag> Invalidates;
  Invalidates.Add(FApiEndpointTag{TEXT("NPC"), TEXT("LIST")});
  return (!Config.Id.IsEmpty()
              ? (Invalidates.Add(FApiEndpointTag{TEXT("NPC"), Config.Id}), void())
              : void(),
          Detail::MakePost<FAgentConfig, FAgent>(
      TEXT("postNPC"), SDKConfig::GetApiUrl() + TEXT("/npcs"), Config,
      Invalidates));
}

/**
 * Builds the endpoint thunk that checks remote API health.
 * User Story: As connectivity checks, I need a reusable endpoint thunk so the
 * runtime can confirm the remote API is reachable before deeper calls.
 */
inline Thunk<FApiStatusResponse> getApiStatus() {
  return Detail::MakeGet<FApiStatusResponse>(
      TEXT("getApiStatus"), SDKConfig::GetApiUrl() + TEXT("/status"));
}

/**
 * Builds the endpoint thunk that lists remote cortex models.
 * User Story: As model selection flows, I need a reusable endpoint thunk so
 * the SDK can enumerate available remote cortex models.
 */
inline Thunk<TArray<FCortexModelInfo>> getCortexModels() {
  return Detail::MakeGet<TArray<FCortexModelInfo>>(
      TEXT("getCortexModels"), SDKConfig::GetApiUrl() + TEXT("/cortex/models"));
}

/**
 * Builds the endpoint thunk that initializes a remote cortex session.
 * User Story: As cortex session setup, I need a reusable endpoint thunk so the
 * runtime can start a remote inference session predictably.
 */
inline Thunk<FCortexInitResponse> postCortexInit(const FCortexInitRequest &Request) {
  return Detail::MakePost<FCortexInitRequest, FCortexInitResponse>(
      TEXT("postCortexInit"), SDKConfig::GetApiUrl() + TEXT("/cortex/init"),
      Request);
}

/**
 * Builds the endpoint thunk that runs remote completion for a cortex session.
 * User Story: As remote inference flows, I need a reusable endpoint thunk so a
 * configured cortex session can produce a completion from one helper.
 */
inline Thunk<FCortexResponse> postCortexComplete(const FString &CortexId,
                               const FCortexCompleteRequest &Request) {
  return Detail::MakeEndpoint<FCortexCompleteRequest, FCortexResponse>(
      TEXT("postCortexComplete"), Request,
      [CortexId](const FCortexCompleteRequest &Arg) {
        return func::AsyncHttp::Post<FCortexResponse>(
            SDKConfig::GetApiUrl() + TEXT("/cortex/") +
                Detail::Encode(CortexId) + TEXT("/complete"),
            Detail::BuildCortexCompletePayload(Arg), SDKConfig::GetApiKey());
      });
}

/**
 * Builds the endpoint thunk that executes the single-hop NPC process route.
 * User Story: As NPC processing flows, I need a reusable endpoint thunk so the
 * SDK can run one process turn against a remote NPC.
 */
inline Thunk<FNPCProcessResponse> postNpcProcess(const FString &NpcId,
                           const FNPCProcessRequest &Request) {
  return Detail::MakePostWithCodec<FNPCProcessRequest, FNPCProcessResponse>(
      TEXT("postNpcProcess"),
      SDKConfig::GetApiUrl() + TEXT("/npcs/") + Detail::Encode(NpcId) +
          TEXT("/process"),
      Request, Detail::EncodeNpcProcessRequest,
      Detail::DecodeNpcProcessResponse);
}

/**
 * Builds the endpoint thunk that requests directive composition for an NPC.
 * User Story: As directive generation flows, I need a reusable endpoint thunk
 * so the runtime can request directives for a specific NPC.
 */
inline Thunk<FDirectiveResponse> postDirective(const FString &NpcId,
                          const FDirectiveRequest &Request) {
  return Detail::MakePostWithCodec<FDirectiveRequest, FDirectiveResponse>(
      TEXT("postDirective"),
      SDKConfig::GetApiUrl() + TEXT("/npcs/") + Detail::Encode(NpcId) +
          TEXT("/directive"),
      Request, Detail::EncodeDirectiveRequest, Detail::DecodeDirectiveResponse);
}

/**
 * Builds the endpoint thunk that requests context composition for an NPC.
 * User Story: As context assembly flows, I need a reusable endpoint thunk so
 * the runtime can request composed context for a specific NPC.
 */
inline Thunk<FContextResponse> postContext(const FString &NpcId, const FContextRequest &Request) {
  return Detail::MakePostWithCodec<FContextRequest, FContextResponse>(
      TEXT("postContext"),
      SDKConfig::GetApiUrl() + TEXT("/npcs/") + Detail::Encode(NpcId) +
          TEXT("/context"),
      Request, Detail::EncodeContextRequest, Detail::DecodeContextResponse);
}

/**
 * Builds the endpoint thunk that validates a verdict for an NPC turn.
 * User Story: As verdict evaluation flows, I need a reusable endpoint thunk so
 * the runtime can validate turn outcomes for a specific NPC.
 */
inline Thunk<FVerdictResponse> postVerdict(const FString &NpcId, const FVerdictRequest &Request) {
  return Detail::MakePostWithCodec<FVerdictRequest, FVerdictResponse>(
      TEXT("postVerdict"),
      SDKConfig::GetApiUrl() + TEXT("/npcs/") + Detail::Encode(NpcId) +
          TEXT("/verdict"),
      Request, Detail::EncodeVerdictRequest, Detail::DecodeVerdictResponse);
}

/**
 * Builds the endpoint thunk that stores remote memory for an NPC.
 * User Story: As remote memory persistence flows, I need a reusable endpoint
 * thunk so memory items can be stored for a specific NPC.
 */
inline Thunk<rtk::FEmptyPayload> postMemoryStore(const FString &NpcId,
                            const FRemoteMemoryStoreRequest &Request) {
  TArray<FApiEndpointTag> Invalidates;
  Invalidates.Add(FApiEndpointTag{TEXT("Memory"), NpcId});
  return Detail::MakePost<FRemoteMemoryStoreRequest, rtk::FEmptyPayload>(
      TEXT("postMemoryStore"),
      SDKConfig::GetApiUrl() + TEXT("/npcs/") + Detail::Encode(NpcId) +
          TEXT("/memory"),
      Request, Invalidates);
}

/**
 * Builds the endpoint thunk that lists remote memories for an NPC.
 * User Story: As remote memory browsing flows, I need a reusable endpoint
 * thunk so stored memories can be listed for a specific NPC.
 */
inline Thunk<TArray<FMemoryItem>> getMemoryList(const FString &NpcId) {
  TArray<FApiEndpointTag> Tags;
  Tags.Add(FApiEndpointTag{TEXT("Memory"), NpcId});
  return Detail::MakeGet<TArray<FMemoryItem>>(
      TEXT("getMemoryList"), SDKConfig::GetApiUrl() + TEXT("/npcs/") +
                                 Detail::Encode(NpcId) + TEXT("/memory"),
      Tags);
}

/**
 * Builds the endpoint thunk that recalls remote memories for an NPC.
 * User Story: As remote recall flows, I need a reusable endpoint thunk so the
 * runtime can fetch relevant memories for a specific NPC.
 */
inline Thunk<TArray<FMemoryItem>> postMemoryRecall(const FString &NpcId,
                             const FRemoteMemoryRecallRequest &Request) {
  return Detail::MakePost<FRemoteMemoryRecallRequest, TArray<FMemoryItem>>(
      TEXT("postMemoryRecall"),
      SDKConfig::GetApiUrl() + TEXT("/npcs/") + Detail::Encode(NpcId) +
          TEXT("/memory/recall"),
      Request);
}

/**
 * Builds the endpoint thunk that clears remote memories for an NPC.
 * User Story: As memory reset flows, I need a reusable endpoint thunk so the
 * runtime can clear stored remote memories for a specific NPC.
 */
inline Thunk<rtk::FEmptyPayload> deleteMemoryClear(const FString &NpcId) {
  TArray<FApiEndpointTag> Invalidates;
  Invalidates.Add(FApiEndpointTag{TEXT("Memory"), NpcId});
  return Detail::MakeDelete<rtk::FEmptyPayload>(
      TEXT("deleteMemoryClear"),
      SDKConfig::GetApiUrl() + TEXT("/npcs/") + Detail::Encode(NpcId) +
          TEXT("/memory/clear"),
      Invalidates);
}

/**
 * Builds the endpoint thunk that validates a bridge payload from raw JSON.
 * User Story: As raw bridge validation flows, I need a reusable endpoint thunk
 * so JSON payloads can be validated without building typed request objects.
 */
inline Thunk<FValidationResult> postBridgeValidate(const FString &NpcId,
                               const FString &PayloadJson) {
  const FString Url = NpcId.IsEmpty()
                          ? SDKConfig::GetApiUrl() + TEXT("/bridge/validate")
                          : SDKConfig::GetApiUrl() + TEXT("/bridge/validate/") +
                                Detail::Encode(NpcId);
  return Detail::MakePostRawWithCodec<FValidationResult>(
      TEXT("postBridgeValidate"), Url, PayloadJson,
      Detail::DecodeValidationResult);
}

/**
 * Builds the endpoint thunk that validates a typed bridge request.
 * User Story: As typed bridge validation flows, I need a reusable endpoint
 * thunk so structured validation requests can be submitted consistently.
 */
inline Thunk<FValidationResult> postBridgeValidate(const FString &NpcId,
                               const FBridgeValidateRequest &Request) {
  const FString Url = NpcId.IsEmpty()
                          ? SDKConfig::GetApiUrl() + TEXT("/bridge/validate")
                          : SDKConfig::GetApiUrl() + TEXT("/bridge/validate/") +
                                Detail::Encode(NpcId);
  return Detail::MakePostWithCodec<FBridgeValidateRequest, FValidationResult>(
      TEXT("postBridgeValidate"), Url, Request,
      Detail::EncodeBridgeValidateRequest, Detail::DecodeValidationResult);
}

/**
 * Builds the endpoint thunk that fetches bridge validation rules.
 * User Story: As bridge rule discovery, I need a reusable endpoint thunk so
 * the runtime can load the current validation rules from the API.
 */
inline Thunk<TArray<FBridgeRule>> getBridgeRules() {
  return Detail::MakeGetWithCodec<TArray<FBridgeRule>>(
      TEXT("getBridgeRules"), SDKConfig::GetApiUrl() + TEXT("/bridge/rules"),
      Detail::DecodeBridgeRulesResponse);
}

/**
 * Builds the endpoint thunk that resolves a named bridge preset.
 * User Story: As bridge preset selection flows, I need a reusable endpoint
 * thunk so a named preset can be fetched on demand.
 */
inline Thunk<FDirectiveRuleSet> postBridgePreset(const FString &PresetName) {
  return Detail::MakePostRawWithCodec<FDirectiveRuleSet>(
      TEXT("postBridgePreset"),
      SDKConfig::GetApiUrl() + TEXT("/rules/presets/") +
          Detail::Encode(PresetName),
      TEXT("{}"), Detail::DecodeDirectiveRuleSetResponse);
}

/**
 * Builds the endpoint thunk that lists registered rulesets.
 * User Story: As rules catalog flows, I need a reusable endpoint thunk so the
 * runtime can enumerate registered directive rulesets.
 */
inline Thunk<TArray<FDirectiveRuleSet>> getRulesets() {
  return Detail::MakeGetWithCodec<TArray<FDirectiveRuleSet>>(
      TEXT("getRulesets"), SDKConfig::GetApiUrl() + TEXT("/rules"),
      Detail::DecodeDirectiveRuleSetListResponse);
}

/**
 * Builds the endpoint thunk that lists available rule preset ids.
 * User Story: As preset discovery flows, I need a reusable endpoint thunk so
 * the runtime can enumerate available rule preset identifiers.
 */
inline Thunk<TArray<FString>> getRulePresets() {
  return Detail::MakeGet<TArray<FString>>(
      TEXT("getRulePresets"), SDKConfig::GetApiUrl() + TEXT("/rules/presets"));
}

/**
 * Builds the endpoint thunk that registers or updates a ruleset.
 * User Story: As rules authoring flows, I need a reusable endpoint thunk so a
 * ruleset can be created or updated through one helper.
 */
inline Thunk<FDirectiveRuleSet> postRuleRegister(const FDirectiveRuleSet &Ruleset) {
  TArray<FApiEndpointTag> Invalidates;
  Invalidates.Add(FApiEndpointTag{TEXT("Rule"), TEXT("LIST")});
  return Detail::MakePostWithCodec<FDirectiveRuleSet, FDirectiveRuleSet>(
      TEXT("postRuleRegister"), SDKConfig::GetApiUrl() + TEXT("/rules"),
      Ruleset, Detail::ToJson<FDirectiveRuleSet>,
      Detail::DecodeDirectiveRuleSetResponse, Invalidates);
}

/**
 * Builds the endpoint thunk that deletes a ruleset by id.
 * User Story: As rules cleanup flows, I need a reusable endpoint thunk so an
 * obsolete ruleset can be removed by identifier.
 */
inline Thunk<rtk::FEmptyPayload> deleteRule(const FString &RulesetId) {
  TArray<FApiEndpointTag> Invalidates;
  Invalidates.Add(FApiEndpointTag{TEXT("Rule"), TEXT("LIST")});
  return Detail::MakeDelete<rtk::FEmptyPayload>(
      TEXT("deleteRule"),
      SDKConfig::GetApiUrl() + TEXT("/rules/") + Detail::Encode(RulesetId),
      Invalidates);
}

/**
 * Builds the endpoint thunk that starts a ghost run from a request payload.
 * User Story: As ghost test execution flows, I need a reusable endpoint thunk
 * so a fully specified ghost run can be started remotely.
 */
inline Thunk<FGhostRunResponse> postGhostRun(const FGhostRunRequest &Request) {
  return Detail::MakePostWithCodec<FGhostRunRequest, FGhostRunResponse>(
      TEXT("postGhostRun"), SDKConfig::GetApiUrl() + TEXT("/ghost/run"),
      Request, Detail::ToJson<FGhostRunRequest>,
      Detail::DecodeGhostRunResponse);
}

/**
 * Builds the endpoint thunk that starts a ghost run from config.
 * User Story: As ghost test setup flows, I need a convenience endpoint thunk
 * so config values can be turned into a run request automatically.
 */
inline Thunk<FGhostRunResponse> postGhostRun(const FGhostConfig &Config) {
  return postGhostRun(
      TypeFactory::GhostRunRequest(Config.TestSuite, Config.Duration));
}

/**
 * Builds the endpoint thunk that fetches ghost session status.
 * User Story: As ghost monitoring flows, I need a reusable endpoint thunk so
 * the runtime can poll the status of a ghost session.
 */
inline Thunk<FGhostStatusResponse> getGhostStatus(const FString &SessionId) {
  return Detail::MakeGetWithCodec<FGhostStatusResponse>(
      TEXT("getGhostStatus"),
      SDKConfig::GetApiUrl() + TEXT("/ghost/") + Detail::Encode(SessionId) +
          TEXT("/status"),
      Detail::DecodeGhostStatusResponse);
}

/**
 * Builds the endpoint thunk that fetches ghost session results.
 * User Story: As ghost result retrieval, I need a reusable endpoint thunk so
 * completed ghost session output can be fetched by session id.
 */
inline Thunk<FGhostResultsResponse> getGhostResults(const FString &SessionId) {
  return Detail::MakeGetWithCodec<FGhostResultsResponse>(
      TEXT("getGhostResults"),
      SDKConfig::GetApiUrl() + TEXT("/ghost/") + Detail::Encode(SessionId) +
          TEXT("/results"),
      Detail::DecodeGhostResultsResponse);
}

/**
 * Builds the endpoint thunk that stops a running ghost session.
 * User Story: As ghost cancellation flows, I need a reusable endpoint thunk so
 * an active ghost session can be stopped from the runtime.
 */
inline Thunk<FGhostStopResponse> postGhostStop(const FString &SessionId) {
  return Detail::MakePostRawWithCodec<FGhostStopResponse>(
      TEXT("postGhostStop"),
      SDKConfig::GetApiUrl() + TEXT("/ghost/") + Detail::Encode(SessionId) +
          TEXT("/stop"),
      TEXT("{}"), Detail::DecodeGhostStopResponse);
}

/**
 * Builds the endpoint thunk that lists recent ghost session history.
 * User Story: As ghost run review flows, I need a reusable endpoint thunk so
 * recent ghost sessions can be listed with a limit.
 */
inline Thunk<FGhostHistoryResponse> getGhostHistory(int32 Limit = 10) {
  return Detail::MakeGetWithCodec<FGhostHistoryResponse>(
      TEXT("getGhostHistory"),
      SDKConfig::GetApiUrl() + TEXT("/ghost/history?limit=") +
          FString::FromInt(Limit),
      Detail::DecodeGhostHistoryResponse);
}

/**
 * Builds the endpoint thunk that starts phase-one soul export for an NPC.
 * User Story: As soul export flows, I need a reusable endpoint thunk so export
 * metadata can be created for a specific NPC.
 */
inline Thunk<FSoulExportPhase1Response> postSoulExport(const FString &NpcId,
                           const FSoulExportPhase1Request &Request) {
  return Detail::MakePostWithCodec<FSoulExportPhase1Request,
                                   FSoulExportPhase1Response>(
      TEXT("postSoulExport"),
      SDKConfig::GetApiUrl() + TEXT("/npcs/") + Detail::Encode(NpcId) +
          TEXT("/soul/export"),
      Request, Detail::EncodeSoulExportPhase1Request,
      Detail::DecodeSoulExportPhase1Response);
}

/**
 * Builds the endpoint thunk that confirms soul export after upload.
 * User Story: As soul export completion flows, I need a reusable endpoint
 * thunk so uploaded export artifacts can be confirmed with the API.
 */
inline Thunk<FSoulExportResponse> postSoulExportConfirm(const FString &NpcId,
                                  const FSoulExportConfirmRequest &Request) {
  return Detail::MakePostWithCodec<FSoulExportConfirmRequest,
                                   FSoulExportResponse>(
      TEXT("postSoulExportConfirm"),
      SDKConfig::GetApiUrl() + TEXT("/npcs/") + Detail::Encode(NpcId) +
          TEXT("/soul/confirm"),
      Request, Detail::EncodeSoulExportConfirmRequest,
      Detail::DecodeSoulExportResponse);
}

/**
 * Builds the endpoint thunk that lists remotely stored souls.
 * User Story: As soul catalog flows, I need a reusable endpoint thunk so the
 * runtime can enumerate remotely stored souls.
 */
inline Thunk<FSoulListResponse> getSouls(int32 Limit = 50) {
  return Detail::MakeGet<FSoulListResponse>(
      TEXT("getSouls"),
      SDKConfig::GetApiUrl() + TEXT("/souls?limit=") + FString::FromInt(Limit));
}

/**
 * Builds the endpoint thunk that loads phase-one soul import metadata.
 * User Story: As soul import flows, I need a reusable endpoint thunk so the
 * runtime can load import metadata for a transaction id.
 */
inline Thunk<FSoulImportPhase1Response> getSoulImport(const FString &TxId) {
  return Detail::MakeGet<FSoulImportPhase1Response>(
      TEXT("getSoulImport"),
      SDKConfig::GetApiUrl() + TEXT("/souls/") + Detail::Encode(TxId));
}

/**
 * Builds the endpoint thunk that verifies a remote soul transaction.
 * User Story: As soul verification flows, I need a reusable endpoint thunk so
 * a remote soul transaction can be verified before import or trust decisions.
 */
inline Thunk<FSoulVerifyResult> postSoulVerify(const FString &TxId) {
  return Detail::MakePostRawWithCodec<FSoulVerifyResult>(
      TEXT("postSoulVerify"),
      SDKConfig::GetApiUrl() + TEXT("/souls/") + Detail::Encode(TxId) +
          TEXT("/verify"),
      TEXT("{}"), Detail::DecodeSoulVerifyResponse);
}

/**
 * Builds the endpoint thunk that starts NPC import from a soul.
 * User Story: As NPC import flows, I need a reusable endpoint thunk so soul
 * import metadata can be created before the final import confirmation step.
 */
inline Thunk<FSoulImportPhase1Response> postNpcImport(const FSoulImportPhase1Request &Request) {
  return Detail::MakePostWithCodec<FSoulImportPhase1Request,
                                   FSoulImportPhase1Response>(
      TEXT("postNpcImport"), SDKConfig::GetApiUrl() + TEXT("/npcs/import"),
      Request, Detail::EncodeSoulImportPhase1Request,
      Detail::DecodeSoulImportPhase1Response);
}

/**
 * Builds the endpoint thunk that confirms NPC import from a soul.
 * User Story: As NPC import completion flows, I need a reusable endpoint thunk
 * so a soul import can be finalized into an imported NPC record.
 */
inline Thunk<FImportedNpc> postNpcImportConfirm(const FSoulImportConfirmRequest &Request) {
  return Detail::MakePostWithCodec<FSoulImportConfirmRequest, FImportedNpc>(
      TEXT("postNpcImportConfirm"),
      SDKConfig::GetApiUrl() + TEXT("/npcs/import/confirm"), Request,
      Detail::EncodeSoulImportConfirmRequest, Detail::DecodeImportedNpc);
}

} // namespace Endpoints

} // namespace APISlice
