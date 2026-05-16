#pragma once

#include "../Core/AsyncHttp.h"
#include "../Core/JsonInterop.h"
#include "../Core/functional_core.hpp"
#include "../Core/rtk.hpp"
#include "../RuntimeConfig.h"
#include "../Types.h"
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

struct FStoreState;

namespace APISlice {

using namespace rtk;

extern rtk::ApiSlice<FStoreState> ForbocAiApi;

namespace Detail {

/**
 * Recursive helpers for codec array operations.
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
namespace detail {

/**
 * Recursively builds a JSON value array from a string array (stop values).
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
inline void BuildStopValuesRecursive(const TArray<FString> &Source,
                                     TArray<TSharedPtr<FJsonValue>> &Out,
                                     int32 Index) {
  Index < Source.Num()
      ? (Out.Add(MakeShared<FJsonValueString>(Source[Index])),
         BuildStopValuesRecursive(Source, Out, Index + 1), void())
      : void();
}

/**
 * Recursively extracts recalled memories from a JSON value array.
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
inline void ExtractRecalledMemoriesRecursive(
    const TArray<TSharedPtr<FJsonValue>> &Source, TArray<FRecalledMemory> &Out,
    int32 Index) {
  Index < Source.Num()
      ? ((Source[Index].IsValid() && Source[Index]->Type == EJson::Object)
             ? (Out.Add(
                    JsonInterop::RecalledMemoryFromObject(Source[Index]->AsObject())),
                void())
             : void(),
         ExtractRecalledMemoriesRecursive(Source, Out, Index + 1), void())
      : void();
}

/**
 * Recursively builds JSON value objects from recalled memories.
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
inline void BuildRecalledMemoriesRecursive(
    const TArray<FRecalledMemory> &Source,
    TArray<TSharedPtr<FJsonValue>> &Out, int32 Index) {
  Index < Source.Num()
      ? (Out.Add(MakeShared<FJsonValueObject>(
             JsonInterop::RecalledMemoryToObject(Source[Index]))),
         BuildRecalledMemoriesRecursive(Source, Out, Index + 1), void())
      : void();
}

/**
 * Recursively extracts memory store instructions from a JSON value array.
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
inline void ExtractMemoryStoreInstructionsRecursive(
    const TArray<TSharedPtr<FJsonValue>> &Source,
    TArray<FMemoryStoreInstruction> &Out, int32 Index) {
  Index < Source.Num()
      ? ((Source[Index].IsValid() && Source[Index]->Type == EJson::Object)
             ? (Out.Add(JsonInterop::MemoryStoreInstructionFromObject(
                    Source[Index]->AsObject())),
                void())
             : void(),
         ExtractMemoryStoreInstructionsRecursive(Source, Out, Index + 1),
         void())
      : void();
}

/**
 * Recursively extracts valid strings from a JSON value array.
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
inline void ExtractStringValuesRecursive(
    const TArray<TSharedPtr<FJsonValue>> &Source, TArray<FString> &Out,
    int32 Index) {
  Index < Source.Num()
      ? ((Source[Index].IsValid() && Source[Index]->Type != EJson::Null)
             ? (Out.Add(Source[Index]->AsString()), void())
             : void(),
         ExtractStringValuesRecursive(Source, Out, Index + 1), void())
      : void();
}

/**
 * Recursively extracts bridge rules from a JSON value array.
 * Forward-declared; defined after DecodeBridgeRuleObject.
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
inline void ExtractBridgeRulesRecursive(
    const TArray<TSharedPtr<FJsonValue>> &Source, TArray<FBridgeRule> &Out,
    int32 Index);

/**
 * Recursively extracts directive rulesets from a JSON value array.
 * Forward-declared; defined after DecodeDirectiveRuleSetObject.
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
inline void ExtractDirectiveRuleSetsRecursive(
    const TArray<TSharedPtr<FJsonValue>> &Source,
    TArray<FDirectiveRuleSet> &Out, int32 Index);

/**
 * Recursively extracts ghost error strings from a JSON value array.
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
inline void ExtractGhostErrorsRecursive(
    const TArray<TSharedPtr<FJsonValue>> &Source, TArray<FString> &Out,
    int32 Index) {
  Index < Source.Num()
      ? (Source[Index].IsValid()
             ? (Out.Add(Source[Index]->AsString()), void())
             : void(),
         ExtractGhostErrorsRecursive(Source, Out, Index + 1), void())
      : void();
}

/**
 * Recursively extracts ghost test result records from a JSON value array.
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
inline void ExtractGhostTestRecordsRecursive(
    const TArray<TSharedPtr<FJsonValue>> &Source,
    TArray<FGhostResultRecord> &Out, int32 Index) {
  Index < Source.Num()
      ? ((Source[Index].IsValid() && Source[Index]->Type == EJson::Object)
             ? [&]() {
                 const TSharedPtr<FJsonObject> Test =
                     Source[Index]->AsObject();
                 FGhostResultRecord Record;
                 Record.TestName = Test->GetStringField(TEXT("testName"));
                 Record.bTestPassed = JsonInterop::detail::TryGetBoolAs(
                     Test, TEXT("testPassed"), false);
                 Record.TestDuration = JsonInterop::detail::TryGetNumberAs<int64>(
                     Test, TEXT("testDuration"), 0);
                 Record.TestError = Test->GetStringField(TEXT("testError"));
                 Record.TestScreenshot =
                     Test->GetStringField(TEXT("testScreenshot"));
                 Out.Add(Record);
               }()
             : void(),
         ExtractGhostTestRecordsRecursive(Source, Out, Index + 1), void())
      : void();
}

/**
 * Recursively extracts ghost metric pairs from a JSON value array.
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
inline void ExtractGhostMetricPairsRecursive(
    const TArray<TSharedPtr<FJsonValue>> &Source, TMap<FString, float> &Out,
    int32 Index) {
  Index < Source.Num()
      ? ((Source[Index].IsValid() && Source[Index]->Type == EJson::Array)
             ? [&]() {
                 const TArray<TSharedPtr<FJsonValue>> Pair =
                     Source[Index]->AsArray();
                 (Pair.Num() == 2 && Pair[0].IsValid() && Pair[1].IsValid())
                     ? (Out.Add(Pair[0]->AsString(),
                                static_cast<float>(Pair[1]->AsNumber())),
                        void())
                     : void();
               }()
             : void(),
         ExtractGhostMetricPairsRecursive(Source, Out, Index + 1), void())
      : void();
}

/**
 * Recursively extracts ghost history entries from a JSON value array.
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
inline void ExtractGhostHistoryEntriesRecursive(
    const TArray<TSharedPtr<FJsonValue>> &Source,
    TArray<FGhostHistoryEntry> &Out, int32 Index);

/**
 * Helper to resolve a field with alias fallback.
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
inline FString FieldOrAlias(const TSharedPtr<FJsonObject> &Object,
                            const FString &Primary,
                            const FString &Alias) {
  return Object->HasField(Primary) ? Object->GetStringField(Primary)
                                   : Object->GetStringField(Alias);
}

/**
 * Helper to try reading a number field and assigning a float.
 * Returns the read value or default.
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
inline float TryGetPassRate(const TSharedPtr<FJsonObject> &Object,
                            const FString &Primary,
                            const FString &Alias) {
  double Value = 0.0;
  return (Object->TryGetNumberField(Primary, Value) ||
          Object->TryGetNumberField(Alias, Value))
             ? static_cast<float>(Value)
             : 0.0f;
}

} // namespace detail

/**
 * Serializes a UStruct-like value into JSON text.
 * User Story: As API request builders, I need a generic JSON encoder so typed
 * payloads can be posted without handwritten serialization per endpoint.
 */
template <typename T> inline FString ToJson(const T &Value) {
  FString Json;
  FJsonObjectConverter::UStructToJsonObjectString(Value, Json);
  return Json;
}

/**
 * URL-encodes a value for safe path and query usage.
 * User Story: As endpoint builders, I need path-safe encoding so ids and other
 * dynamic values can be inserted into API URLs without corruption.
 */
inline FString Encode(const FString &Value) {
  return FGenericPlatformHttp::UrlEncode(Value);
}

/**
 * Serializes a JSON object into text.
 * User Story: As codec helpers, I need object-to-string conversion so ad hoc
 * payloads can move through the generic HTTP helpers unchanged.
 */
inline FString ToJsonString(const TSharedRef<FJsonObject> &Object) {
  FString Json;
  const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
  FJsonSerializer::Serialize(Object, Writer);
  return Json;
}

/**
 * Tries to parse a JSON schema string into a JSON object field on target.
 * Returns void — sets the field only when parsing succeeds.
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
inline void TrySetJsonSchemaField(const TSharedRef<FJsonObject> &Target,
                                  const FString &JsonSchemaJson) {
  !JsonSchemaJson.IsEmpty()
      ? [&]() {
          TSharedPtr<FJsonObject> JsonSchema;
          const TSharedRef<TJsonReader<>> Reader =
              TJsonReaderFactory<>::Create(JsonSchemaJson);
          (FJsonSerializer::Deserialize(Reader, JsonSchema) &&
           JsonSchema.IsValid())
              ? (Target->SetObjectField(TEXT("jsonSchema"), JsonSchema), void())
              : void();
        }()
      : void();
}

/**
 * Builds the specialized payload for remote cortex completion.
 * User Story: As cortex completion flows, I need request shaping that omits
 * unset fields so the API only receives intentional completion options.
 */
inline FString
BuildCortexCompletePayload(const FCortexCompleteRequest &Request) {
  const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
  return (
      Payload->SetStringField(TEXT("prompt"), Request.Prompt),
      Request.MaxTokens >= 0
          ? (Payload->SetNumberField(TEXT("maxTokens"), Request.MaxTokens),
             void())
          : void(),
      Request.Temperature >= 0.0f
          ? (Payload->SetNumberField(TEXT("temperature"), Request.Temperature),
             void())
          : void(),
      Request.Stop.Num() > 0
          ? [&]() {
              TArray<TSharedPtr<FJsonValue>> StopValues;
              StopValues.Reserve(Request.Stop.Num());
              detail::BuildStopValuesRecursive(Request.Stop, StopValues, 0);
              Payload->SetArrayField(TEXT("stop"), StopValues);
            }()
          : void(),
      TrySetJsonSchemaField(Payload, Request.JsonSchemaJson),
      ToJsonString(Payload));
}

/**
 * Builds an injected RTK-style endpoint thunk from request metadata.
 * User Story: As API endpoint wiring, I need one endpoint factory so store
 * integration, tags, and request builders stay consistent across endpoints.
 */
template <typename Arg, typename Result>
inline ThunkAction<Result, FStoreState> MakeEndpoint(
    const FString &EndpointName, const Arg &ArgValue,
    std::function<func::AsyncResult<func::HttpResult<Result>>(const Arg &)>
        RequestBuilder,
    const TArray<FApiEndpointTag> &ProvidesTags = TArray<FApiEndpointTag>(),
    const TArray<FApiEndpointTag> &InvalidatesTags =
        TArray<FApiEndpointTag>()) {
  ApiEndpoint<Arg, Result> Endpoint;
  Endpoint.EndpointName = EndpointName;
  Endpoint.ProvidesTags = ProvidesTags;
  Endpoint.InvalidatesTags = InvalidatesTags;
  Endpoint.RequestBuilder = RequestBuilder;
  return injectEndpoint(ForbocAiApi, Endpoint)(ArgValue);
}

/**
 * Builds a GET endpoint thunk.
 * User Story: As read-only API calls, I need a GET factory so fetch endpoints
 * can reuse the same store integration path.
 */
template <typename Result>
inline ThunkAction<Result, FStoreState>
MakeGet(const FString &EndpointName, const FString &Url,
        const TArray<FApiEndpointTag> &Tags = TArray<FApiEndpointTag>()) {
  return MakeEndpoint<rtk::FEmptyPayload, Result>(
      EndpointName, rtk::FEmptyPayload{},
      [Url](const rtk::FEmptyPayload &) {
        return func::AsyncHttp::Get<Result>(Url, SDKConfig::GetApiKey());
      },
      Tags);
}

/**
 * Builds a POST endpoint thunk using struct-to-JSON encoding.
 * User Story: As write-oriented API calls, I need a POST factory so typed
 * request payloads can be serialized and dispatched uniformly.
 */
template <typename Request, typename Result>
inline ThunkAction<Result, FStoreState> MakePost(
    const FString &EndpointName, const FString &Url,
    const Request &RequestValue,
    const TArray<FApiEndpointTag> &Invalidates = TArray<FApiEndpointTag>()) {
  return MakeEndpoint<Request, Result>(
      EndpointName, RequestValue,
      [Url](const Request &Arg) {
        return func::AsyncHttp::Post<Result>(Url, ToJson(Arg),
                                             SDKConfig::GetApiKey());
      },
      TArray<FApiEndpointTag>(), Invalidates);
}

/**
 * Builds a DELETE endpoint thunk.
 * User Story: As destructive API calls, I need a DELETE factory so endpoint
 * removal flows share the same dispatch and invalidation behavior.
 */
template <typename Result>
inline ThunkAction<Result, FStoreState> MakeDelete(
    const FString &EndpointName, const FString &Url,
    const TArray<FApiEndpointTag> &Invalidates = TArray<FApiEndpointTag>()) {
  return MakeEndpoint<rtk::FEmptyPayload, Result>(
      EndpointName, rtk::FEmptyPayload{},
      [Url](const rtk::FEmptyPayload &) {
        return func::AsyncHttp::Delete<Result>(Url, SDKConfig::GetApiKey());
      },
      TArray<FApiEndpointTag>(), Invalidates);
}

/**
 * Decodes a raw string HTTP result into a typed HTTP result.
 * User Story: As endpoint codecs, I need shared decode handling so response
 * parsing and transport failures are mapped consistently.
 */
template <typename Result>
inline func::AsyncResult<func::HttpResult<Result>>
DecodeHttpResult(func::AsyncResult<func::HttpResult<FString>> RawResult,
                 std::function<bool(const FString &, Result &)> Decoder) {
  return func::AsyncResult<func::HttpResult<Result>>::create(
      [RawResult,
       Decoder](std::function<void(func::HttpResult<Result>)> Resolve,
                std::function<void(std::string)> Reject) {
        RawResult
            .then([Decoder, Resolve](const func::HttpResult<FString> &Http) {
              !Http.bSuccess
                  ? (Resolve(func::HttpResult<Result>::Failure(
                         Http.error, Http.ResponseCode)),
                     void())
                  : [&]() {
                      Result Parsed;
                      Decoder(Http.data, Parsed)
                          ? (Resolve(func::HttpResult<Result>::Success(
                                 Parsed, Http.ResponseCode)),
                             void())
                          : (Resolve(func::HttpResult<Result>::Failure(
                                 "JSON deserialization failed",
                                 Http.ResponseCode)),
                             void());
                    }();
            })
            .catch_([Resolve](std::string Error) {
              Resolve(func::HttpResult<Result>::Failure(Error));
            })
            .execute();
      });
}

/**
 * Builds a POST endpoint thunk that uses custom encode and decode functions.
 * User Story: As custom API codecs, I need one helper for hand-rolled payloads
 * so endpoints can bypass generic struct serialization when needed.
 */
template <typename Request, typename Result>
inline ThunkAction<Result, FStoreState> MakePostWithCodec(
    const FString &EndpointName, const FString &Url,
    const Request &RequestValue,
    std::function<FString(const Request &)> Encoder,
    std::function<bool(const FString &, Result &)> Decoder,
    const TArray<FApiEndpointTag> &Invalidates = TArray<FApiEndpointTag>()) {
  return MakeEndpoint<Request, Result>(
      EndpointName, RequestValue,
      [Url, Encoder, Decoder](const Request &Arg) {
        return DecodeHttpResult<Result>(
            func::AsyncHttp::Post<FString>(Url, Encoder(Arg),
                                           SDKConfig::GetApiKey()),
            Decoder);
      },
      TArray<FApiEndpointTag>(), Invalidates);
}

/**
 * Builds a GET endpoint thunk that decodes a custom response shape.
 * User Story: As custom API codecs, I need GET helpers with pluggable decoders
 * so complex JSON responses can still use the common endpoint pipeline.
 */
template <typename Result>
inline ThunkAction<Result, FStoreState> MakeGetWithCodec(
    const FString &EndpointName, const FString &Url,
    std::function<bool(const FString &, Result &)> Decoder,
    const TArray<FApiEndpointTag> &Tags = TArray<FApiEndpointTag>()) {
  return MakeEndpoint<rtk::FEmptyPayload, Result>(
      EndpointName, rtk::FEmptyPayload{},
      [Url, Decoder](const rtk::FEmptyPayload &) {
        return DecodeHttpResult<Result>(
            func::AsyncHttp::Get<FString>(Url, SDKConfig::GetApiKey()),
            Decoder);
      },
      Tags);
}

/**
 * Builds a POST endpoint thunk from raw JSON text plus a custom decoder.
 * User Story: As raw-payload endpoints, I need a helper that accepts JSON text
 * directly so already-shaped payloads can be posted without double encoding.
 */
template <typename Result>
inline ThunkAction<Result, FStoreState>
MakePostRawWithCodec(const FString &EndpointName, const FString &Url,
                     const FString &PayloadJson,
                     std::function<bool(const FString &, Result &)> Decoder) {
  return MakeEndpoint<FString, Result>(
      EndpointName, PayloadJson, [Url, Decoder](const FString &Arg) {
        return DecodeHttpResult<Result>(
            func::AsyncHttp::Post<FString>(Url, Arg, SDKConfig::GetApiKey()),
            Decoder);
      });
}

/**
 * Encodes an NPC process tape into a JSON object.
 * User Story: As protocol request composition, I need process tapes converted
 * into JSON objects so nested runtime state can be transmitted cleanly.
 */
inline TSharedRef<FJsonObject>
EncodeProcessTapeObject(const FNPCProcessTape &Tape) {
  const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
  TArray<TSharedPtr<FJsonValue>> Memories;
  return (
      Root->SetStringField(TEXT("observation"), Tape.Observation),
      JsonInterop::SetFieldFromJsonString(Root, TEXT("context"),
                                          Tape.ContextJson, false),
      Root->SetObjectField(TEXT("npcState"),
                           JsonInterop::StateToObject(Tape.NpcState)),
      !Tape.Persona.IsEmpty()
          ? (Root->SetStringField(TEXT("structuredPersona"), Tape.Persona), void())
          : void(),
      Tape.bHasActor
          ? [&]() {
              const TSharedRef<FJsonObject> Actor = MakeShared<FJsonObject>();
              Actor->SetStringField(TEXT("npcId"), Tape.Actor.NpcId);
              Actor->SetStringField(TEXT("persona"), Tape.Actor.Persona);
              Actor->SetObjectField(
                  TEXT("data"), JsonInterop::StateToObject(Tape.Actor.Data));
              Root->SetObjectField(TEXT("actor"), Actor);
            }()
          : void(),
      detail::BuildRecalledMemoriesRecursive(Tape.Memories, Memories, 0),
      Root->SetArrayField(TEXT("memories"), Memories),
      !Tape.Prompt.IsEmpty()
          ? (Root->SetStringField(TEXT("prompt"), Tape.Prompt), void())
          : void(),
      (!Tape.Prompt.IsEmpty() || !Tape.GeneratedOutput.IsEmpty())
          ? (Root->SetObjectField(
                 TEXT("constraints"),
                 JsonInterop::PromptConstraintsToObject(Tape.Constraints)),
             void())
          : void(),
      !Tape.GeneratedOutput.IsEmpty()
          ? (Root->SetStringField(TEXT("generatedOutput"),
                                  Tape.GeneratedOutput),
             void())
          : void(),
      !Tape.RulesetId.IsEmpty()
          ? (Root->SetStringField(TEXT("rulesetId"), Tape.RulesetId), void())
          : void(),
      Root->SetBoolField(TEXT("vectorQueried"), Tape.bVectorQueried),
      Tape.bDecisionCompleted
          ? [&]() {
              const TSharedRef<FJsonObject> Intent = MakeShared<FJsonObject>();
              Intent->SetStringField(TEXT("goal"), Tape.DecisionIntent.Goal);
              Intent->SetStringField(TEXT("actionType"), Tape.DecisionIntent.ActionType);
              !Tape.DecisionIntent.Target.IsEmpty()
                  ? (Intent->SetStringField(TEXT("target"), Tape.DecisionIntent.Target), void())
                  : void();
              Root->SetObjectField(TEXT("decisionIntent"), Intent);
            }()
          : void(),
      Tape.bReasoningCompleted
          ? [&]() {
              const TSharedRef<FJsonObject> Reasoning = MakeShared<FJsonObject>();
              Reasoning->SetStringField(TEXT("reasoningText"), Tape.ReasoningOutput.ReasoningText);
              Reasoning->SetStringField(TEXT("responseText"), Tape.ReasoningOutput.ResponseText);
              Root->SetObjectField(TEXT("reasoningOutput"), Reasoning);
            }()
          : void(),
      Root);
}

/**
 * Decodes a process tape JSON object into a typed tape value.
 * User Story: As protocol response handling, I need process tapes parsed back
 * into runtime types so downstream orchestration can inspect the turn state.
 */
inline bool DecodeProcessTapeObject(const TSharedPtr<FJsonObject> &Object,
                                    FNPCProcessTape &Tape) {
  return !Object.IsValid()
             ? false
             : (Tape.Observation = Object->GetStringField(TEXT("observation")),
                Tape.ContextJson =
                    JsonInterop::JsonStringFromField(Object, TEXT("context")),
                Tape.NpcState =
                    JsonInterop::StateFromField(Object, TEXT("npcState")),
                Tape.Persona = JsonInterop::OptionalStringFromField(
                    Object, TEXT("structuredPersona")),
                Tape.bHasActor =
                    Object->HasTypedField<EJson::Object>(TEXT("actor")),
                Tape.bHasActor
                    ? [&]() {
                        const TSharedPtr<FJsonObject> Actor =
                            Object->GetObjectField(TEXT("actor"));
                        Tape.Actor.NpcId =
                            Actor->GetStringField(TEXT("npcId"));
                        Tape.Actor.Persona =
                            Actor->GetStringField(TEXT("persona"));
                        Tape.Actor.Data =
                            JsonInterop::StateFromField(Actor, TEXT("data"));
                      }()
                    : void(),
                Tape.Memories.Empty(),
                [&]() {
                  const TArray<TSharedPtr<FJsonValue>> *MemoryValues = nullptr;
                  (Object->TryGetArrayField(TEXT("memories"), MemoryValues) &&
                   MemoryValues)
                      ? (detail::ExtractRecalledMemoriesRecursive(
                             *MemoryValues, Tape.Memories, 0),
                         void())
                      : void();
                }(),
                Tape.Prompt = JsonInterop::OptionalStringFromField(
                    Object, TEXT("prompt")),
                Object->HasTypedField<EJson::Object>(TEXT("constraints"))
                    ? (Tape.Constraints = JsonInterop::PromptConstraintsFromObject(
                           Object->GetObjectField(TEXT("constraints"))),
                       void())
                    : void(),
                Tape.GeneratedOutput = JsonInterop::OptionalStringFromField(
                    Object, TEXT("generatedOutput")),
                Tape.RulesetId = JsonInterop::OptionalStringFromField(
                    Object, TEXT("rulesetId")),
                Tape.bVectorQueried = JsonInterop::detail::TryGetBoolAs(
                    Object, TEXT("vectorQueried"), false),
                Tape.bDecisionCompleted = Object->HasTypedField<EJson::Object>(TEXT("decisionIntent")),
                Tape.bDecisionCompleted
                    ? [&]() {
                        const TSharedPtr<FJsonObject> Intent = Object->GetObjectField(TEXT("decisionIntent"));
                        Tape.DecisionIntent.Goal = Intent->GetStringField(TEXT("goal"));
                        Tape.DecisionIntent.ActionType = Intent->GetStringField(TEXT("actionType"));
                        Tape.DecisionIntent.Target = JsonInterop::OptionalStringFromField(Intent, TEXT("target"));
                      }()
                    : void(),
                Tape.bReasoningCompleted = Object->HasTypedField<EJson::Object>(TEXT("reasoningOutput")),
                Tape.bReasoningCompleted
                    ? [&]() {
                        const TSharedPtr<FJsonObject> Reasoning = Object->GetObjectField(TEXT("reasoningOutput"));
                        Tape.ReasoningOutput.ReasoningText = Reasoning->GetStringField(TEXT("reasoningText"));
                        Tape.ReasoningOutput.ResponseText = Reasoning->GetStringField(TEXT("responseText"));
                      }()
                    : void(),
                true);
}

/**
 * Encodes an NPC process request into JSON text.
 * User Story: As process endpoint callers, I need a request encoder so typed
 * process requests can be posted without duplicating JSON assembly logic.
 */
inline FString EncodeNpcProcessRequest(const FNPCProcessRequest &Request) {
  const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
  return (Root->SetObjectField(TEXT("tape"),
                               EncodeProcessTapeObject(Request.Tape)),
          Request.bHasLastResult
              ? (JsonInterop::SetFieldFromJsonString(
                     Root, TEXT("lastResult"), Request.LastResult, false),
                 void())
              : void(),
          ToJsonString(Root));
}

/**
 * Decodes an instruction object from JSON.
 * User Story: As process response handling, I need typed instruction parsing so
 * protocol actions can be reconstructed from the API payload.
 */
inline bool DecodeInstructionObject(const TSharedPtr<FJsonObject> &Object,
                                    FNPCInstruction &Instruction) {
  return !Object.IsValid()
             ? false
             : [&]() -> bool {
                 const FString Type = Object->GetStringField(TEXT("type"));
                 const func::Maybe<bool> Matched =
                     func::multi_match<FString, bool>(
                         Type,
                         {func::when<FString, bool>(
                              func::equals<FString>(TEXT("IdentifyActor")),
                              [&](const FString &) -> bool {
                                Instruction.Type =
                                    ENPCInstructionType::IdentifyActor;
                                return true;
                              }),
                          func::when<FString, bool>(
                              func::equals<FString>(TEXT("QueryVector")),
                              [&](const FString &) -> bool {
                                return (
                                    Instruction.Type =
                                        ENPCInstructionType::QueryVector,
                                    Instruction.Query =
                                        Object->GetStringField(TEXT("query")),
                                    Instruction.Limit =
                                        JsonInterop::detail::TryGetNumberAs<
                                            int32>(Object, TEXT("limit"),
                                                   Instruction.Limit),
                                    Instruction.Threshold =
                                        JsonInterop::detail::TryGetNumberAs<
                                            float>(Object, TEXT("threshold"),
                                                   Instruction.Threshold),
                                    true);
                              }),
                          func::when<FString, bool>(
                              func::equals<FString>(TEXT("ExecuteInference")),
                              [&](const FString &) -> bool {
                                return (
                                    Instruction.Type =
                                        ENPCInstructionType::ExecuteInference,
                                    Instruction.Prompt =
                                        Object->GetStringField(TEXT("prompt")),
                                    Object->HasTypedField<EJson::Object>(
                                        TEXT("constraints"))
                                        ? (Instruction.Constraints =
                                               JsonInterop::
                                                   PromptConstraintsFromObject(
                                                       Object->GetObjectField(
                                                           TEXT(
                                                               "constraints"))),
                                           void())
                                        : void(),
                                    true);
                              }),
                          func::when<FString, bool>(
                              func::equals<FString>(TEXT("Finalize")),
                              [&](const FString &) -> bool {
                                return (
                                    Instruction.Type =
                                        ENPCInstructionType::Finalize,
                                    Instruction.bValid =
                                        JsonInterop::detail::TryGetBoolAs(
                                            Object, TEXT("valid"), true),
                                    Instruction.Signature =
                                        JsonInterop::OptionalStringFromField(
                                            Object, TEXT("signature")),
                                    Instruction.StateTransform =
                                        JsonInterop::StateFromField(
                                            Object, TEXT("stateTransform")),
                                    Instruction.Dialogue =
                                        Object->GetStringField(
                                            TEXT("dialogue")),
                                    Instruction.bHasAction =
                                        Object->HasTypedField<EJson::Object>(
                                            TEXT("action")),
                                    Instruction.bHasAction
                                        ? (Instruction.Action =
                                               JsonInterop::ActionFromObject(
                                                   Object->GetObjectField(
                                                       TEXT("action"))),
                                           void())
                                        : void(),
                                    [&]() {
                                      const TArray<TSharedPtr<FJsonValue>>
                                          *MemoryValues = nullptr;
                                      (Object->TryGetArrayField(
                                           TEXT("memoryStore"),
                                           MemoryValues) &&
                                       MemoryValues)
                                          ? (Instruction.MemoryStore.Empty(
                                                 MemoryValues->Num()),
                                             detail::
                                                 ExtractMemoryStoreInstructionsRecursive(
                                                     *MemoryValues,
                                                     Instruction.MemoryStore,
                                                     0),
                                             void())
                                          : void();
                                    }(),
                                    true);
                              })});
                 return func::or_else(Matched, false);
               }();
}

/**
 * Decodes a process response into its instruction and tape payloads.
 * User Story: As the SDK protocol loop, I need one process-response decoder so
 * turn instructions and echoed tape state are reconstructed together.
 */
inline bool DecodeNpcProcessResponse(const FString &Json,
                                     FNPCProcessResponse &Response) {
  TSharedPtr<FJsonObject> Root;
  return (!JsonInterop::ParseJsonObject(Json, Root) || !Root.IsValid())
             ? false
             : (!Root->HasTypedField<EJson::Object>(TEXT("instruction")) ||
                !Root->HasTypedField<EJson::Object>(TEXT("tape")))
                   ? false
                   : (DecodeInstructionObject(
                          Root->GetObjectField(TEXT("instruction")),
                          Response.Instruction) &&
                      DecodeProcessTapeObject(
                          Root->GetObjectField(TEXT("tape")), Response.Tape));
}

/**
 * Encodes a directive request into JSON text.
 * User Story: As directive endpoint callers, I need a request encoder so
 * observation, state, and context fields are posted with a stable schema.
 */
inline FString EncodeDirectiveRequest(const FDirectiveRequest &Request) {
  const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
  return (Root->SetStringField(TEXT("observation"), Request.Observation),
          Root->SetObjectField(TEXT("npcState"),
                               JsonInterop::StateToObject(Request.NpcState)),
          JsonInterop::SetFieldFromJsonString(Root, TEXT("context"),
                                              Request.ContextJson, false),
          ToJsonString(Root));
}

/**
 * Decodes a directive response into the memory-recall instruction payload.
 * User Story: As directive endpoint callers, I need typed response decoding so
 * recall query, limit, and threshold values can drive the next lookup step.
 */
inline bool DecodeDirectiveResponse(const FString &Json,
                                    FDirectiveResponse &Response) {
  TSharedPtr<FJsonObject> Root;
  return (!JsonInterop::ParseJsonObject(Json, Root) || !Root.IsValid() ||
          !Root->HasTypedField<EJson::Object>(TEXT("memoryRecall")))
             ? false
             : [&]() -> bool {
                 const TSharedPtr<FJsonObject> Recall =
                     Root->GetObjectField(TEXT("memoryRecall"));
                 return (
                     Response.MemoryRecall.Query =
                         Recall->GetStringField(TEXT("query")),
                     Response.MemoryRecall.Limit =
                         JsonInterop::detail::TryGetNumberAs<int32>(
                             Recall, TEXT("limit"),
                             Response.MemoryRecall.Limit),
                     Response.MemoryRecall.Threshold =
                         JsonInterop::detail::TryGetNumberAs<float>(
                             Recall, TEXT("threshold"),
                             Response.MemoryRecall.Threshold),
                     true);
               }();
}

/**
 * Encodes a context-building request into JSON text.
 * User Story: As context endpoint callers, I need a request encoder so
 * observation, persona, and recalled memories reach the API in one payload.
 */
inline FString EncodeContextRequest(const FContextRequest &Request) {
  const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
  TArray<TSharedPtr<FJsonValue>> Memories;
  return (Root->SetStringField(TEXT("observation"), Request.Observation),
          Root->SetObjectField(TEXT("npcState"),
                               JsonInterop::StateToObject(Request.NpcState)),
          Root->SetStringField(TEXT("persona"), Request.Persona),
          detail::BuildRecalledMemoriesRecursive(Request.Memories, Memories, 0),
          Root->SetArrayField(TEXT("memories"), Memories),
          ToJsonString(Root));
}

/**
 * Decodes a context response into prompt and constraint data.
 * User Story: As context endpoint callers, I need typed response decoding so
 * prompt generation results can feed later inference and validation stages.
 */
inline bool DecodeContextResponse(const FString &Json,
                                  FContextResponse &Response) {
  TSharedPtr<FJsonObject> Root;
  return (!JsonInterop::ParseJsonObject(Json, Root) || !Root.IsValid())
             ? false
             : (Response.Prompt = Root->GetStringField(TEXT("prompt")),
                Root->HasTypedField<EJson::Object>(TEXT("constraints"))
                    ? (Response.Constraints =
                           JsonInterop::PromptConstraintsFromObject(
                               Root->GetObjectField(TEXT("constraints"))),
                       void())
                    : void(),
                true);
}

/**
 * Encodes a verdict request into JSON text.
 * User Story: As verdict endpoint callers, I need a request encoder so the
 * generated output and NPC state can be evaluated against the observation.
 */
inline FString EncodeVerdictRequest(const FVerdictRequest &Request) {
  const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
  return (
      Root->SetStringField(TEXT("generatedOutput"), Request.GeneratedOutput),
      Root->SetStringField(TEXT("observation"), Request.Observation),
      Root->SetObjectField(TEXT("npcState"),
                           JsonInterop::StateToObject(Request.NpcState)),
      ToJsonString(Root));
}

/**
 * Decodes a verdict response into a typed verdict result.
 * User Story: As verdict endpoint callers, I need response decoding so
 * validity, dialogue, actions, and memory updates return to runtime code.
 */
inline bool DecodeVerdictResponse(const FString &Json,
                                  FVerdictResponse &Response) {
  TSharedPtr<FJsonObject> Root;
  return (!JsonInterop::ParseJsonObject(Json, Root) || !Root.IsValid())
             ? false
             : (Response.bValid = JsonInterop::detail::TryGetBoolAs(
                    Root, TEXT("valid"), Response.bValid),
                Response.Signature = JsonInterop::OptionalStringFromField(
                    Root, TEXT("signature")),
                Response.StateDelta =
                    JsonInterop::StateFromField(Root, TEXT("stateDelta")),
                Response.Dialogue = Root->GetStringField(TEXT("dialogue")),
                Response.bHasAction =
                    Root->HasTypedField<EJson::Object>(TEXT("action")),
                Response.bHasAction
                    ? (Response.Action = JsonInterop::ActionFromObject(
                           Root->GetObjectField(TEXT("action"))),
                       void())
                    : void(),
                [&]() {
                  const TArray<TSharedPtr<FJsonValue>> *MemoryValues = nullptr;
                  (Root->TryGetArrayField(TEXT("memoryStore"), MemoryValues) &&
                   MemoryValues)
                      ? (Response.MemoryStore.Empty(MemoryValues->Num()),
                         detail::ExtractMemoryStoreInstructionsRecursive(
                             *MemoryValues, Response.MemoryStore, 0),
                         void())
                      : void();
                }(),
                true);
}

/**
 * Encodes a bridge-validation request into JSON text.
 * User Story: As bridge-rule validation callers, I need a request encoder so
 * action and validation context values are serialized consistently.
 */
inline FString
EncodeBridgeValidateRequest(const FBridgeValidateRequest &Request) {
  const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
  return (Root->SetObjectField(TEXT("action"),
                               JsonInterop::ActionToObject(Request.Action)),
          Root->SetObjectField(
              TEXT("context"),
              JsonInterop::ValidationContextToObject(Request.Context)),
          ToJsonString(Root));
}

/**
 * Decodes a validation response into a typed validation result.
 * User Story: As bridge-rule validation callers, I need one decoder so both
 * wrapped and direct validation payloads map into the same runtime type.
 */
inline bool DecodeValidationResult(const FString &Json,
                                   FValidationResult &Result) {
  TSharedPtr<FJsonObject> Root;
  return (!JsonInterop::ParseJsonObject(Json, Root) || !Root.IsValid())
             ? false
             : (Result = Root->HasTypedField<EJson::Object>(TEXT("brResult"))
                             ? JsonInterop::ValidationResultFromObject(
                                   Root->GetObjectField(TEXT("brResult")))
                             : JsonInterop::ValidationResultFromObject(Root),
                true);
}

/**
 * Reads a string array field from a JSON object.
 * User Story: As codec helpers, I need a reusable array reader so repeated
 * string-list decoding stays consistent across bridge and ruleset payloads.
 */
inline TArray<FString>
DecodeStringArrayField(const TSharedPtr<FJsonObject> &Object,
                       const FString &FieldName) {
  TArray<FString> Values;
  const TArray<TSharedPtr<FJsonValue>> *RawValues = nullptr;
  return (!Object.IsValid() ||
          !Object->TryGetArrayField(FieldName, RawValues) || !RawValues)
             ? Values
             : (Values.Reserve(RawValues->Num()),
                detail::ExtractStringValuesRecursive(*RawValues, Values, 0),
                Values);
}

/**
 * Decodes a bridge-rule object into a typed bridge rule.
 * User Story: As bridge rule management, I need rule-object decoding so mixed
 * API field shapes still produce one normalized bridge-rule value.
 */
inline bool DecodeBridgeRuleObject(const TSharedPtr<FJsonObject> &Object,
                                   FBridgeRule &Rule) {
  return !Object.IsValid()
             ? false
             : [&]() -> bool {
                 const FString RuleName1 =
                     JsonInterop::OptionalStringFromField(Object,
                                                          TEXT("ruleName"));
                 const FString RuleName2 =
                     !RuleName1.IsEmpty()
                         ? RuleName1
                         : JsonInterop::OptionalStringFromField(
                               Object, TEXT("brRuleId"));
                 Rule.RuleName =
                     !RuleName2.IsEmpty()
                         ? RuleName2
                         : JsonInterop::OptionalStringFromField(
                               Object, TEXT("ruleId"));

                 const FString RuleDesc1 =
                     JsonInterop::OptionalStringFromField(
                         Object, TEXT("ruleDescription"));
                 Rule.RuleDescription =
                     !RuleDesc1.IsEmpty()
                         ? RuleDesc1
                         : JsonInterop::OptionalStringFromField(
                               Object, TEXT("ruleReason"));

                 const TArray<FString> Actions1 =
                     DecodeStringArrayField(Object, TEXT("ruleActionTypes"));
                 const TArray<FString> Actions2 =
                     Actions1.Num() > 0
                         ? Actions1
                         : DecodeStringArrayField(Object,
                                                  TEXT("affectedActions"));
                 Rule.RuleActionTypes =
                     Actions2.Num() > 0
                         ? Actions2
                         : [&]() -> TArray<FString> {
                             const FString SingleAction =
                                 JsonInterop::OptionalStringFromField(
                                     Object, TEXT("ruleAction"));
                             TArray<FString> Result;
                             !SingleAction.IsEmpty()
                                 ? (Result.Add(SingleAction), void())
                                 : void();
                             return Result;
                           }();

                 return true;
               }();
}

/**
 * Recursive bridge-rule extraction definition.
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
namespace detail {
inline void ExtractBridgeRulesRecursive(
    const TArray<TSharedPtr<FJsonValue>> &Source, TArray<FBridgeRule> &Out,
    int32 Index) {
  Index < Source.Num()
      ? ((Source[Index].IsValid() && Source[Index]->Type == EJson::Object)
             ? [&]() {
                 FBridgeRule Rule;
                 DecodeBridgeRuleObject(Source[Index]->AsObject(), Rule)
                     ? (Out.Add(Rule), void())
                     : void();
               }()
             : void(),
         ExtractBridgeRulesRecursive(Source, Out, Index + 1), void())
      : void();
}
} // namespace detail

/**
 * Decodes a directive ruleset object into a typed ruleset value.
 * User Story: As directive rule management, I need ruleset-object decoding so
 * nested rule arrays become a usable runtime ruleset structure.
 */
inline bool DecodeDirectiveRuleSetObject(const TSharedPtr<FJsonObject> &Object,
                                         FDirectiveRuleSet &Ruleset) {
  return !Object.IsValid()
             ? false
             : (Ruleset.Id = JsonInterop::OptionalStringFromField(
                    Object, TEXT("id")),
                Ruleset.RulesetId = JsonInterop::OptionalStringFromField(
                    Object, TEXT("rulesetId")),
                Ruleset.Id.IsEmpty()
                    ? (Ruleset.Id = Ruleset.RulesetId, void())
                    : void(),
                Ruleset.RulesetRules.Empty(),
                [&]() {
                  const TArray<TSharedPtr<FJsonValue>> *RuleValues = nullptr;
                  (Object->TryGetArrayField(TEXT("rulesetRules"), RuleValues) &&
                   RuleValues)
                      ? (Ruleset.RulesetRules.Reserve(RuleValues->Num()),
                         detail::ExtractBridgeRulesRecursive(
                             *RuleValues, Ruleset.RulesetRules, 0),
                         void())
                      : void();
                }(),
                true);
}

/**
 * Recursive directive ruleset extraction definition.
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
namespace detail {
inline void ExtractDirectiveRuleSetsRecursive(
    const TArray<TSharedPtr<FJsonValue>> &Source,
    TArray<FDirectiveRuleSet> &Out, int32 Index) {
  Index < Source.Num()
      ? ((Source[Index].IsValid() && Source[Index]->Type == EJson::Object)
             ? [&]() {
                 FDirectiveRuleSet Ruleset;
                 DecodeDirectiveRuleSetObject(Source[Index]->AsObject(), Ruleset)
                     ? (Out.Add(Ruleset), void())
                     : void();
               }()
             : void(),
         ExtractDirectiveRuleSetsRecursive(Source, Out, Index + 1), void())
      : void();
}
} // namespace detail

/**
 * Decodes a bridge-rules response into a typed rule collection.
 * User Story: As bridge rule management, I need list-response decoding so API
 * rule inventories can be consumed without handwritten JSON walking.
 */
inline bool DecodeBridgeRulesResponse(const FString &Json,
                                      TArray<FBridgeRule> &Rules) {
  TArray<TSharedPtr<FJsonValue>> Values;
  return !JsonInterop::ParseJsonArray(Json, Values)
             ? false
             : (Rules.Empty(), Rules.Reserve(Values.Num()),
                detail::ExtractBridgeRulesRecursive(Values, Rules, 0), true);
}

/**
 * Decodes a directive-ruleset response into a single ruleset value.
 * User Story: As directive rule management, I need single-ruleset decoding so
 * details views and edits can load one ruleset cleanly.
 */
inline bool DecodeDirectiveRuleSetResponse(const FString &Json,
                                           FDirectiveRuleSet &Ruleset) {
  TSharedPtr<FJsonObject> Root;
  return (!JsonInterop::ParseJsonObject(Json, Root) || !Root.IsValid())
             ? false
             : DecodeDirectiveRuleSetObject(Root, Ruleset);
}

/**
 * Decodes a directive-ruleset list response into typed rulesets.
 * User Story: As directive rule management, I need list-response decoding so
 * ruleset indexes can hydrate strongly typed runtime collections.
 */
inline bool
DecodeDirectiveRuleSetListResponse(const FString &Json,
                                   TArray<FDirectiveRuleSet> &Rulesets) {
  TArray<TSharedPtr<FJsonValue>> Values;
  return !JsonInterop::ParseJsonArray(Json, Values)
             ? false
             : (Rulesets.Empty(), Rulesets.Reserve(Values.Num()),
                detail::ExtractDirectiveRuleSetsRecursive(Values, Rulesets, 0),
                true);
}

/**
 * Encodes the first soul-export request into JSON text.
 * User Story: As soul export flows, I need phase-one request encoding so NPC
 * identity, persona, and state can be sent for upload preparation.
 */
inline FString
EncodeSoulExportPhase1Request(const FSoulExportPhase1Request &Request) {
  const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
  return (Root->SetStringField(TEXT("npcIdRef"), Request.NpcIdRef),
          Root->SetStringField(TEXT("persona"), Request.Persona),
          Root->SetObjectField(TEXT("npcState"),
                               JsonInterop::StateToObject(Request.NpcState)),
          ToJsonString(Root));
}

/**
 * Decodes the first soul-export response into upload instructions.
 * User Story: As soul export flows, I need phase-one response decoding so the
 * client can execute upload instructions and preserve signed payload metadata.
 */
inline bool
DecodeSoulExportPhase1Response(const FString &Json,
                               FSoulExportPhase1Response &Response) {
  TSharedPtr<FJsonObject> Root;
  return (!JsonInterop::ParseJsonObject(Json, Root) || !Root.IsValid() ||
          !Root->HasTypedField<EJson::Object>(TEXT("se1Instruction")))
             ? false
             : (Response.se1Instruction =
                    JsonInterop::UploadInstructionFromObject(
                        Root->GetObjectField(TEXT("se1Instruction"))),
                Response.se1SignedPayload = JsonInterop::JsonStringFromField(
                    Root, TEXT("se1SignedPayload"), TEXT("")),
                Response.se1Signature =
                    JsonInterop::OptionalStringFromField(Root,
                                                         TEXT("se1Signature")),
                true);
}

/**
 * Encodes the soul-export confirmation request into JSON text.
 * User Story: As soul export flows, I need confirmation request encoding so
 * upload results and signatures can be posted back to finalize export.
 */
inline FString
EncodeSoulExportConfirmRequest(const FSoulExportConfirmRequest &Request) {
  const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
  return (Root->SetObjectField(
              TEXT("secUploadResult"),
              JsonInterop::UploadResultToObject(Request.secUploadResult)),
          JsonInterop::SetFieldFromJsonString(Root, TEXT("secSignedPayload"),
                                              Request.secSignedPayload, false),
          Root->SetStringField(TEXT("secSignature"), Request.secSignature),
          ToJsonString(Root));
}

/**
 * Decodes the final soul-export response into typed export details.
 * User Story: As soul export flows, I need final response decoding so the
 * transaction id, Arweave URL, and optional soul payload are available locally.
 */
inline bool DecodeSoulExportResponse(const FString &Json,
                                     FSoulExportResponse &Response) {
  TSharedPtr<FJsonObject> Root;
  return (!JsonInterop::ParseJsonObject(Json, Root) || !Root.IsValid())
             ? false
             : (Response.TxId = Root->GetStringField(TEXT("txId")),
                Response.ArweaveUrl = Root->GetStringField(TEXT("arweaveUrl")),
                Response.Signature = JsonInterop::OptionalStringFromField(
                    Root, TEXT("signature")),
                Root->HasTypedField<EJson::Object>(TEXT("soul"))
                    ? (Response.Soul = JsonInterop::SoulFromObject(
                           Root->GetObjectField(TEXT("soul"))),
                       void())
                    : void(),
                true);
}

/**
 * Encodes the first soul-import request into JSON text.
 * User Story: As soul import flows, I need phase-one request encoding so the
 * referenced transaction id can be sent to start remote retrieval.
 */
inline FString
EncodeSoulImportPhase1Request(const FSoulImportPhase1Request &Request) {
  const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
  return (Root->SetStringField(TEXT("txIdRef"), Request.TxIdRef),
          ToJsonString(Root));
}

/**
 * Decodes the first soul-import response into download instructions.
 * User Story: As soul import flows, I need phase-one response decoding so the
 * client can perform the requested download before confirming import.
 */
inline bool
DecodeSoulImportPhase1Response(const FString &Json,
                               FSoulImportPhase1Response &Response) {
  TSharedPtr<FJsonObject> Root;
  return (!JsonInterop::ParseJsonObject(Json, Root) || !Root.IsValid() ||
          !Root->HasTypedField<EJson::Object>(TEXT("si1Instruction")))
             ? false
             : (Response.si1Instruction =
                    JsonInterop::DownloadInstructionFromObject(
                        Root->GetObjectField(TEXT("si1Instruction"))),
                true);
}

/**
 * Encodes the soul-import confirmation request into JSON text.
 * User Story: As soul import flows, I need confirmation request encoding so
 * download results can be posted to complete the import handshake.
 */
inline FString
EncodeSoulImportConfirmRequest(const FSoulImportConfirmRequest &Request) {
  const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
  return (Root->SetStringField(TEXT("sicTxId"), Request.sicTxId),
          Root->SetObjectField(
              TEXT("sicDownloadResult"),
              JsonInterop::DownloadResultToObject(Request.sicDownloadResult)),
          ToJsonString(Root));
}

/**
 * Decodes an imported NPC payload into a typed NPC value.
 * User Story: As soul import flows, I need imported NPC decoding so runtime
 * systems can consume the restored character data directly.
 */
inline bool DecodeImportedNpc(const FString &Json, FImportedNpc &Npc) {
  TSharedPtr<FJsonObject> Root;
  return (!JsonInterop::ParseJsonObject(Json, Root) || !Root.IsValid())
             ? false
             : (Npc = JsonInterop::ImportedNpcFromObject(Root), true);
}

/**
 * Decodes a soul-verification response into a verification result.
 * User Story: As soul verification flows, I need response decoding so validity
 * and failure reasons map into one runtime verification record.
 */
inline bool DecodeSoulVerifyResponse(const FString &Json,
                                     FSoulVerifyResult &Response) {
  TSharedPtr<FJsonObject> Root;
  return (!JsonInterop::ParseJsonObject(Json, Root) || !Root.IsValid())
             ? false
             : [&]() -> bool {
                 const bool bVerifyValid =
                     JsonInterop::detail::TryGetBoolAs(Root,
                                                       TEXT("verifyValid"),
                                                       false);
                 const bool bHasVerifyValid =
                     Root->HasField(TEXT("verifyValid"));
                 Response.bValid =
                     bHasVerifyValid
                         ? bVerifyValid
                         : JsonInterop::detail::TryGetBoolAs(
                               Root, TEXT("valid"), false);

                 const FString VerifyReason =
                     JsonInterop::OptionalStringFromField(Root,
                                                          TEXT("verifyReason"));
                 Response.Reason =
                     !VerifyReason.IsEmpty()
                         ? VerifyReason
                         : JsonInterop::OptionalStringFromField(Root,
                                                                TEXT("reason"));
                 return true;
               }();
}

/**
 * Decodes a ghost-run response into run metadata.
 * User Story: As ghost execution flows, I need run-response decoding so the
 * created session id and initial run status are captured immediately.
 */
inline bool DecodeGhostRunResponse(const FString &Json,
                                   FGhostRunResponse &Response) {
  TSharedPtr<FJsonObject> Root;
  return (!JsonInterop::ParseJsonObject(Json, Root) || !Root.IsValid())
             ? false
             : (Response.SessionId = Root->GetStringField(TEXT("sessionId")),
                Response.RunStatus = Root->GetStringField(TEXT("runStatus")),
                true);
}

/**
 * Reads an int64 from either numeric or string JSON fields.
 * User Story: As ghost and history codecs, I need tolerant integer parsing so
 * timestamp fields decode correctly even when the API changes wire formats.
 */
inline int64 JsonNumberOrStringToInt64(const TSharedPtr<FJsonObject> &Object,
                                       const FString &FieldName) {
  return (!Object.IsValid() || !Object->HasField(FieldName))
             ? static_cast<int64>(0)
             : [&]() -> int64 {
                 const TSharedPtr<FJsonValue> Value =
                     Object->TryGetField(FieldName);
                 return !Value.IsValid()
                            ? static_cast<int64>(0)
                            : func::or_else(
                                  func::multi_match<EJson, int64>(
                                      Value->Type,
                                      {func::when<EJson, int64>(
                                           func::equals<EJson>(EJson::Number),
                                           [&](const EJson &) -> int64 {
                                             return static_cast<int64>(
                                                 Value->AsNumber());
                                           }),
                                       func::when<EJson, int64>(
                                           func::equals<EJson>(EJson::String),
                                           [&](const EJson &) -> int64 {
                                             return FCString::Atoi64(
                                                 *Value->AsString());
                                           })}),
                                  static_cast<int64>(0));
               }();
}

/**
 * Decodes a ghost-status response into a typed status snapshot.
 * User Story: As ghost execution flows, I need status-response decoding so
 * progress, timing, and error fields can drive polling and UI updates.
 */
inline bool DecodeGhostStatusResponse(const FString &Json,
                                      FGhostStatusResponse &Response) {
  TSharedPtr<FJsonObject> Root;
  return (!JsonInterop::ParseJsonObject(Json, Root) || !Root.IsValid())
             ? false
             : (Response.GhostSessionId =
                    Root->GetStringField(TEXT("ghostSessionId")),
                Response.GhostStatus =
                    Root->GetStringField(TEXT("ghostStatus")),
                Response.GhostProgress =
                    JsonInterop::detail::TryGetNumberAs<float>(
                        Root, TEXT("ghostProgress"), Response.GhostProgress),
                Response.GhostStartedAt =
                    JsonNumberOrStringToInt64(Root, TEXT("ghostStartedAt")),
                Response.GhostDuration =
                    JsonInterop::detail::TryGetNumberAs<int32>(
                        Root, TEXT("ghostDuration"), Response.GhostDuration),
                Response.GhostErrors.Empty(),
                [&]() {
                  Root->HasTypedField<EJson::Array>(TEXT("ghostErrors"))
                      ? [&]() {
                          const TArray<TSharedPtr<FJsonValue>> *Values =
                              nullptr;
                          (Root->TryGetArrayField(TEXT("ghostErrors"), Values) &&
                           Values)
                              ? (detail::ExtractGhostErrorsRecursive(
                                     *Values, Response.GhostErrors, 0),
                                 void())
                              : void();
                        }()
                      : (Root->HasField(TEXT("ghostErrors"))
                             ? [&]() {
                                 const TSharedPtr<FJsonValue> Value =
                                     Root->TryGetField(TEXT("ghostErrors"));
                                 Value.IsValid()
                                     ? (func::or_else(
                                           func::multi_match<EJson, bool>(
                                               Value->Type,
                                               {func::when<EJson, bool>(
                                                    func::equals<EJson>(
                                                        EJson::String),
                                                    [&](const EJson &) -> bool {
                                                      !Value->AsString()
                                                               .IsEmpty()
                                                          ? (Response
                                                                 .GhostErrors
                                                                 .Add(Value
                                                                          ->AsString()),
                                                             void())
                                                          : void();
                                                      return true;
                                                    }),
                                                func::when<EJson, bool>(
                                                    func::equals<EJson>(
                                                        EJson::Number),
                                                    [&](const EJson &) -> bool {
                                                      const int32 Count =
                                                          static_cast<int32>(
                                                              Value
                                                                  ->AsNumber());
                                                      Count > 0
                                                          ? (Response
                                                                 .GhostErrors
                                                                 .Add(FString::
                                                                          FromInt(
                                                                              Count)),
                                                             void())
                                                          : void();
                                                      return true;
                                                    })}),
                                           false), void())
                                     : void();
                               }()
                             : void());
                }(),
                true);
}

/**
 * Decodes a ghost-results response into typed result records.
 * User Story: As ghost execution flows, I need results-response decoding so
 * aggregate metrics and per-test outcomes can be inspected in tooling.
 */
inline bool DecodeGhostResultsResponse(const FString &Json,
                                       FGhostResultsResponse &Response) {
  TSharedPtr<FJsonObject> Root;
  return (!JsonInterop::ParseJsonObject(Json, Root) || !Root.IsValid())
             ? false
             : (Response.ResultsSessionId =
                    Root->GetStringField(TEXT("resultsSessionId")),
                Response.ResultsTotalTests =
                    JsonInterop::detail::TryGetNumberAs<int32>(
                        Root, TEXT("resultsTotalTests"),
                        Response.ResultsTotalTests),
                Response.ResultsPassed =
                    JsonInterop::detail::TryGetNumberAs<int32>(
                        Root, TEXT("resultsPassed"), Response.ResultsPassed),
                Response.ResultsFailed =
                    JsonInterop::detail::TryGetNumberAs<int32>(
                        Root, TEXT("resultsFailed"), Response.ResultsFailed),
                Response.ResultsSkipped =
                    JsonInterop::detail::TryGetNumberAs<int32>(
                        Root, TEXT("resultsSkipped"), Response.ResultsSkipped),
                Response.ResultsDuration =
                    JsonInterop::detail::TryGetNumberAs<int64>(
                        Root, TEXT("resultsDuration"),
                        Response.ResultsDuration),
                Response.ResultsCoverage =
                    JsonInterop::detail::TryGetNumberAs<float>(
                        Root, TEXT("resultsCoverage"),
                        Response.ResultsCoverage),
                Response.ResultsTests.Empty(),
                [&]() {
                  const TArray<TSharedPtr<FJsonValue>> *Tests = nullptr;
                  (Root->TryGetArrayField(TEXT("resultsTests"), Tests) && Tests)
                      ? (detail::ExtractGhostTestRecordsRecursive(
                             *Tests, Response.ResultsTests, 0),
                         void())
                      : void();
                }(),
                Response.ResultsMetrics.Empty(),
                [&]() {
                  const TArray<TSharedPtr<FJsonValue>> *MetricPairs = nullptr;
                  (Root->TryGetArrayField(TEXT("resultsMetrics"), MetricPairs) &&
                   MetricPairs)
                      ? (detail::ExtractGhostMetricPairsRecursive(
                             *MetricPairs, Response.ResultsMetrics, 0),
                         void())
                      : void();
                }(),
                true);
}

/**
 * Decodes a ghost-stop response into typed stop metadata.
 * User Story: As ghost execution flows, I need stop-response decoding so the
 * caller can confirm the target session stopped and record its final state.
 */
inline bool DecodeGhostStopResponse(const FString &Json,
                                    FGhostStopResponse &Response) {
  TSharedPtr<FJsonObject> Root;
  return (!JsonInterop::ParseJsonObject(Json, Root) || !Root.IsValid())
             ? false
             : (Response.StopStatus = Root->GetStringField(TEXT("stopStatus")),
                Response.StopSessionId =
                    Root->GetStringField(TEXT("stopSessionId")),
                Response.bStopped = Response.StopStatus.Equals(
                    TEXT("stopped"), ESearchCase::IgnoreCase),
                true);
}

/**
 * Recursive ghost history entry extraction definition.
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
namespace detail {
inline void ExtractGhostHistoryEntriesRecursive(
    const TArray<TSharedPtr<FJsonValue>> &Source,
    TArray<FGhostHistoryEntry> &Out, int32 Index) {
  Index < Source.Num()
      ? ((Source[Index].IsValid() && Source[Index]->Type == EJson::Object)
             ? [&]() {
                 const TSharedPtr<FJsonObject> Session =
                     Source[Index]->AsObject();
                 FGhostHistoryEntry Entry;
                 Entry.SessionId =
                     FieldOrAlias(Session, TEXT("sessionId"),
                                  TEXT("histSessionId"));
                 Entry.TestSuite =
                     FieldOrAlias(Session, TEXT("testSuite"),
                                  TEXT("histTestSuite"));
                 Entry.StartedAt = JsonNumberOrStringToInt64(
                     Session,
                     Session->HasField(TEXT("startedAt"))
                         ? TEXT("startedAt")
                         : TEXT("histStartedAt"));
                 Entry.CompletedAt = JsonNumberOrStringToInt64(
                     Session,
                     Session->HasField(TEXT("completedAt"))
                         ? TEXT("completedAt")
                         : TEXT("histCompletedAt"));
                 Entry.Status =
                     FieldOrAlias(Session, TEXT("status"), TEXT("histStatus"));
                 Entry.PassRate =
                     TryGetPassRate(Session, TEXT("passRate"),
                                   TEXT("histPassRate"));
                 Out.Add(Entry);
               }()
             : void(),
         ExtractGhostHistoryEntriesRecursive(Source, Out, Index + 1), void())
      : void();
}
} // namespace detail

/**
 * Decodes a ghost-history response into typed session history.
 * User Story: As ghost execution flows, I need history-response decoding so
 * prior sessions can be listed with ids, timing, status, and pass-rate data.
 */
inline bool DecodeGhostHistoryResponse(const FString &Json,
                                       FGhostHistoryResponse &Response) {
  TSharedPtr<FJsonObject> Root;
  return (!JsonInterop::ParseJsonObject(Json, Root) || !Root.IsValid())
             ? false
             : [&]() -> bool {
                 const TArray<TSharedPtr<FJsonValue>> *Sessions = nullptr;
                 return (!Root->TryGetArrayField(TEXT("sessions"), Sessions) ||
                         !Sessions)
                            ? false
                            : (Response.Sessions.Empty(Sessions->Num()),
                               detail::ExtractGhostHistoryEntriesRecursive(
                                   *Sessions, Response.Sessions, 0),
                               true);
               }();
}

} // namespace Detail

} // namespace APISlice
