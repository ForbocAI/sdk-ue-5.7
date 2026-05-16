#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Types.h"
#include "Core/functional_core.hpp"

namespace JsonInterop {

namespace detail {

/**
 * Sets a string field only when the value is non-empty.
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
inline void SetIfNonEmpty(const TSharedRef<FJsonObject> &Object,
                          const FString &FieldName, const FString &Value) {
  !Value.IsEmpty() ? (Object->SetStringField(FieldName, Value), void())
                   : void();
}

/**
 * Tries to read a number field and cast it to the target type.
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
template <typename T>
inline T TryGetNumberAs(const TSharedPtr<FJsonObject> &Object,
                        const FString &FieldName, T Default) {
  double Value = static_cast<double>(Default);
  return Object->TryGetNumberField(FieldName, Value) ? static_cast<T>(Value)
                                                     : Default;
}

/**
 * Tries to read a bool field, returning the default when absent.
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
inline bool TryGetBoolAs(const TSharedPtr<FJsonObject> &Object,
                         const FString &FieldName, bool Default) {
  bool Value = Default;
  return Object->TryGetBoolField(FieldName, Value) ? Value : Default;
}

/**
 * Recursively builds a JSON value array from a string array.
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
 * Recursively extracts strings from a JSON value array with validity check.
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
inline void ExtractStopValuesRecursive(
    const TArray<TSharedPtr<FJsonValue>> &Source, TArray<FString> &Out,
    int32 Index) {
  Index < Source.Num()
      ? (Source[Index].IsValid()
             ? (Out.Add(Source[Index]->AsString()), void())
             : void(),
         ExtractStopValuesRecursive(Source, Out, Index + 1), void())
      : void();
}

} // namespace detail

/**
 * Parses a JSON object string, defaulting to an empty object on blank input.
 * User Story: As JSON-backed runtime parsing, I need safe object parsing so
 * blank or malformed payloads do not crash reducers and thunks.
 */
inline bool ParseJsonObject(const FString &Json,
                             TSharedPtr<FJsonObject> &OutObject) {
  return Json.IsEmpty()
             ? (OutObject = MakeShared<FJsonObject>(), true)
             : [&]() -> bool {
                 TSharedRef<TJsonReader<>> Reader =
                     TJsonReaderFactory<>::Create(Json);
                 return (FJsonSerializer::Deserialize(Reader, OutObject) &&
                         OutObject.IsValid())
                            ? true
                            : (OutObject = MakeShared<FJsonObject>(), false);
               }();
}

/**
 * Parses a JSON array string into UE JSON values.
 * User Story: As JSON-backed runtime parsing, I need array payloads decoded so
 * API responses and config payloads can be traversed safely.
 */
inline bool ParseJsonArray(const FString &Json,
                           TArray<TSharedPtr<FJsonValue>> &OutArray) {
  TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
  return FJsonSerializer::Deserialize(Reader, OutArray);
}

/**
 * Serializes a JSON object to a string, using `{}` when the pointer is null.
 * User Story: As JSON serialization flows, I need null-safe object encoding so
 * outbound payload construction stays predictable.
 */
inline FString StringifyObject(const TSharedPtr<FJsonObject> &Object) {
  FString Json;
  const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
  return Object.IsValid()
             ? (FJsonSerializer::Serialize(Object.ToSharedRef(), Writer), Json)
             : (Writer->WriteObjectStart(), Writer->WriteObjectEnd(),
                Writer->Close(), Json);
}

/**
 * Serializes a JSON array to a string.
 * User Story: As JSON serialization flows, I need array values encoded so
 * runtime payloads can move between structs and HTTP bodies.
 */
inline FString StringifyArray(const TArray<TSharedPtr<FJsonValue>> &Array) {
  FString Json;
  const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
  FJsonSerializer::Serialize(Array, Writer);
  return Json;
}

/**
 * Serializes a generic JSON value to its string representation.
 * User Story: As JSON normalization flows, I need one value serializer so
 * mixed-type fields can be preserved without custom branching everywhere.
 */
inline FString StringifyValue(const TSharedPtr<FJsonValue> &Value) {
  return !Value.IsValid()
             ? FString(TEXT("null"))
             : func::or_else(
                   func::multi_match<EJson, FString>(
                       Value->Type,
                       {func::when<EJson, FString>(
                            func::equals<EJson>(EJson::Object),
                            [&](const EJson &) {
                              return StringifyObject(Value->AsObject());
                            }),
                        func::when<EJson, FString>(
                            func::equals<EJson>(EJson::Array),
                            [&](const EJson &) {
                              return StringifyArray(Value->AsArray());
                            }),
                        func::when<EJson, FString>(
                            func::equals<EJson>(EJson::String),
                            [&](const EJson &) {
                              return FString::Printf(TEXT("\"%s\""),
                                                     *Value->AsString());
                            }),
                        func::when<EJson, FString>(
                            func::equals<EJson>(EJson::Boolean),
                            [&](const EJson &) {
                              return Value->AsBool()
                                         ? FString(TEXT("true"))
                                         : FString(TEXT("false"));
                            }),
                        func::when<EJson, FString>(
                            func::equals<EJson>(EJson::Number),
                            [&](const EJson &) {
                              return FString::SanitizeFloat(
                                  Value->AsNumber());
                            })}),
                   FString(TEXT("null")));
}

/**
 * Returns true when an object contains a field whose value is not JSON null.
 * User Story: As defensive JSON readers, I need nullable field checks so
 * optional payload fields can be handled without unsafe assumptions.
 */
inline bool HasNonNullField(const TSharedPtr<FJsonObject> &Object,
                            const FString &FieldName) {
  return (!Object.IsValid() || !Object->HasField(FieldName))
             ? false
             : [&]() -> bool {
                 const TSharedPtr<FJsonValue> Value =
                     Object->TryGetField(FieldName);
                 return Value.IsValid() && Value->Type != EJson::Null;
               }();
}

/**
 * Reads a field as a string, falling back to serialized JSON or a default.
 * User Story: As JSON field readers, I need a tolerant string extractor so
 * scalar and structured fields can be consumed through one helper.
 */
inline FString OptionalStringFromField(const TSharedPtr<FJsonObject> &Object,
                                       const FString &FieldName,
                                       const FString &Fallback = TEXT("")) {
  return !HasNonNullField(Object, FieldName)
             ? Fallback
             : [&]() -> FString {
                 const TSharedPtr<FJsonValue> Value =
                     Object->TryGetField(FieldName);
                 return !Value.IsValid()
                            ? Fallback
                            : (Value->Type == EJson::String
                                   ? Value->AsString()
                                   : StringifyValue(Value));
               }();
}

/**
 * Parses an object string and always returns a valid JSON object pointer.
 * User Story: As downstream JSON consumers, I need a guaranteed object handle
 * so later helpers can operate without repeated null checks.
 */
inline TSharedPtr<FJsonObject> ParseJsonObjectOrEmpty(const FString &Json) {
  TSharedPtr<FJsonObject> Object;
  ParseJsonObject(Json, Object);
  return Object.IsValid() ? Object : MakeShared<FJsonObject>();
}

/**
 * Reads a field as JSON text, preserving objects and arrays when present.
 * User Story: As payload passthrough flows, I need raw JSON field extraction so
 * nested state blobs can survive round-trips without lossy conversion.
 */
inline FString JsonStringFromField(const TSharedPtr<FJsonObject> &Object,
                                   const FString &FieldName,
                                   const FString &Fallback = TEXT("{}")) {
  return (!Object.IsValid() || !Object->HasField(FieldName))
             ? Fallback
             : [&]() -> FString {
                 const TSharedPtr<FJsonValue> Value =
                     Object->TryGetField(FieldName);
                 return (!Value.IsValid() || Value->Type == EJson::Null)
                            ? Fallback
                            : ((Value->Type == EJson::Object ||
                                Value->Type == EJson::Array)
                                   ? StringifyValue(Value)
                                   : (Value->Type == EJson::String
                                          ? Value->AsString()
                                          : StringifyValue(Value)));
               }();
}

/**
 * Writes a JSON string field back as object, array, or plain string content.
 * User Story: As payload assembly flows, I need JSON text rehydrated into the
 * right field type so outbound request bodies keep their structure.
 */
inline void SetFieldFromJsonString(const TSharedRef<FJsonObject> &Object,
                                   const FString &FieldName,
                                   const FString &JsonString,
                                   bool bOmitWhenEmpty = true) {
  JsonString.IsEmpty()
      ? (bOmitWhenEmpty
             ? void()
             : (Object->SetObjectField(FieldName, MakeShared<FJsonObject>()),
                void()))
      : [&]() {
          TSharedPtr<FJsonObject> JsonObject;
          (ParseJsonObject(JsonString, JsonObject) && JsonObject.IsValid())
              ? (Object->SetObjectField(FieldName, JsonObject), void())
              : [&]() {
                  TArray<TSharedPtr<FJsonValue>> JsonArray;
                  ParseJsonArray(JsonString, JsonArray)
                      ? (Object->SetArrayField(FieldName, JsonArray), void())
                      : ((!bOmitWhenEmpty || !JsonString.IsEmpty())
                             ? (Object->SetStringField(FieldName, JsonString),
                                void())
                             : void());
                }();
        }();
}

/**
 * Wraps a JSON object as an agent-state payload.
 * User Story: As state-bridging code, I need JSON objects converted into
 * `FAgentState` values so state can move through strongly typed APIs.
 */
inline FAgentState StateFromObject(const TSharedPtr<FJsonObject> &Object) {
  return TypeFactory::AgentState(StringifyObject(Object));
}

/**
 * Reads a named JSON field and wraps it as an agent-state payload.
 * User Story: As state-bridging code, I need nested state fields lifted into
 * `FAgentState` values so reducers can reuse them without manual parsing.
 */
inline FAgentState StateFromField(const TSharedPtr<FJsonObject> &Object,
                                  const FString &FieldName) {
  return TypeFactory::AgentState(JsonStringFromField(Object, FieldName));
}

/**
 * Parses stored agent-state JSON back into an object.
 * User Story: As state inspection flows, I need `FAgentState` decoded back to
 * JSON objects so nested fields can be inspected and transformed.
 */
inline TSharedPtr<FJsonObject> StateToObject(const FAgentState &State) {
  return ParseJsonObjectOrEmpty(State.JsonData);
}

/**
 * Serializes an agent action into the API object shape.
 * User Story: As protocol request builders, I need agent actions encoded into
 * the API schema so remote validation and processing can consume them.
 */
inline TSharedRef<FJsonObject> ActionToObject(const FAgentAction &Action) {
  const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
  return (detail::SetIfNonEmpty(Object, TEXT("type"), Action.Type),
          detail::SetIfNonEmpty(Object, TEXT("target"), Action.Target),
          detail::SetIfNonEmpty(Object, TEXT("reason"), Action.Reason),
          (!FMath::IsNearlyEqual(Action.Confidence, 1.0f) ||
           !Action.Type.IsEmpty())
              ? (Object->SetNumberField(TEXT("confidence"), Action.Confidence),
                 void())
              : void(),
          detail::SetIfNonEmpty(Object, TEXT("signature"), Action.Signature),
          SetFieldFromJsonString(Object, TEXT("payload"), Action.PayloadJson),
          Object);
}

/**
 * Extracts a string field with a primary name, falling back to an alias.
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
inline FString ExtractWithAlias(const TSharedPtr<FJsonObject> &Object,
                                const FString &Primary,
                                const FString &Alias) {
  const FString PrimaryValue = OptionalStringFromField(Object, Primary);
  return !PrimaryValue.IsEmpty() ? PrimaryValue
                                 : OptionalStringFromField(Object, Alias);
}

/**
 * Deserializes an API action object, including legacy field aliases.
 * User Story: As protocol response readers, I need action payloads normalized
 * so legacy and current server fields map into one runtime shape.
 */
inline FAgentAction ActionFromObject(const TSharedPtr<FJsonObject> &Object) {
  FAgentAction Action;
  return !Object.IsValid()
             ? Action
             : (Action.Type =
                    ExtractWithAlias(Object, TEXT("gaType"), TEXT("type")),
                Action.Target =
                    ExtractWithAlias(Object, TEXT("actionTarget"), TEXT("target")),
                Action.Reason =
                    ExtractWithAlias(Object, TEXT("actionReason"), TEXT("reason")),
                Action.Confidence = detail::TryGetNumberAs<float>(
                    Object, TEXT("confidence"), Action.Confidence),
                Action.Signature =
                    OptionalStringFromField(Object, TEXT("signature")),
                Action.PayloadJson =
                    JsonStringFromField(Object, TEXT("payload"), TEXT("{}")),
                Action);
}

/**
 * Serializes a memory-store instruction into API JSON.
 * User Story: As memory request builders, I need instructions encoded into the
 * API schema so remote memory writes carry the expected fields.
 */
inline TSharedRef<FJsonObject>
MemoryStoreInstructionToObject(const FMemoryStoreInstruction &Instruction) {
  const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
  return (Object->SetStringField(TEXT("text"), Instruction.Text),
          Object->SetStringField(TEXT("type"), Instruction.Type),
          Object->SetNumberField(TEXT("importance"), Instruction.Importance),
          Object);
}

/**
 * Deserializes a memory-store instruction from API JSON.
 * User Story: As memory response readers, I need store instructions decoded so
 * runtime flows can inspect or replay persisted write intents.
 */
inline FMemoryStoreInstruction
MemoryStoreInstructionFromObject(const TSharedPtr<FJsonObject> &Object) {
  FMemoryStoreInstruction Instruction;
  return !Object.IsValid()
             ? Instruction
             : (Instruction.Text = Object->GetStringField(TEXT("text")),
                Instruction.Type = Object->GetStringField(TEXT("type")),
                Instruction.Importance = detail::TryGetNumberAs<float>(
                    Object, TEXT("importance"), Instruction.Importance),
                Instruction);
}

/**
 * Serializes a recalled memory record into API JSON.
 * User Story: As recall payload builders, I need recalled memories encoded so
 * context assembly can include structured memory data.
 */
inline TSharedRef<FJsonObject>
RecalledMemoryToObject(const FRecalledMemory &Memory) {
  const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
  return (Object->SetStringField(TEXT("text"), Memory.Text),
          Object->SetStringField(TEXT("type"), Memory.Type),
          Object->SetNumberField(TEXT("importance"), Memory.Importance),
          Object->SetNumberField(TEXT("similarity"), Memory.Similarity),
          Object);
}

/**
 * Deserializes a recalled memory record from API JSON.
 * User Story: As recall response readers, I need recalled memories decoded so
 * prompt builders can consume similarity-ranked memory data.
 */
inline FRecalledMemory
RecalledMemoryFromObject(const TSharedPtr<FJsonObject> &Object) {
  FRecalledMemory Memory;
  return !Object.IsValid()
             ? Memory
             : (Memory.Text = Object->GetStringField(TEXT("text")),
                Memory.Type = Object->GetStringField(TEXT("type")),
                Memory.Importance = detail::TryGetNumberAs<float>(
                    Object, TEXT("importance"), Memory.Importance),
                Memory.Similarity = detail::TryGetNumberAs<float>(
                    Object, TEXT("similarity"), Memory.Similarity),
                Memory);
}

/**
 * Serializes a stored memory item into API JSON.
 * User Story: As memory persistence flows, I need memory items encoded so
 * local and remote stores can share one interchange format.
 */
inline TSharedRef<FJsonObject> MemoryItemToObject(const FMemoryItem &Memory) {
  const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
  return (detail::SetIfNonEmpty(Object, TEXT("id"), Memory.Id),
          Object->SetStringField(TEXT("text"), Memory.Text),
          Object->SetStringField(TEXT("type"), Memory.Type),
          Object->SetNumberField(TEXT("importance"), Memory.Importance),
          Object->SetNumberField(TEXT("similarity"), Memory.Similarity),
          Object->SetNumberField(TEXT("timestamp"),
                                 static_cast<double>(Memory.Timestamp)),
          Object);
}

/**
 * Deserializes a stored memory item from API JSON.
 * User Story: As memory readers, I need stored items decoded so runtime state
 * can rebuild memory entities from transport payloads.
 */
inline FMemoryItem MemoryItemFromObject(const TSharedPtr<FJsonObject> &Object) {
  FMemoryItem Memory;
  return !Object.IsValid()
             ? Memory
             : (Memory.Id = Object->GetStringField(TEXT("id")),
                Memory.Text = Object->GetStringField(TEXT("text")),
                Memory.Type = Object->GetStringField(TEXT("type")),
                Memory.Importance = detail::TryGetNumberAs<float>(
                    Object, TEXT("importance"), Memory.Importance),
                Memory.Similarity = detail::TryGetNumberAs<float>(
                    Object, TEXT("similarity"), Memory.Similarity),
                Memory.Timestamp = detail::TryGetNumberAs<int64>(
                    Object, TEXT("timestamp"),
                    static_cast<int64>(Memory.Timestamp)),
                Memory);
}

/**
 * Serializes cortex generation config into API JSON.
 * User Story: As cortex request builders, I need generation config encoded so
 * remote and local completion requests share the same config shape.
 */
inline TSharedRef<FJsonObject>
CortexConfigToObject(const FCortexConfig &Config) {
  const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
  return (detail::SetIfNonEmpty(Object, TEXT("model"), Config.Model),
          Object->SetBoolField(TEXT("useGPU"), Config.UseGPU),
          Object->SetNumberField(TEXT("maxTokens"), Config.MaxTokens),
          Object->SetNumberField(TEXT("temperature"), Config.Temperature),
          Object->SetNumberField(TEXT("topK"), Config.TopK),
          Object->SetNumberField(TEXT("topP"), Config.TopP),
          Config.Stop.Num() > 0
              ? [&]() {
                  TArray<TSharedPtr<FJsonValue>> StopValues;
                  StopValues.Reserve(Config.Stop.Num());
                  detail::BuildStopValuesRecursive(Config.Stop, StopValues, 0);
                  Object->SetArrayField(TEXT("stop"), StopValues);
                }()
              : void(),
          SetFieldFromJsonString(Object, TEXT("jsonSchema"),
                                 Config.JsonSchemaJson),
          Object);
}

/**
 * Deserializes cortex generation config from API JSON.
 * User Story: As cortex response readers, I need generation config decoded so
 * returned settings can be reapplied or inspected in runtime state.
 */
inline FCortexConfig
CortexConfigFromObject(const TSharedPtr<FJsonObject> &Object) {
  FCortexConfig Config;
  return !Object.IsValid()
             ? Config
             : (Config.Model =
                    OptionalStringFromField(Object, TEXT("model")),
                Config.UseGPU = detail::TryGetBoolAs(Object, TEXT("useGPU"),
                                                     Config.UseGPU),
                Config.MaxTokens = detail::TryGetNumberAs<int32>(
                    Object, TEXT("maxTokens"), Config.MaxTokens),
                Config.Temperature = detail::TryGetNumberAs<float>(
                    Object, TEXT("temperature"), Config.Temperature),
                Config.TopK = detail::TryGetNumberAs<int32>(
                    Object, TEXT("topK"), Config.TopK),
                Config.TopP = detail::TryGetNumberAs<float>(
                    Object, TEXT("topP"), Config.TopP),
                [&]() {
                  const TArray<TSharedPtr<FJsonValue>> *StopValues = nullptr;
                  (Object->TryGetArrayField(TEXT("stop"), StopValues) &&
                   StopValues)
                      ? (Config.Stop.Empty(StopValues->Num()),
                         detail::ExtractStopValuesRecursive(
                             *StopValues, Config.Stop, 0),
                         void())
                      : void();
                }(),
                Config.JsonSchemaJson = JsonStringFromField(
                    Object, TEXT("jsonSchema"), TEXT("")),
                Config);
}

/**
 * Serializes bridge validation context payloads into API JSON.
 * User Story: As bridge validation requests, I need context encoded so the
 * validator can inspect NPC, world, and constraint state together.
 */
inline TSharedRef<FJsonObject>
ValidationContextToObject(const FBridgeValidationContext &Context) {
  const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
  return (SetFieldFromJsonString(Object, TEXT("npcState"),
                                 Context.NpcStateJson),
          SetFieldFromJsonString(Object, TEXT("worldState"),
                                 Context.WorldStateJson),
          SetFieldFromJsonString(Object, TEXT("constraints"),
                                 Context.ConstraintsJson),
          Object);
}

/**
 * Deserializes a bridge validation response into SDK result state.
 * User Story: As bridge validation readers, I need response payloads decoded
 * so runtime code can surface corrected actions and rejection reasons.
 */
inline FValidationResult
ValidationResultFromObject(const TSharedPtr<FJsonObject> &Object) {
  FValidationResult Result;
  return !Object.IsValid()
             ? (Result.bValid = false,
                Result.Reason = TEXT("Invalid validation response"), Result)
             : (Result.bValid = detail::TryGetBoolAs(Object, TEXT("valid"),
                                                     Result.bValid),
                Result.Reason =
                    OptionalStringFromField(Object, TEXT("reason")),
                Object->HasTypedField<EJson::Object>(TEXT("correctedAction"))
                    ? (Result.CorrectedAction = ActionFromObject(
                           Object->GetObjectField(TEXT("correctedAction"))),
                       void())
                    : void(),
                Result);
}

/**
 * Deserializes an Arweave upload instruction from API JSON.
 * User Story: As soul export flows, I need upload instructions decoded so the
 * client can perform the correct signed upload handshake.
 */
inline FArweaveUploadInstruction
UploadInstructionFromObject(const TSharedPtr<FJsonObject> &Object) {
  FArweaveUploadInstruction Instruction;
  return !Object.IsValid()
             ? Instruction
             : (Instruction.UploadUrl = OptionalStringFromField(
                    Object, TEXT("auiEndpoint")),
                Instruction.ContentType = OptionalStringFromField(
                    Object, TEXT("auiContentType")),
                Instruction.AuiAuthHeader = OptionalStringFromField(
                    Object, TEXT("auiAuthHeader")),
                Instruction.PayloadJson = JsonStringFromField(
                    Object, TEXT("auiPayload"), TEXT("{}")),
                Instruction);
}

/**
 * Serializes an Arweave upload result into API JSON.
 * User Story: As soul export confirmation flows, I need upload results encoded
 * so the server can verify what happened during the upload step.
 */
inline TSharedRef<FJsonObject>
UploadResultToObject(const FArweaveUploadResult &Result) {
  const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
  return (Object->SetStringField(TEXT("aurTxId"), Result.TxId),
          Object->SetNumberField(
              TEXT("aurStatus"),
              Result.StatusCode > 0 ? Result.StatusCode
                                    : FCString::Atoi(*Result.Status)),
          Object->SetBoolField(TEXT("aurSuccess"), Result.bSuccess),
          detail::SetIfNonEmpty(Object, TEXT("aurError"), Result.Error),
          Object);
}

/**
 * Deserializes an Arweave download instruction from API JSON.
 * User Story: As soul import flows, I need download instructions decoded so
 * the client can fetch the expected transaction from the right gateway.
 */
inline FArweaveDownloadInstruction
DownloadInstructionFromObject(const TSharedPtr<FJsonObject> &Object) {
  FArweaveDownloadInstruction Instruction;
  return !Object.IsValid()
             ? Instruction
             : (Instruction.GatewayUrl = OptionalStringFromField(
                    Object, TEXT("adiGatewayUrl")),
                Instruction.TxId = OptionalStringFromField(
                    Object, TEXT("adiExpectedTxId")),
                Instruction.ExpectedTxId = Instruction.TxId, Instruction);
}

/**
 * Serializes an Arweave download result into API JSON.
 * User Story: As soul import confirmation flows, I need download results
 * encoded so later steps can validate the fetched payload.
 */
inline TSharedRef<FJsonObject>
DownloadResultToObject(const FArweaveDownloadResult &Result) {
  const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
  return (SetFieldFromJsonString(
              Object, TEXT("adrBody"),
              !Result.BodyJson.IsEmpty() ? Result.BodyJson : Result.Payload,
              false),
          Object->SetNumberField(TEXT("adrStatus"), Result.StatusCode),
          Object->SetBoolField(TEXT("adrSuccess"), Result.bSuccess),
          detail::SetIfNonEmpty(Object, TEXT("adrError"), Result.Error),
          Object);
}

/**
 * Recursively extracts memory items from a JSON value array.
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
namespace detail {
inline void ExtractMemoryItemsRecursive(
    const TArray<TSharedPtr<FJsonValue>> &Source, TArray<FMemoryItem> &Out,
    int32 Index) {
  Index < Source.Num()
      ? ((Source[Index].IsValid() && Source[Index]->Type == EJson::Object)
             ? (Out.Add(MemoryItemFromObject(Source[Index]->AsObject())), void())
             : void(),
         ExtractMemoryItemsRecursive(Source, Out, Index + 1), void())
      : void();
}
} // namespace detail

/**
 * Deserializes a soul payload from API JSON.
 * User Story: As soul import and listing flows, I need souls decoded so NPC
 * state and memories can be reconstructed from transport payloads.
 */
inline FSoul SoulFromObject(const TSharedPtr<FJsonObject> &Object) {
  FSoul Soul;
  return !Object.IsValid()
             ? Soul
             : (Soul.Id = OptionalStringFromField(Object, TEXT("id")),
                Soul.Version =
                    OptionalStringFromField(Object, TEXT("version")),
                Soul.Name = OptionalStringFromField(Object, TEXT("name")),
                Soul.Persona =
                    OptionalStringFromField(Object, TEXT("persona")),
                Soul.Signature =
                    OptionalStringFromField(Object, TEXT("signature")),
                Soul.State = StateFromField(Object, TEXT("state")),
                [&]() {
                  const TArray<TSharedPtr<FJsonValue>> *MemoryValues = nullptr;
                  (Object->TryGetArrayField(TEXT("memories"), MemoryValues) &&
                   MemoryValues)
                      ? (Soul.Memories.Empty(MemoryValues->Num()),
                         detail::ExtractMemoryItemsRecursive(
                             *MemoryValues, Soul.Memories, 0),
                         void())
                      : void();
                }(),
                Soul);
}

/**
 * Deserializes an imported-NPC payload from API JSON.
 * User Story: As soul import flows, I need imported NPC results decoded so the
 * terminal and runtime can inspect the created NPC record.
 */
inline FImportedNpc
ImportedNpcFromObject(const TSharedPtr<FJsonObject> &Object) {
  FImportedNpc Npc;
  return !Object.IsValid()
             ? Npc
             : (Npc.NpcId =
                    OptionalStringFromField(Object, TEXT("npcId")),
                Npc.Persona =
                    OptionalStringFromField(Object, TEXT("persona")),
                Npc.DataJson = JsonStringFromField(Object, TEXT("data")), Npc);
}

} // namespace JsonInterop
