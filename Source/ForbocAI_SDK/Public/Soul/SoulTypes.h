#pragma once

// clang-format off
#include "CoreMinimal.h"
#include "Memory/MemoryTypes.h"
#include "NPC/NPCBaseTypes.h"
#include "SoulTypes.generated.h"
// clang-format on

USTRUCT(BlueprintType)
struct FArweaveUploadInstruction {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString UploadUrl;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString GatewayUrl;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString PayloadJson;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString ContentType;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString AuiAuthHeader;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString TagsJson;
};

USTRUCT(BlueprintType)
struct FArweaveUploadResult {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString TxId;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString Status;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  int32 StatusCode;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  bool bSuccess;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString Error;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString ArweaveUrl;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString ResponseJson;

  FArweaveUploadResult() : StatusCode(0), bSuccess(false) {}
};

USTRUCT(BlueprintType)
struct FArweaveDownloadInstruction {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString TxId;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString ExpectedTxId;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString GatewayUrl;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString DownloadUrl;
};

USTRUCT(BlueprintType)
struct FArweaveDownloadResult {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString TxId;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString BodyJson;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString Payload;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString Status;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  int32 StatusCode;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  bool bSuccess;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString Error;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString ResponseJson;

  FArweaveDownloadResult() : StatusCode(0), bSuccess(false) {}
};

USTRUCT(BlueprintType)
struct FSoul {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString Id;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString Version;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString Name;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString Persona;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  TArray<FMemoryItem> Memories;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FAgentState State;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString Signature;
};

USTRUCT(BlueprintType)
struct FSoulExportPhase1Request {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString NpcIdRef;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString Persona;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FAgentState NpcState;
};

USTRUCT(BlueprintType)
struct FSoulExportPhase1Response {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FArweaveUploadInstruction se1Instruction;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString se1SignedPayload;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString se1Signature;
};

USTRUCT(BlueprintType)
struct FSoulExportConfirmRequest {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FArweaveUploadResult secUploadResult;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString secSignedPayload;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString secSignature;
};

USTRUCT(BlueprintType)
struct FSoulExportResponse {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString TxId;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString ArweaveUrl;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString Signature;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FSoul Soul;
};

USTRUCT(BlueprintType)
struct FSoulImportPhase1Request {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString TxIdRef;
};

USTRUCT(BlueprintType)
struct FSoulImportPhase1Response {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FArweaveDownloadInstruction si1Instruction;
};

USTRUCT(BlueprintType)
struct FSoulImportConfirmRequest {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString sicTxId;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FArweaveDownloadResult sicDownloadResult;
};

USTRUCT(BlueprintType)
struct FSoulExportResult {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString TxId;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString Url;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FSoul Soul;
};

USTRUCT(BlueprintType)
struct FSoulListItem {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString TxId;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString Name;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString NpcId;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString ExportedAt;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString ArweaveUrl;
};

USTRUCT(BlueprintType)
struct FSoulListResponse {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  TArray<FSoulListItem> Souls;
};

USTRUCT(BlueprintType)
struct FSoulVerifyResult {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  bool bValid;

  UPROPERTY(BlueprintReadOnly, Category = "Soul")
  FString Reason;

  FSoulVerifyResult() : bValid(false) {}
};

namespace TypeFactory {

/**
 * Builds a soul value from its core fields.
 * User Story: As soul construction, I need a factory that assembles soul
 * records consistently before validation, export, or storage.
 */
inline FSoul Soul(const FString &Id, const FString &Version,
                  const FString &Name, const FString &Persona,
                  const FAgentState &State, const TArray<FMemoryItem> &Memories,
                  const FString &Signature = TEXT("")) {
  FSoul Value;
  Value.Id = Id;
  Value.Version = Version;
  Value.Name = Name;
  Value.Persona = Persona;
  Value.State = State;
  Value.Memories = Memories;
  Value.Signature = Signature;
  return Value;
}

/**
 * Builds the phase-one soul export request.
 * User Story: As soul export orchestration, I need a request factory so NPC
 * metadata is packaged consistently for the server handshake.
 */
inline FSoulExportPhase1Request
SoulExportPhase1Request(const FString &NpcIdRef, const FString &Persona,
                        const FAgentState &NpcState) {
  FSoulExportPhase1Request Value;
  Value.NpcIdRef = NpcIdRef;
  Value.Persona = Persona;
  Value.NpcState = NpcState;
  return Value;
}

/**
 * Builds the soul export confirmation request.
 * User Story: As soul export completion, I need a factory that bundles upload
 * evidence and signatures before confirming the export remotely.
 */
inline FSoulExportConfirmRequest
SoulExportConfirmRequest(const FArweaveUploadResult &UploadResult,
                         const FString &SignedPayload,
                         const FString &Signature) {
  FSoulExportConfirmRequest Value;
  Value.secUploadResult = UploadResult;
  Value.secSignedPayload = SignedPayload;
  Value.secSignature = Signature;
  return Value;
}

/**
 * Builds the phase-one soul import request.
 * User Story: As soul import orchestration, I need a request factory so the
 * transaction id is packaged consistently for the server lookup.
 */
inline FSoulImportPhase1Request
SoulImportPhase1Request(const FString &TxIdRef) {
  FSoulImportPhase1Request Value;
  Value.TxIdRef = TxIdRef;
  return Value;
}

/**
 * Builds the soul import confirmation request.
 * User Story: As soul import completion, I need a factory that carries the
 * downloaded payload metadata into the confirmation step.
 */
inline FSoulImportConfirmRequest
SoulImportConfirmRequest(const FString &TxId,
                         const FArweaveDownloadResult &DownloadResult) {
  FSoulImportConfirmRequest Value;
  Value.sicTxId = TxId;
  Value.sicDownloadResult = DownloadResult;
  return Value;
}

/**
 * Builds the soul export result payload.
 * User Story: As soul export consumers, I need a normalized result object so
 * later flows can reuse the transaction id, URL, and exported soul.
 */
inline FSoulExportResult SoulExportResult(const FString &TxId,
                                          const FString &Url,
                                          const FSoul &SoulValue) {
  FSoulExportResult Value;
  Value.TxId = TxId;
  Value.Url = Url;
  Value.Soul = SoulValue;
  return Value;
}

/**
 * Builds the soul verification result payload.
 * User Story: As soul verification flows, I need a consistent result type so
 * verification status and failure reasons can be passed downstream.
 */
inline FSoulVerifyResult SoulVerifyResult(bool bValid,
                                          const FString &Reason = TEXT("")) {
  FSoulVerifyResult Value;
  Value.bValid = bValid;
  Value.Reason = Reason;
  return Value;
}

} // namespace TypeFactory
