#pragma once

#include "API/APISlice.h"
#include "Async/Async.h"
#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "JsonObjectConverter.h"
#include "Misc/Paths.h"
#include "NativeEngine.h"
#include "RuntimeStore.h"
#include "Core/JsonInterop.h"
#include "Serialization/JsonSerializer.h"

namespace rtk {
namespace detail {

/**
 * Returns the singleton handle backing the local inference cortex.
 * User Story: As node-cortex thunks, I need a shared inference handle so model
 * initialization and inference reuse the same native runtime.
 */
inline Native::Llama::Context &NodeCortexHandle() {
  static Native::Llama::Context Handle = nullptr;
  return Handle;
}

/**
 * Returns the singleton handle backing the local embedding model.
 * User Story: As embedding thunks, I need a dedicated embedding handle so
 * vector generation does not conflict with the inference runtime.
 */
inline Native::Llama::Context &NodeEmbeddingHandle() {
  static Native::Llama::Context Handle = nullptr;
  return Handle;
}

/**
 * Returns the singleton handle backing the local sqlite memory database.
 * User Story: As node-memory thunks, I need a shared database handle so local
 * memory operations reuse one opened connection.
 */
inline Native::Sqlite::DB &NodeMemoryHandle() {
  static Native::Sqlite::DB Handle = nullptr;
  return Handle;
}

/**
 * Wraps an already available value in a resolved AsyncResult.
 * User Story: As thunk authors, I need synchronous values adapted into
 * AsyncResult so local helpers can plug into the thunk pipeline uniformly.
 */
template <typename T>
inline func::AsyncResult<T> ResolveAsync(const T &Value) {
  return func::AsyncResult<T>::create(
      [Value](std::function<void(T)> Resolve,
              std::function<void(std::string)> Reject) { Resolve(Value); });
}

/**
 * Wraps a UE string error in a rejected AsyncResult.
 * User Story: As thunk authors, I need UE-string failures adapted into
 * AsyncResult so error handling stays consistent with async thunks.
 */
template <typename T>
inline func::AsyncResult<T> RejectAsync(const FString &Error) {
  const std::string Utf8Error = TCHAR_TO_UTF8(*Error);
  return func::AsyncResult<T>::create(
      [Utf8Error](std::function<void(T)> Resolve,
                  std::function<void(std::string)> Reject) {
        Reject(Utf8Error);
      });
}

/**
 * Normalizes empty JSON payloads to an empty object literal.
 * User Story: As serializer helpers, I need empty payloads normalized so
 * outbound JSON contracts never receive blank strings unexpectedly.
 */
inline FString SafeJson(const FString &Json) {
  return Json.IsEmpty() ? TEXT("{}") : Json;
}

/**
 * Extracts the state JSON payload and normalizes empty values.
 * User Story: As protocol serializers, I need state payloads normalized so
 * downstream requests always receive valid JSON text.
 */
inline FString SafeStateJson(const FAgentState &State) {
  return SafeJson(State.JsonData);
}

/**
 * Reports whether the state carries a non-empty JSON payload.
 * User Story: As request builders, I need to detect meaningful state payloads
 * so I only serialize optional state when there is real data to send.
 */
inline bool HasStatePayload(const FAgentState &State) {
  return !State.JsonData.IsEmpty() && State.JsonData != TEXT("{}");
}

/**
 * Serializes a JSON object pointer into a string payload.
 * User Story: As thunk serializers, I need a shared object-to-string helper so
 * JSON payload generation stays consistent across instructions.
 */
inline FString JsonObjectToString(const TSharedPtr<FJsonObject> &Object) {
  return JsonInterop::StringifyObject(Object);
}

/**
 * Serializes an identify-actor result payload for protocol tooling.
 * User Story: As protocol execution, I need actor-identification results
 * wrapped in a stable JSON envelope so later instructions can consume them.
 */
inline FString SerializeIdentifyActorResult(const FNPCActorInfo &Actor) {
  const TSharedPtr<FJsonObject> ActorObject = MakeShared<FJsonObject>();
  ActorObject->SetStringField(TEXT("npcId"), Actor.NpcId);
  ActorObject->SetStringField(TEXT("persona"), Actor.Persona);
  ActorObject->SetObjectField(TEXT("data"), JsonInterop::StateToObject(Actor.Data));

  const TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
  Root->SetStringField(TEXT("type"), TEXT("IdentifyActorResult"));
  Root->SetObjectField(TEXT("actor"), ActorObject);
  return JsonObjectToString(Root);
}

/**
 * Serializes a decision intent into a decision result payload.
 * User Story: As protocol execution, I need decision intent wrapped in a
 * stable JSON envelope so later instructions can consume it consistently.
 */
inline FString SerializeDecisionResult(const FString &Goal, const FString &ActionType) {
  const TSharedPtr<FJsonObject> IntentObject = MakeShared<FJsonObject>();
  IntentObject->SetStringField(TEXT("goal"), Goal);
  IntentObject->SetStringField(TEXT("actionType"), ActionType);

  const TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
  Root->SetStringField(TEXT("type"), TEXT("Decision"));
  Root->SetObjectField(TEXT("decisionIntent"), IntentObject);
  return JsonObjectToString(Root);
}

/**
 * Serializes reasoning output into a reasoning result payload.
 * User Story: As protocol execution, I need reasoning output wrapped in a
 * stable JSON envelope so later instructions can consume it consistently.
 */
inline FString SerializeReasoningResult(const FString &ReasoningText, const FString &ResponseText) {
  const TSharedPtr<FJsonObject> ReasoningObject = MakeShared<FJsonObject>();
  ReasoningObject->SetStringField(TEXT("reasoningText"), ReasoningText);
  ReasoningObject->SetStringField(TEXT("responseText"), ResponseText);

  const TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
  Root->SetStringField(TEXT("type"), TEXT("Reasoning"));
  Root->SetObjectField(TEXT("reasoningOutput"), ReasoningObject);
  return JsonObjectToString(Root);
}

/**
 * Serializes generated output into an execute-inference result payload.
 * User Story: As protocol execution, I need inference output wrapped in a
 * stable JSON envelope so later instructions can consume it consistently.
 */
inline FString SerializeInferenceResult(const FString &GeneratedOutput) {
  const TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
  Root->SetStringField(TEXT("type"), TEXT("ExecuteInferenceResult"));
  Root->SetStringField(TEXT("generatedOutput"), GeneratedOutput);
  return JsonObjectToString(Root);
}

/**
 * Serializes recalled memories into a query-vector result payload.
 * User Story: As protocol execution, I need recalled memories wrapped in a
 * stable JSON envelope so follow-up instructions can read them consistently.
 */
/**
 * Recursively serializes memory items into a JSON value array.
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
inline void SerializeMemoryItemsRecursive(
    const TArray<FMemoryItem> &Memories,
    TArray<TSharedPtr<FJsonValue>> &Out, int32 Index) {
  Index < Memories.Num()
      ? [&]() {
          const FMemoryItem &Memory = Memories[Index];
          const TSharedPtr<FJsonObject> MemoryObject =
              MakeShared<FJsonObject>();
          MemoryObject->SetStringField(TEXT("text"), Memory.Text);
          MemoryObject->SetStringField(TEXT("type"), Memory.Type);
          MemoryObject->SetNumberField(TEXT("importance"), Memory.Importance);
          MemoryObject->SetNumberField(TEXT("similarity"), Memory.Similarity);
          Out.Add(MakeShared<FJsonValueObject>(MemoryObject));
          SerializeMemoryItemsRecursive(Memories, Out, Index + 1);
        }()
      : void();
}

inline FString SerializeQueryVectorResult(const TArray<FMemoryItem> &Memories) {
  TArray<TSharedPtr<FJsonValue>> MemoryValues;
  SerializeMemoryItemsRecursive(Memories, MemoryValues, 0);

  const TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
  Root->SetStringField(TEXT("type"), TEXT("QueryVectorResult"));
  Root->SetArrayField(TEXT("memories"), MemoryValues);
  return JsonObjectToString(Root);
}

/**
 * Returns the project-local infrastructure root used by native runtimes.
 * User Story: As local runtime setup, I need one canonical infrastructure root
 * so models and vector assets resolve from the same place.
 */
inline FString GetLocalInfrastructureDir() {
  return FPaths::ProjectDir() + TEXT("local_infrastructure/");
}

/**
 * Returns the default sqlite database path for node-backed memory.
 * User Story: As node-memory setup, I need a canonical database location so
 * local memory storage initializes predictably.
 */
inline FString DefaultNodeMemoryPath() {
  return GetLocalInfrastructureDir() +
         TEXT("vectors/forbocai_vectors.db");
}

/**
 * Returns the default embedding model path used for local vectorization.
 * User Story: As embedding setup, I need a canonical model path so vector
 * generation can boot without repeated path wiring.
 */
inline FString DefaultEmbeddingModelPath() {
  return GetLocalInfrastructureDir() + TEXT("models/all-MiniLM-L6-v2-Q4_K_M.gguf");
}

/**
 * Returns the mutable storage holding the active node-memory database path.
 * User Story: As node-memory setup, I need one mutable path slot so config and
 * initialization code share the active database target.
 */
inline FString &NodeMemoryPathStorage() {
  static FString Path = DefaultNodeMemoryPath();
  return Path;
}

/**
 * Returns the current node-memory database handle.
 * User Story: As node-memory helpers, I need one place to read the active
 * database handle so storage and recall use the same connection.
 */
inline Native::Sqlite::DB EnsureNodeMemoryDatabase() {
  return NodeMemoryHandle();
}

/**
 * Builds a persisted memory item from a memory-store instruction.
 * User Story: As node-memory store thunks, I need instructions converted into
 * persisted memory items so local storage uses a complete record shape.
 */
inline FMemoryItem MakeMemoryItem(const FMemoryStoreInstruction &Instruction) {
  FMemoryItem Item;
  Item.Id = FGuid::NewGuid().ToString();
  Item.Text = Instruction.Text;
  Item.Type = Instruction.Type.IsEmpty() ? TEXT("observation")
                                         : Instruction.Type;
  Item.Importance = Instruction.Importance;
  Item.Timestamp = FDateTime::UtcNow().ToUnixTimestamp();
  return Item;
}

/**
 * Converts a protocol instruction into the public agent response shape.
 * User Story: As protocol callers, I need typed instructions converted into
 * agent responses so gameplay code can consume dialogue and actions directly.
 */
inline FAgentResponse BuildAgentResponse(const FNPCInstruction &Instruction) {
  FAgentResponse Response;
  return (Response.Dialogue = Instruction.Dialogue,
          Response.Thought = Instruction.Dialogue,
          Instruction.bHasAction
              ? (Response.Action = Instruction.Action, void())
              : void(),
          Response);
}

/**
 * Upload a signed soul payload to Arweave.
 * User Story: As soul export flows, I need uploads handled with consistent
 * retry and result semantics so remote persistence behaves predictably.
 *
 * Matches TS SDK handler_ArweaveUpload behaviour:
 *  - 3 attempts max (configurable via MaxRetries)
 *  - Exponential backoff between retries: 250ms * 2^(attempt-1)
 *  - 60-second HTTP timeout per attempt
 *  - txId derivation order:
 *      1. (UE-only enhancement) x-id response header
 *      2. Response JSON ".id" field        (same as TS)
 *      3. Generated hash fallback          (same as TS)
 *  - On exhausted retries, resolves with error result (does NOT reject)
 */
inline func::AsyncResult<FArweaveUploadResult>
UploadSignedSoul(const FArweaveUploadInstruction &Instruction,
                 const FString &SignedPayload,
                 int32 MaxRetries = 3) {
  return func::AsyncResult<FArweaveUploadResult>::create(
      [Instruction, SignedPayload, MaxRetries](
          std::function<void(FArweaveUploadResult)> Resolve,
          std::function<void(std::string)> Reject) {
        const FString Url = !Instruction.UploadUrl.IsEmpty()
                                ? Instruction.UploadUrl
                                : Instruction.GatewayUrl;
        const FString Payload =
            !Instruction.PayloadJson.IsEmpty() ? Instruction.PayloadJson
                                               : SignedPayload;
        Url.IsEmpty()
            ? (Reject("Missing Arweave upload URL"), void())
            : [&]() {

        /**
         * Shared mutable attempt counter, accessed from the async task.
         * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
         */
        struct FRetryState {
          int32 Attempt = 0;
          int32 MaxRetries = 3;
          FString Url;
          FString Payload;
          FArweaveUploadInstruction Instruction;
          std::function<void(FArweaveUploadResult)> Resolve;
          std::function<void(std::string)> Reject;
        };

        TSharedPtr<FRetryState> State = MakeShared<FRetryState>();
        State->MaxRetries = MaxRetries;
        State->Url = Url;
        State->Payload = Payload;
        State->Instruction = Instruction;
        State->Resolve = Resolve;
        State->Reject = Reject;

        /**
         * Forward-declared so the lambda can reference itself for retries.
         * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
         */
        TSharedPtr<TFunction<void()>> TryOnce = MakeShared<TFunction<void()>>();
        *TryOnce = [State, TryOnce]() {
          State->Attempt += 1;

          TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
              FHttpModule::Get().CreateRequest();
          Request->SetURL(State->Url);
          Request->SetVerb(TEXT("POST"));
          Request->SetTimeout(60.0f); // 60-second timeout (matches TS SDK)
          Request->SetHeader(TEXT("Content-Type"),
                             State->Instruction.ContentType.IsEmpty()
                                 ? TEXT("application/octet-stream")
                                 : State->Instruction.ContentType);
          !State->Instruction.AuiAuthHeader.IsEmpty()
              ? (Request->SetHeader(TEXT("Authorization"),
                                    State->Instruction.AuiAuthHeader),
                 void())
              : void();
          !State->Instruction.TagsJson.IsEmpty()
              ? (Request->SetHeader(TEXT("X-Forboc-Tags"),
                                    State->Instruction.TagsJson),
                 void())
              : void();
          Request->SetContentAsString(State->Payload);

          Request->OnProcessRequestComplete().BindLambda(
              [State, TryOnce](FHttpRequestPtr Req, FHttpResponsePtr Res,
                               bool bWasSuccessful) {
                /**
                 * Retry helper: schedules backoff then re-invokes TryOnce.
                 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
                 */
                const auto ScheduleRetry = [&State, &TryOnce]() {
                  const float BackoffSec =
                      0.250f *
                      FMath::Pow(2.0f,
                                 static_cast<float>(State->Attempt - 1));
                  AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
                            [State, TryOnce, BackoffSec]() {
                              FPlatformProcess::Sleep(BackoffSec);
                              AsyncTask(ENamedThreads::GameThread,
                                        [TryOnce]() { (*TryOnce)(); });
                            });
                };

                /**
                 * Network-level failure → retry or exhaust.
                 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
                 */
                (!bWasSuccessful || !Res.IsValid())
                    ? (State->Attempt < State->MaxRetries
                           ? (ScheduleRetry(), void())
                           : [&State]() {
                               FArweaveUploadResult Result;
                               Result.StatusCode = 0;
                               Result.bSuccess = false;
                               Result.Status = TEXT("0");
                               Result.Error =
                                   TEXT("upload_request_failed:network_error");
                               Result.TxId = FString::Printf(
                                   TEXT("ar_tx_failed_%lld"),
                                   FDateTime::UtcNow().ToUnixTimestamp());
                               State->Resolve(Result);
                             }())
                    : [&State, &Res, &ScheduleRetry]() {
                        /**
                         * Got an HTTP response — build result and handle.
                         * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
                         */
                        FArweaveUploadResult Result;
                        Result.ResponseJson = Res->GetContentAsString();
                        Result.StatusCode = Res->GetResponseCode();
                        Result.bSuccess = Result.StatusCode >= 200 &&
                                          Result.StatusCode < 300;
                        Result.Status = FString::FromInt(Result.StatusCode);
                        Result.Error =
                            Result.bSuccess
                                ? FString(TEXT(""))
                                : FString::Printf(
                                      TEXT("upload_failed_status_%d"),
                                      Result.StatusCode);

                        /**
                         * Non-2xx: retry if attempts remain; otherwise derive
                         * txId and resolve.
                         * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
                         */
                        (!Result.bSuccess &&
                         State->Attempt < State->MaxRetries)
                            ? (ScheduleRetry(), void())
                            : (Result.TxId = Res->GetHeader(TEXT("x-id")),
                               Result.TxId.IsEmpty()
                                   ? [&Result]() {
                                       TSharedPtr<FJsonObject> ResponseObject;
                                       (JsonInterop::ParseJsonObject(
                                            Result.ResponseJson,
                                            ResponseObject) &&
                                        ResponseObject.IsValid())
                                           ? (Result.TxId =
                                                  ResponseObject
                                                      ->GetStringField(
                                                          TEXT("id")),
                                              void())
                                           : void();
                                     }()
                                   : void(),
                               Result.TxId.IsEmpty()
                                   ? (Result.TxId = LexToString(GetTypeHash(
                                          Result.ResponseJson)),
                                      void())
                                   : void(),
                               Result.ArweaveUrl =
                                   !State->Instruction.GatewayUrl.IsEmpty()
                                       ? State->Instruction.GatewayUrl +
                                             TEXT("/") + Result.TxId
                                       : TEXT(""),
                               State->Resolve(Result), void());
                      }();
              });
          Request->ProcessRequest();
        };

        /**
         * Fire the first attempt.
         * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
         */
        (*TryOnce)();
      }();
      });
}

/**
 * Download a soul payload from Arweave.
 * User Story: As soul import flows, I need download results normalized so
 * callers can validate and import remote payloads consistently.
 *
 * Matches TS SDK handler_ArweaveDownload behaviour:
 *  - 60-second HTTP timeout
 *  - No retry (single attempt)
 *  - Returns structured result with status/success/error fields
 */
inline func::AsyncResult<FArweaveDownloadResult>
DownloadSoulPayload(const FArweaveDownloadInstruction &Instruction) {
  return func::AsyncResult<FArweaveDownloadResult>::create(
      [Instruction](std::function<void(FArweaveDownloadResult)> Resolve,
                    std::function<void(std::string)> Reject) {
        const FString Url =
            !Instruction.DownloadUrl.IsEmpty()
                ? Instruction.DownloadUrl
                : (!Instruction.GatewayUrl.IsEmpty() &&
                           !Instruction.TxId.IsEmpty()
                       ? Instruction.GatewayUrl + TEXT("/") + Instruction.TxId
                       : FString());

        Url.IsEmpty()
            ? (Reject("Missing Arweave download URL"), void())
            : [&]() {
                TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
                    FHttpModule::Get().CreateRequest();
                Request->SetURL(Url);
                Request->SetVerb(TEXT("GET"));
                Request->SetTimeout(60.0f);
                Request->OnProcessRequestComplete().BindLambda(
                    [Instruction, Resolve, Reject](FHttpRequestPtr Req,
                                                   FHttpResponsePtr Res,
                                                   bool bWasSuccessful) {
                      (!bWasSuccessful || !Res.IsValid())
                          ? (Reject("Arweave download failed"), void())
                          : [&]() {
                              FArweaveDownloadResult Result;
                              Result.TxId =
                                  !Instruction.ExpectedTxId.IsEmpty()
                                      ? Instruction.ExpectedTxId
                                      : Instruction.TxId;
                              Result.Payload = Res->GetContentAsString();
                              Result.BodyJson = Result.Payload;
                              Result.StatusCode = Res->GetResponseCode();
                              Result.bSuccess = Result.StatusCode >= 200 &&
                                                Result.StatusCode < 300;
                              Result.Status =
                                  FString::FromInt(Result.StatusCode);
                              Result.Error =
                                  Result.bSuccess
                                      ? FString(TEXT(""))
                                      : FString::Printf(
                                            TEXT("download_failed_status_%d"),
                                            Result.StatusCode);
                              Result.ResponseJson = Result.Payload;
                              Resolve(Result);
                            }();
                    });
                Request->ProcessRequest();
              }();
      });
}

} // namespace detail
} // namespace rtk
