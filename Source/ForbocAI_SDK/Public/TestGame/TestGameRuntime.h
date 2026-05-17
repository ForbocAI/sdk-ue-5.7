#pragma once
/**
 * Test-game runtime URL helpers.
 *
 * This header is intentionally narrow: it resolves the runtime URL used by
 * the test-game harness to reach the API. It is NOT an executor boundary
 * and contains no in-process command execution. All test-game command
 * execution must flow through TestGame::CommandSurface (defined in
 * TestGame/TestGameCommandSurface.h), which delegates to the canonical
 * CLIOps command surface.
 *
 * Background: this file was split out of the legacy TestGameLib.h, which
 * previously combined runtime URL helpers, ASCII grid rendering, and an
 * in-process command executor on one surface. The executor was retired
 * in favor of the first-class commandlet/command-surface boundary; the
 * remaining pure helpers were split into TestGameRuntime.h and
 * TestGameGridRender.h so test-game code can include only what it needs
 * without re-importing executor-shaped concerns.
 *
 * User Story: As the UE test-game harness, I need a narrow runtime-URL
 * resolution surface so the harness can decide which API host to call
 * without including the legacy TestGameLib in-process executor surface.
 */

#include "Core/AsyncHttp.h"
#include "CLI/CliOperations.h"
#include "CoreMinimal.h"
#include "Core/functional_core.hpp"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include <exception>

namespace TestGame {

using FRuntimeConnectivityProbe = TFunction<bool(const FString &)>;

/**
 * Checks runtime connectivity to a given status URL.
 * Returns true if the endpoint responds with JSON containing a status field
 * within 1.5s.
 * User Story: As runtime fallback selection, I need a connectivity probe so
 * the test game can decide which runtime URL is available.
 */
namespace detail {
inline bool HasStatusJsonField(const FString &Body) {
  TSharedPtr<FJsonObject> JsonObject;
  TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
  return FJsonSerializer::Deserialize(Reader, JsonObject) &&
         JsonObject.IsValid() && JsonObject->HasTypedField<EJson::String>(
                                     TEXT("status"));
}
} // namespace detail

inline bool CheckRuntimeConnectivity(
    const FString &Url = TEXT("http://localhost:8080/status")) {
  try {
    const func::HttpResult<FString> Result = Ops::WaitForResult(
        func::AsyncHttp::Get<FString>(Url), 1.5);
    return Result.bSuccess && Result.ResponseCode == 200 &&
           detail::HasStatusJsonField(Result.data);
  } catch (const std::exception &) {
    return false;
  }
}

/**
 * Resolves the explicitly configured runtime URL without probing.
 * Prefers FORBOC_RUNTIME_URL, then falls back to FORBOCAI_API_URL.
 * User Story: As the no-fallback runtime policy, I need consumers to set the
 * runtime URL explicitly so production hosts never silently downgrade to
 * localhost or an unintended remote.
 */
inline FString ResolveConfiguredRuntimeUrl(const FString &RuntimeUrlOverride,
                                           const FString &ApiUrlOverride) {
  return !RuntimeUrlOverride.IsEmpty() ? RuntimeUrlOverride : ApiUrlOverride;
}

inline FString ResolveRuntimeUrlFromEnv() {
  return ResolveConfiguredRuntimeUrl(
      FPlatformMisc::GetEnvironmentVariable(TEXT("FORBOC_RUNTIME_URL")),
      FPlatformMisc::GetEnvironmentVariable(TEXT("FORBOCAI_API_URL")));
}

/**
 * Test-harness probe-based resolver. Verification-only.
 * User Story: As the test-game harness, I need a resolver that probes
 * localhost first and a published API second so verification runs locally
 * during development and remotely in CI without per-developer config.
 *
 * Production code paths must NOT call this. Use ResolveRuntimeUrl
 * (or set FORBOC_RUNTIME_URL / FORBOCAI_API_URL explicitly) so the
 * no-fallback policy holds.
 */
inline FString ResolveVerificationRuntimeUrl(
    const FRuntimeConnectivityProbe &Probe) {
  return Probe(TEXT("http://localhost:8080/status"))
             ? FString(TEXT("http://localhost:8080"))
             : (Probe(TEXT("https://api.forboc.ai/status"))
                    ? FString(TEXT("https://api.forboc.ai"))
                    : FString());
}

/**
 * Default test-harness resolver. Prefers explicit runtime configuration; falls
 * back to the probe-based path only if no env var is configured.
 * User Story: As a test runner, I need explicit env-var configuration to take
 * precedence so CI can pin the runtime without relying on probe order.
 */
inline FString ResolveVerificationRuntimeUrl() {
  const FString FromEnv = ResolveRuntimeUrlFromEnv();
  return !FromEnv.IsEmpty()
             ? FromEnv
             : ResolveVerificationRuntimeUrl(
                    [](const FString &Url) {
                      return CheckRuntimeConnectivity(Url);
                    });
}

/**
 * Production/runtime resolver. Explicit configuration only.
 * User Story: As a production runtime caller, I need hard-fail behavior when
 * the runtime URL is unset so verification-only probe fallback never leaks
 * into shipped entrypoints.
 */
inline FString ResolveRuntimeUrl() { return ResolveRuntimeUrlFromEnv(); }

} // namespace TestGame
