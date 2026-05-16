#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "rtk.hpp"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "functional_core.hpp"

namespace func {
namespace AsyncHttp {
namespace detail {

template <typename T, typename Enable = void> struct JsonDeserializer;

template <typename JsonValueT>
bool DeserializeStringArrayRecursive(
    const TArray<TSharedPtr<JsonValueT>> &JsonValues, int32 Index,
    TArray<FString> &OutValue) {
  return Index == JsonValues.Num()
             ? true
             : !JsonValues[Index].IsValid()
                   ? false
                   : (OutValue.Add(JsonValues[Index]->AsString()),
                      DeserializeStringArrayRecursive(JsonValues, Index + 1,
                                                     OutValue));
}

template <typename T, typename JsonValueT>
bool DeserializeStructArrayRecursive(
    const TArray<TSharedPtr<JsonValueT>> &JsonValues, int32 Index,
    TArray<T> &OutValue) {
  const TSharedPtr<FJsonObject> JsonObject =
      Index == JsonValues.Num()
          ? TSharedPtr<FJsonObject>()
          : (JsonValues[Index].IsValid() ? JsonValues[Index]->AsObject()
                                         : TSharedPtr<FJsonObject>());
  T Item;
  return Index == JsonValues.Num()
             ? true
             : !JsonValues[Index].IsValid()
                   ? false
                   : !JsonObject.IsValid()
                         ? false
                         : !FJsonObjectConverter::JsonObjectToUStruct(
                               JsonObject.ToSharedRef(), T::StaticStruct(),
                               &Item, 0, 0)
                               ? false
                               : (OutValue.Add(Item),
                                  DeserializeStructArrayRecursive<T>(
                                      JsonValues, Index + 1, OutValue));
}

template <typename CallbackT>
void NotifyCallbacksRecursive(const std::vector<CallbackT> &Callbacks,
                              size_t Index, const FString &Body,
                              HttpStatusCode Code) {
  Index == Callbacks.size()
      ? void()
      : (Callbacks[Index](Body, Code),
         NotifyCallbacksRecursive(Callbacks, Index + 1, Body, Code));
}

inline bool IsWriteVerb(const FString &Verb) {
  return Verb == TEXT("POST") || Verb == TEXT("PUT") || Verb == TEXT("PATCH");
}

inline bool ApplyPayload(IHttpRequest &Request, const FString &Verb,
                         const FString &Payload) {
  return IsWriteVerb(Verb)
             ? (Request.SetHeader(TEXT("Content-Type"),
                                  TEXT("application/json; charset=utf-8")),
                Request.SetContentAsString(Payload), true)
             : true;
}

inline bool ApplyAuthorization(IHttpRequest &Request, const FString &ApiKey) {
  return ApiKey.IsEmpty()
             ? true
             : (Request.SetHeader(TEXT("Authorization"),
                                  FString::Printf(TEXT("Bearer %s"), *ApiKey)),
                true);
}

template <typename T>
func::HttpResult<T> DecodeContentResult(const FString &Content,
                                        HttpStatusCode Code) {
  T ResultPayload;
  return Content.IsEmpty()
             ? func::HttpResult<T>::Success(ResultPayload, Code)
             : JsonDeserializer<T>::Deserialize(Content, ResultPayload)
                   ? func::HttpResult<T>::Success(ResultPayload, Code)
                   : func::HttpResult<T>::Failure(
                         "JSON deserialization failed", Code);
}

template <typename T>
void ResolveRequestCompletion(
    const std::function<void(func::HttpResult<T>)> &Resolve,
    FHttpResponsePtr Res, bool bWasSuccessful) {
  const HttpStatusCode Code = Res.IsValid() ? Res->GetResponseCode() : 0;
  const FString Content = Res.IsValid() ? Res->GetContentAsString() : TEXT("");

  Resolve((!bWasSuccessful || !Res.IsValid())
              ? func::HttpResult<T>::Failure("Network failure", 0)
              : (Code < 200 || Code >= 300)
                    ? func::HttpResult<T>::Failure(TCHAR_TO_UTF8(*Content),
                                                   Code)
                    : DecodeContentResult<T>(Content, Code));
}

/**
 * Deserializes an HTTP payload into a UStruct-backed value.
 * User Story: As generic HTTP decoding, I need a default deserializer so API
 * responses can hydrate SDK structs without endpoint-specific code.
 */
template <typename T, typename Enable> struct JsonDeserializer {
  static bool Deserialize(const FString &Content, T &OutValue) {
    return FJsonObjectConverter::JsonObjectStringToUStruct(Content, &OutValue, 0,
                                                           0);
  }
};

/**
 * Deserializes an HTTP payload into a plain string.
 * User Story: As raw-response consumers, I need string passthrough decoding so
 * endpoints can return text bodies without JSON struct conversion.
 */
template <> struct JsonDeserializer<FString, void> {
  static bool Deserialize(const FString &Content, FString &OutValue) {
    OutValue = Content;
    return true;
  }
};

/**
 * Deserializes an empty HTTP payload.
 * User Story: As endpoints with no response body, I need empty-payload support
 * so success responses can still resolve cleanly.
 */
template <> struct JsonDeserializer<rtk::FEmptyPayload, void> {
  static bool Deserialize(const FString &Content,
                          rtk::FEmptyPayload &OutValue) {
    return true;
  }
};

/**
 * Deserializes an HTTP payload into an array of strings.
 * User Story: As list endpoints returning raw strings, I need array decoding so
 * preset ids and similar payloads can be consumed directly.
 */
template <> struct JsonDeserializer<TArray<FString>, void> {
  static bool Deserialize(const FString &Content, TArray<FString> &OutValue) {
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    return !FJsonSerializer::Deserialize(Reader, JsonValues)
               ? false
               : (OutValue.Empty(JsonValues.Num()),
                  DeserializeStringArrayRecursive(JsonValues, 0, OutValue));
  }
};

/**
 * Deserializes an HTTP payload into an array of UStruct-backed values.
 * User Story: As collection endpoints, I need array decoding so JSON lists can
 * hydrate SDK structs without custom per-type parsers.
 */
template <typename T> struct JsonDeserializer<TArray<T>, void> {
  static bool Deserialize(const FString &Content, TArray<T> &OutValue) {
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
    TArray<TSharedPtr<FJsonValue>> JsonValues;
    return !FJsonSerializer::Deserialize(Reader, JsonValues)
               ? false
               : (OutValue.Empty(JsonValues.Num()),
                  DeserializeStructArrayRecursive<T>(JsonValues, 0, OutValue));
  }
};

/**
 * Builds the generic HTTP request wrapper for an async call.
 * User Story: As endpoint helpers, I need one request builder so GET, POST,
 * and DELETE calls share the same network, auth, and decode path.
 */
template <typename T>
func::AsyncResult<func::HttpResult<T>>
CreateRequest(const FString &Verb, const FString &Url,
              const FString &ApiKey = TEXT(""),
              const FString &Payload = TEXT("")) {
  return func::createAsyncResult<func::HttpResult<T>>(
      [Verb, Url, ApiKey, Payload](std::function<void(func::HttpResult<T>)> Resolve,
                                   std::function<void(std::string)> Reject) {
        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
            FHttpModule::Get().CreateRequest();
        Request->SetURL(Url);
        Request->SetVerb(Verb);
        Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
        (void)Reject;
        (void)ApplyPayload(*Request, Verb, Payload);
        (void)ApplyAuthorization(*Request, ApiKey);

        Request->OnProcessRequestComplete().BindLambda(
            [Resolve](FHttpRequestPtr Req, FHttpResponsePtr Res,
                      bool bWasSuccessful) {
              (void)Req;
              ResolveRequestCompletion<T>(Resolve, Res, bWasSuccessful);
            });

        Request->ProcessRequest();
      });
}

} // namespace detail

/**
 * Builds an async POST request for a typed response.
 * User Story: As endpoint wrappers, I need a POST helper so write operations
 * can share the common request and decode path.
 */
template <typename T>
func::AsyncResult<func::HttpResult<T>>
Post(const FString &Url, const FString &Payload,
     const FString &ApiKey = TEXT("")) {
  return detail::CreateRequest<T>(TEXT("POST"), Url, ApiKey, Payload);
}

/**
 * Builds an async GET request for a typed response.
 * User Story: As endpoint wrappers, I need a GET helper so read operations can
 * share the common request and decode path.
 */
template <typename T>
func::AsyncResult<func::HttpResult<T>> Get(const FString &Url,
                                           const FString &ApiKey = TEXT("")) {
  return detail::CreateRequest<T>(TEXT("GET"), Url, ApiKey);
}

/**
 * Builds an async DELETE request for a typed response.
 * User Story: As endpoint wrappers, I need a DELETE helper so destructive
 * operations can share the common request and decode path.
 */
template <typename T>
func::AsyncResult<func::HttpResult<T>>
Delete(const FString &Url, const FString &ApiKey = TEXT("")) {
  return detail::CreateRequest<T>(TEXT("DELETE"), Url, ApiKey);
}

/**
 * G10: Request deduplication — prevents duplicate in-flight GET requests.
 * If a GET for the same URL is already in-flight, callers receive the
 * same result instead of firing a second HTTP request.
 * User Story: As a maintainer, I need this implementation note so I can understand which milestone behavior the surrounding code is preserving.
 */

namespace Dedup {

namespace detail {

struct InFlightEntry {
  std::vector<std::function<void(const FString &, HttpStatusCode)>> Callbacks;
  bool bCompleted;
  FString ResponseBody;
  HttpStatusCode StatusCode;

  InFlightEntry() : bCompleted(false), StatusCode(0) {}
};

inline TMap<FString, TSharedPtr<InFlightEntry>> &InFlightRequests() {
  static TMap<FString, TSharedPtr<InFlightEntry>> Map;
  return Map;
}

inline TSharedPtr<InFlightEntry>
GetPendingEntry(TMap<FString, TSharedPtr<InFlightEntry>> &Map,
                const FString &Key) {
  const TSharedPtr<InFlightEntry> Existing =
      Map.Contains(Key) ? Map[Key] : TSharedPtr<InFlightEntry>();
  return Existing.IsValid() && !Existing->bCompleted
             ? Existing
             : TSharedPtr<InFlightEntry>();
}

template <typename T>
func::HttpResult<T> DecodeDedupedResult(const FString &Body,
                                        HttpStatusCode Code) {
  return Code >= 200 && Code < 300
             ? DecodeContentResult<T>(Body, Code)
             : func::HttpResult<T>::Failure(TCHAR_TO_UTF8(*Body), Code);
}

template <typename T>
func::HttpResult<T> DecodeRawResult(
    const func::HttpResult<FString> &RawResult) {
  return RawResult.bSuccess
             ? DecodeContentResult<T>(RawResult.data, RawResult.ResponseCode)
             : func::HttpResult<T>::Failure(RawResult.error,
                                            RawResult.ResponseCode);
}

template <typename T>
func::AsyncResult<func::HttpResult<T>>
CreatePiggybackRequest(const TSharedPtr<InFlightEntry> &Existing) {
  return func::createAsyncResult<func::HttpResult<T>>(
      [Existing](std::function<void(func::HttpResult<T>)> Resolve,
                 std::function<void(std::string)> Reject) {
        (void)Reject;
        Existing->Callbacks.push_back(
            [Resolve](const FString &Body, HttpStatusCode Code) {
              Resolve(DecodeDedupedResult<T>(Body, Code));
            });
      });
}

template <typename T>
func::AsyncResult<func::HttpResult<T>>
CreateFreshDedupedRequest(const FString &Key, const FString &Url,
                          const FString &ApiKey) {
  TSharedPtr<InFlightEntry> Entry = MakeShared<InFlightEntry>();
  InFlightRequests().Add(Key, Entry);

  return func::AsyncChain::then<func::HttpResult<FString>, func::HttpResult<T>>(
      AsyncHttp::Get<FString>(Url, ApiKey),
      [Key, Entry](const func::HttpResult<FString> &RawResult) {
        func::HttpResult<T> DecodedResult = DecodeRawResult<T>(RawResult);
        Entry->bCompleted = true;
        Entry->ResponseBody =
            RawResult.bSuccess ? RawResult.data
                               : FString(UTF8_TO_TCHAR(RawResult.error.c_str()));
        Entry->StatusCode = RawResult.ResponseCode;
        AsyncHttp::detail::NotifyCallbacksRecursive(
            Entry->Callbacks, 0, Entry->ResponseBody, Entry->StatusCode);
        InFlightRequests().Remove(Key);
        return func::createAsyncResult<func::HttpResult<T>>(
            [DecodedResult](std::function<void(func::HttpResult<T>)> Resolve,
                            std::function<void(std::string)> Reject) {
              (void)Reject;
              Resolve(DecodedResult);
            });
      });
}

} // namespace detail

/**
 * Builds a deduplicated async GET request for a typed response.
 * User Story: As request fan-out control, I need GET deduplication so repeated
 * callers can share one in-flight network request instead of spamming the API.
 */
template <typename T>
func::AsyncResult<func::HttpResult<T>>
GetDeduped(const FString &Url, const FString &ApiKey = TEXT("")) {
  const FString Key = Url;

  auto &Map = detail::InFlightRequests();
  const TSharedPtr<detail::InFlightEntry> Existing =
      detail::GetPendingEntry(Map, Key);

  return Existing.IsValid()
             ? detail::CreatePiggybackRequest<T>(Existing)
             : detail::CreateFreshDedupedRequest<T>(Key, Url, ApiKey);
}

} // namespace Dedup

} // namespace AsyncHttp
} // namespace func
