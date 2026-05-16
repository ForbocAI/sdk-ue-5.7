#pragma once

#include "Core/functional_core.hpp"
#include "CoreMinimal.h"

namespace Errors {

/**
 * G9: Consistent error shape — all SDK errors flow through this type
 * User Story: As a maintainer, I need this implementation note so I can understand which milestone behavior the surrounding code is preserving.
 */

enum class EErrorCategory : uint8 {
  Network,    // Connection failed, timeout, DNS
  Http,       // Non-2xx response
  Validation, // Missing required field, invalid input
  Protocol,   // Protocol loop errors (max turns, unknown instruction)
  Memory,     // Vector DB / embedding errors
  Cortex,     // SLM inference errors
  Arweave,    // Soul upload/download errors
  Config,     // Configuration errors (missing key, invalid URL)
  Unknown     // Unclassified
};

struct FSDKError {
  EErrorCategory Category;
  FString Code;
  FString Message;
  int32 StatusCode; // HTTP status code (0 for non-HTTP errors)

  FSDKError()
      : Category(EErrorCategory::Unknown), Code(TEXT("UNKNOWN")),
        Message(TEXT("")), StatusCode(0) {}
};

/**
 * Builds a normalized network error.
 * User Story: As SDK error consumers, I need transport failures normalized so
 * retry and UI flows can branch on a stable category.
 */
inline FSDKError NetworkError(const FString &Message) {
  FSDKError E;
  E.Category = EErrorCategory::Network;
  E.Code = TEXT("NETWORK_ERROR");
  E.Message = Message;
  return E;
}

/**
 * Builds a normalized HTTP error with status code metadata.
 * User Story: As API callers, I need HTTP failures normalized with status
 * codes so handlers can map them to the right recovery path.
 */
inline FSDKError HttpError(int32 StatusCode, const FString &Message) {
  FSDKError E;
  E.Category = EErrorCategory::Http;
  E.Code = FString::Printf(TEXT("HTTP_%d"), StatusCode);
  E.Message = Message;
  E.StatusCode = StatusCode;
  return E;
}

/**
 * Builds a normalized validation error.
 * User Story: As input-validation flows, I need invalid requests surfaced in a
 * shared shape so user-facing feedback stays consistent.
 */
inline FSDKError ValidationError(const FString &Message) {
  FSDKError E;
  E.Category = EErrorCategory::Validation;
  E.Code = TEXT("VALIDATION_ERROR");
  E.Message = Message;
  return E;
}

/**
 * Builds a normalized protocol error.
 * User Story: As protocol orchestration, I need protocol-loop failures
 * normalized so the turn runner can stop cleanly and report the cause.
 */
inline FSDKError ProtocolError(const FString &Message) {
  FSDKError E;
  E.Category = EErrorCategory::Protocol;
  E.Code = TEXT("PROTOCOL_ERROR");
  E.Message = Message;
  return E;
}

/**
 * Builds a normalized memory error.
 * User Story: As memory runtime consumers, I need storage and recall failures
 * normalized so diagnostics stay consistent across backends.
 */
inline FSDKError MemoryError(const FString &Message) {
  FSDKError E;
  E.Category = EErrorCategory::Memory;
  E.Code = TEXT("MEMORY_ERROR");
  E.Message = Message;
  return E;
}

/**
 * Builds a normalized cortex error.
 * User Story: As cortex runtime consumers, I need inference failures
 * normalized so callers can handle model issues without ad hoc parsing.
 */
inline FSDKError CortexError(const FString &Message) {
  FSDKError E;
  E.Category = EErrorCategory::Cortex;
  E.Code = TEXT("CORTEX_ERROR");
  E.Message = Message;
  return E;
}

/**
 * Builds a normalized Arweave error.
 * User Story: As soul import/export flows, I need upload and download failures
 * normalized so recovery guidance is consistent.
 */
inline FSDKError ArweaveError(const FString &Message) {
  FSDKError E;
  E.Category = EErrorCategory::Arweave;
  E.Code = TEXT("ARWEAVE_ERROR");
  E.Message = Message;
  return E;
}

/**
 * Builds a normalized config error.
 * User Story: As environment and config setup flows, I need configuration
 * mistakes normalized so setup guidance can be shown consistently.
 */
inline FSDKError ConfigError(const FString &Message) {
  FSDKError E;
  E.Category = EErrorCategory::Config;
  E.Code = TEXT("CONFIG_ERROR");
  E.Message = Message;
  return E;
}

/**
 * Classifies a raw thunk or transport error string into the SDK error shape.
 * User Story: As thunk callers, I need raw failure text classified so UI and
 * retry logic can work from categories instead of string matching everywhere.
 */
inline FSDKError classifyError(const FString &RawError) {
  std::vector<func::MatchCase<FString, FSDKError>> cases;

  /**
   * Network errors
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  cases.push_back(func::when<FString, FSDKError>(
      [](const FString &E) { return E.Contains(TEXT("network_error")); },
      [](const FString &E) { return NetworkError(E); }));
  cases.push_back(func::when<FString, FSDKError>(
      [](const FString &E) { return E.Contains(TEXT("request_failed")); },
      [](const FString &E) { return NetworkError(E); }));
  cases.push_back(func::when<FString, FSDKError>(
      [](const FString &E) { return E.Contains(TEXT("timeout")); },
      [](const FString &E) { return NetworkError(E); }));

  /**
   * HTTP errors (status code in message)
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  cases.push_back(func::when<FString, FSDKError>(
      [](const FString &E) { return E.Contains(TEXT("status_4")); },
      [](const FString &E) { return HttpError(400, E); }));
  cases.push_back(func::when<FString, FSDKError>(
      [](const FString &E) { return E.Contains(TEXT("status_5")); },
      [](const FString &E) { return HttpError(500, E); }));

  /**
   * Arweave errors
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  cases.push_back(func::when<FString, FSDKError>(
      [](const FString &E) {
        return E.Contains(TEXT("Arweave")) || E.Contains(TEXT("upload")) ||
               E.Contains(TEXT("download"));
      },
      [](const FString &E) { return ArweaveError(E); }));

  /**
   * Protocol errors
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  cases.push_back(func::when<FString, FSDKError>(
      [](const FString &E) {
        return E.Contains(TEXT("Protocol")) || E.Contains(TEXT("max turns")) ||
               E.Contains(TEXT("instruction type"));
      },
      [](const FString &E) { return ProtocolError(E); }));

  /**
   * Memory errors
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  cases.push_back(func::when<FString, FSDKError>(
      [](const FString &E) {
        return E.Contains(TEXT("memory")) || E.Contains(TEXT("Memory"));
      },
      [](const FString &E) { return MemoryError(E); }));

  /**
   * Cortex errors
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  cases.push_back(func::when<FString, FSDKError>(
      [](const FString &E) {
        return E.Contains(TEXT("cortex")) || E.Contains(TEXT("Cortex")) ||
               E.Contains(TEXT("inference"));
      },
      [](const FString &E) { return CortexError(E); }));

  /**
   * Config/validation errors
   * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
   */
  cases.push_back(func::when<FString, FSDKError>(
      [](const FString &E) {
        return E.Contains(TEXT("API key")) || E.Contains(TEXT("apiKey")) ||
               E.Contains(TEXT("Missing"));
      },
      [](const FString &E) { return ConfigError(E); }));

  /**
   * Wildcard default
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  cases.push_back(func::when<FString, FSDKError>(
      func::wildcard<FString>(), [](const FString &E) {
        FSDKError Err;
        Err.Category = EErrorCategory::Unknown;
        Err.Code = TEXT("UNKNOWN");
        Err.Message = E;
        return Err;
      }));

  return func::multi_match<FString, FSDKError>(RawError, cases)
      .value; // wildcard guarantees a match
}

/**
 * Returns a non-empty thunk error message for FString-based failures.
 * User Story: As thunk adapters, I need a guaranteed message string so CLI and
 * UI surfaces never render empty failure text.
 */
inline FString extractThunkErrorMessage(const FString &Message,
                                        const FString &Fallback =
                                            TEXT("Request failed")) {
  return Message.IsEmpty() ? Fallback : Message;
}

/**
 * Returns a non-empty thunk error message for std::string-based failures.
 * User Story: As thunk adapters, I need a guaranteed message string even for
 * native std::string errors so reporting stays consistent across layers.
 */
inline FString extractThunkErrorMessage(const std::string &Message,
                                        const FString &Fallback =
                                            TEXT("Request failed")) {
  return Message.empty() ? Fallback : FString(UTF8_TO_TCHAR(Message.c_str()));
}

/**
 * Returns guidance when production API calls are missing an API key.
 * User Story: As SDK setup flows, I need targeted guidance for missing keys so
 * production calls fail with actionable remediation instead of a dead end.
 */
inline func::Maybe<FString>
requireApiKeyGuidance(const FString &ApiUrl, const FString &ApiKey) {
  return (ApiKey.IsEmpty() && ApiUrl.Contains(TEXT("api.forboc.ai")))
             ? func::just<FString>(
                   TEXT("Missing API key. Set FORBOCAI_API_KEY (or run `forboc "
                        "config set apiKey <key>`) for production API calls."))
             : func::nothing<FString>();
}

} // namespace Errors
