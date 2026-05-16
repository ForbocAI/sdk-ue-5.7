#pragma once

#include "Core/ThunkDetail.h"
#include "Errors.h"
#include "Memory/MemorySlice.h"
#include "NPC/NPCSlice.h"
#include "RuntimeConfig.h"
#include "Soul/SoulSlice.h"

namespace rtk {

/**
 * Soul thunks (mirrors TS soulSlice.ts)
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */

inline ThunkAction<FSoulExportResult, FStoreState>
exportSoulThunk(const FString &NpcId) {
  return [NpcId](std::function<AnyAction(const AnyAction &)> Dispatch,
                 std::function<FStoreState()> GetState)
             -> func::AsyncResult<FSoulExportResult> {
    const auto ApiKeyError = Errors::requireApiKeyGuidance(
        SDKConfig::GetApiUrl(), SDKConfig::GetApiKey());
    return ApiKeyError.hasValue
        ? detail::RejectAsync<FSoulExportResult>(ApiKeyError.value)
        : [&]() -> func::AsyncResult<FSoulExportResult> {
            const auto Npc = NPCSlice::SelectNPCById(GetState().NPCs, NpcId);
            return !Npc.hasValue
                ? detail::RejectAsync<FSoulExportResult>(
                      TEXT("NPC not found"))
                : (Dispatch(SoulSlice::Actions::RemoteExportSoulPending()),
                   func::AsyncChain::then<FSoulExportPhase1Response,
                                          FSoulExportResult>(
                       APISlice::Endpoints::postSoulExport(
                           NpcId,
                           TypeFactory::SoulExportPhase1Request(
                               NpcId, Npc.value.Persona, Npc.value.State))(
                           Dispatch, GetState),
                       [NpcId, Dispatch,
                        GetState](const FSoulExportPhase1Response &Phase1) {
                         return func::AsyncChain::then<FArweaveUploadResult,
                                                       FSoulExportResult>(
                             detail::UploadSignedSoul(
                                 Phase1.se1Instruction,
                                 Phase1.se1SignedPayload),
                             [NpcId, Phase1, Dispatch, GetState](
                                 const FArweaveUploadResult &UploadResult) {
                               const FSoulExportConfirmRequest Confirm =
                                   TypeFactory::SoulExportConfirmRequest(
                                       UploadResult,
                                       Phase1.se1SignedPayload,
                                       Phase1.se1Signature);

                               return func::AsyncChain::then<
                                   FSoulExportResponse, FSoulExportResult>(
                                   APISlice::Endpoints::
                                       postSoulExportConfirm(NpcId, Confirm)(
                                           Dispatch, GetState),
                                   [Dispatch](const FSoulExportResponse
                                                  &Response) {
                                     const FSoulExportResult Result =
                                         TypeFactory::SoulExportResult(
                                             Response.TxId,
                                             Response.ArweaveUrl,
                                             Response.Soul);
                                     Dispatch(
                                         SoulSlice::Actions::
                                             RemoteExportSoulSuccess(Result));
                                     return detail::ResolveAsync(Result);
                                   });
                             });
                       })
                       .catch_([Dispatch](std::string Error) {
                         Dispatch(SoulSlice::Actions::RemoteExportSoulFailed(
                             FString(UTF8_TO_TCHAR(Error.c_str()))));
                       }));
          }();
  };
}

inline ThunkAction<FSoulExportResult, FStoreState>
exportSoulThunk(const FSoul &Soul) {
  return [Soul](std::function<AnyAction(const AnyAction &)> Dispatch,
                std::function<FStoreState()> GetState)
             -> func::AsyncResult<FSoulExportResult> {
    const auto ApiKeyError = Errors::requireApiKeyGuidance(
        SDKConfig::GetApiUrl(), SDKConfig::GetApiKey());
    return ApiKeyError.hasValue
        ? detail::RejectAsync<FSoulExportResult>(ApiKeyError.value)
        : Soul.Id.IsEmpty()
            ? detail::RejectAsync<FSoulExportResult>(
                  TEXT("Soul ID is required"))
            : (Dispatch(SoulSlice::Actions::RemoteExportSoulPending()),
               func::AsyncChain::then<FSoulExportPhase1Response,
                                      FSoulExportResult>(
                   APISlice::Endpoints::postSoulExport(
                       Soul.Id,
                       TypeFactory::SoulExportPhase1Request(
                           Soul.Id, Soul.Persona, Soul.State))(Dispatch,
                                                                GetState),
                   [Soul, Dispatch,
                    GetState](const FSoulExportPhase1Response &Phase1) {
                     return func::AsyncChain::then<FArweaveUploadResult,
                                                   FSoulExportResult>(
                         detail::UploadSignedSoul(Phase1.se1Instruction,
                                                  Phase1.se1SignedPayload),
                         [Soul, Phase1, Dispatch, GetState](
                             const FArweaveUploadResult &UploadResult) {
                           const FSoulExportConfirmRequest Confirm =
                               TypeFactory::SoulExportConfirmRequest(
                                   UploadResult, Phase1.se1SignedPayload,
                                   Phase1.se1Signature);

                           return func::AsyncChain::then<
                               FSoulExportResponse, FSoulExportResult>(
                               APISlice::Endpoints::postSoulExportConfirm(
                                   Soul.Id, Confirm)(Dispatch, GetState),
                               [Soul,
                                Dispatch](const FSoulExportResponse
                                              &Response) {
                                 const FSoulExportResult Result =
                                     TypeFactory::SoulExportResult(
                                         Response.TxId, Response.ArweaveUrl,
                                         Response.Soul.Id.IsEmpty()
                                             ? Soul
                                             : Response.Soul);
                                 Dispatch(
                                     SoulSlice::Actions::
                                         RemoteExportSoulSuccess(Result));
                                 return detail::ResolveAsync(Result);
                               });
                         });
                   })
                   .catch_([Dispatch](std::string Error) {
                     Dispatch(SoulSlice::Actions::RemoteExportSoulFailed(
                         FString(UTF8_TO_TCHAR(Error.c_str()))));
                   }));
  };
}

inline ThunkAction<FSoul, FStoreState>
importSoulThunk(const FString &TxId) {
  return [TxId](std::function<AnyAction(const AnyAction &)> Dispatch,
                std::function<FStoreState()> GetState)
             -> func::AsyncResult<FSoul> {
    const auto ApiKeyError = Errors::requireApiKeyGuidance(
        SDKConfig::GetApiUrl(), SDKConfig::GetApiKey());
    return ApiKeyError.hasValue
        ? detail::RejectAsync<FSoul>(ApiKeyError.value)
        : (Dispatch(SoulSlice::Actions::ImportSoulPending()),
           func::AsyncChain::then<FSoulImportPhase1Response, FSoul>(
               APISlice::Endpoints::postNpcImport(
                   TypeFactory::SoulImportPhase1Request(TxId))(Dispatch,
                                                               GetState),
               [TxId, Dispatch,
                GetState](const FSoulImportPhase1Response &Phase1) {
                 return func::AsyncChain::then<FArweaveDownloadResult, FSoul>(
                     detail::DownloadSoulPayload(Phase1.si1Instruction),
                     [TxId, Dispatch, GetState](
                         const FArweaveDownloadResult &DownloadResult) {
                       return func::AsyncChain::then<FImportedNpc, FSoul>(
                           APISlice::Endpoints::postNpcImportConfirm(
                               TypeFactory::SoulImportConfirmRequest(
                                   TxId, DownloadResult))(Dispatch, GetState),
                           [Dispatch,
                            TxId](const FImportedNpc &ImportedNpc) {
                             FSoul Soul = TypeFactory::Soul(
                                 ImportedNpc.NpcId.IsEmpty()
                                     ? TxId
                                     : ImportedNpc.NpcId,
                                 TEXT("2.0.0"), ImportedNpc.NpcId,
                                 ImportedNpc.Persona,
                                 TypeFactory::AgentState(
                                     ImportedNpc.DataJson),
                                 TArray<FMemoryItem>());
                             Dispatch(SoulSlice::Actions::ImportSoulSuccess(
                                 Soul));
                             return detail::ResolveAsync(Soul);
                           });
                     });
               })
               .catch_([Dispatch](std::string Error) {
                 Dispatch(SoulSlice::Actions::ImportSoulFailed(
                     FString(UTF8_TO_TCHAR(Error.c_str()))));
               }));
  };
}

/**
 * Soul convenience thunks
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */

inline ThunkAction<FSoul, FStoreState>
localExportSoulThunk(const FString &NpcId = TEXT("")) {
  return [NpcId](std::function<AnyAction(const AnyAction &)> Dispatch,
                 std::function<FStoreState()> GetState)
             -> func::AsyncResult<FSoul> {
    const FString TargetNpcId =
        NpcId.IsEmpty() ? NPCSlice::SelectActiveNpcId(GetState().NPCs) : NpcId;
    const auto Npc = NPCSlice::SelectNPCById(GetState().NPCs, TargetNpcId);
    return !Npc.hasValue
        ? detail::RejectAsync<FSoul>(TEXT("NPC not found"))
        : detail::ResolveAsync(TypeFactory::Soul(
              TargetNpcId, TEXT("1.0.0"), TEXT("NPC"), Npc.value.Persona,
              Npc.value.State,
              MemorySlice::SelectAllMemories(GetState().Memory)));
  };
}

/**
 * Imports a Soul from a local JSON representation (no network).
 * Sets NPC info from the soul data and dispatches ImportSoulSuccess.
 * Mirrors TS localImportSoulThunk in soulSlice.ts.
 * User Story: As an SDK integrator, I need this type or module note so I can understand the role of the surrounding API surface quickly.
 */
inline ThunkAction<FSoul, FStoreState>
localImportSoulThunk(const FSoul &Soul) {
  return [Soul](std::function<AnyAction(const AnyAction &)> Dispatch,
                std::function<FStoreState()> GetState)
             -> func::AsyncResult<FSoul> {
    return Soul.Id.IsEmpty()
        ? detail::RejectAsync<FSoul>(TEXT("Soul ID is required"))
        : [&]() -> func::AsyncResult<FSoul> {
            FNPCInternalState Npc;
            Npc.Id = Soul.Id;
            Npc.Persona = Soul.Persona;
            Npc.State = Soul.State;
            Dispatch(NPCSlice::Actions::SetNPCInfo(Npc));
            Dispatch(SoulSlice::Actions::ImportSoulSuccess(Soul));
            return detail::ResolveAsync(Soul);
          }();
  };
}

inline ThunkAction<FSoulExportResult, FStoreState>
remoteExportSoulThunk(const FString &NpcId = TEXT("")) {
  return [NpcId](std::function<AnyAction(const AnyAction &)> Dispatch,
                 std::function<FStoreState()> GetState)
             -> func::AsyncResult<FSoulExportResult> {
    const FString TargetNpcId =
        NpcId.IsEmpty() ? NPCSlice::SelectActiveNpcId(GetState().NPCs) : NpcId;
    return exportSoulThunk(TargetNpcId)(Dispatch, GetState);
  };
}

inline ThunkAction<FSoul, FStoreState>
importSoulFromArweaveThunk(const FString &TxId) {
  return importSoulThunk(TxId);
}

inline ThunkAction<TArray<FSoulListItem>, FStoreState>
getSoulListThunk(int32 Limit = 50) {
  return [Limit](std::function<AnyAction(const AnyAction &)> Dispatch,
                 std::function<FStoreState()> GetState)
             -> func::AsyncResult<TArray<FSoulListItem>> {
    const auto ApiKeyError = Errors::requireApiKeyGuidance(
        SDKConfig::GetApiUrl(), SDKConfig::GetApiKey());
    return ApiKeyError.hasValue
        ? detail::RejectAsync<TArray<FSoulListItem>>(ApiKeyError.value)
        : func::AsyncChain::then<FSoulListResponse, TArray<FSoulListItem>>(
              APISlice::Endpoints::getSouls(Limit)(Dispatch, GetState),
              [Dispatch](const FSoulListResponse &Response) {
                Dispatch(SoulSlice::Actions::SetSoulList(Response.Souls));
                return detail::ResolveAsync(Response.Souls);
              });
  };
}

inline ThunkAction<FSoulVerifyResult, FStoreState>
verifySoulThunk(const FString &TxId) {
  return [TxId](std::function<AnyAction(const AnyAction &)> Dispatch,
                std::function<FStoreState()> GetState)
             -> func::AsyncResult<FSoulVerifyResult> {
    const auto ApiKeyError = Errors::requireApiKeyGuidance(
        SDKConfig::GetApiUrl(), SDKConfig::GetApiKey());
    return ApiKeyError.hasValue
        ? detail::RejectAsync<FSoulVerifyResult>(ApiKeyError.value)
        : APISlice::Endpoints::postSoulVerify(TxId)(Dispatch, GetState);
  };
}

inline ThunkAction<FImportedNpc, FStoreState>
importNpcFromSoulThunk(const FString &TxId) {
  return [TxId](std::function<AnyAction(const AnyAction &)> Dispatch,
                std::function<FStoreState()> GetState)
             -> func::AsyncResult<FImportedNpc> {
    const auto ApiKeyError = Errors::requireApiKeyGuidance(
        SDKConfig::GetApiUrl(), SDKConfig::GetApiKey());
    return ApiKeyError.hasValue
        ? detail::RejectAsync<FImportedNpc>(ApiKeyError.value)
        : func::AsyncChain::then<FSoulImportPhase1Response, FImportedNpc>(
              APISlice::Endpoints::postNpcImport(
                  TypeFactory::SoulImportPhase1Request(TxId))(Dispatch,
                                                              GetState),
              [TxId, Dispatch,
               GetState](const FSoulImportPhase1Response &Phase1) {
                return func::AsyncChain::then<FArweaveDownloadResult,
                                              FImportedNpc>(
                    detail::DownloadSoulPayload(Phase1.si1Instruction),
                    [TxId, Dispatch, GetState](
                        const FArweaveDownloadResult &DownloadResult) {
                      return func::AsyncChain::then<FImportedNpc,
                                                    FImportedNpc>(
                          APISlice::Endpoints::postNpcImportConfirm(
                              TypeFactory::SoulImportConfirmRequest(
                                  TxId, DownloadResult))(Dispatch, GetState),
                          [Dispatch,
                           TxId](const FImportedNpc &ImportedNpc) {
                            FNPCInternalState Npc;
                            Npc.Id = ImportedNpc.NpcId.IsEmpty()
                                         ? TxId
                                         : ImportedNpc.NpcId;
                            Npc.Persona = ImportedNpc.Persona;
                            Npc.State = TypeFactory::AgentState(
                                ImportedNpc.DataJson);
                            Dispatch(NPCSlice::Actions::SetNPCInfo(Npc));
                            return detail::ResolveAsync(ImportedNpc);
                          });
                    });
              });
  };
}

} // namespace rtk
