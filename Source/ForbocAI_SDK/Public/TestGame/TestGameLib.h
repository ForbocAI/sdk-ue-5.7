#pragma once
/**
 * Test-game lib modules — mirrors the non-command UI/runtime helpers from the
 * TS test-game support layer.
 * ASCII grid rendering and runtime connectivity checks only.
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */

#include "CLI/CliOperations.h"
#include "Core/AsyncHttp.h"
#include "CoreMinimal.h"
#include "RuntimeStore.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TestGame/TestGameStore.h"
#include "TestGame/TestGameTypes.h"
#include "Core/functional_core.hpp"
#include <exception>

namespace TestGame {

/**
 * Resolves a single grid cell character for the current game state.
 * User Story: As ASCII rendering, I need one cell resolver so the grid view
 * can show blocked tiles, the player, and NPCs consistently.
 */

/**
 * Recursive helper — scans blocked positions for a match.
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
namespace detail {
inline bool IsBlocked(const TArray<FPosition> &Blocked, const FPosition &Pos,
                      int32 Index) {
  return Index >= Blocked.Num()
             ? false
             : (Blocked[Index] == Pos ? true
                                      : IsBlocked(Blocked, Pos, Index + 1));
}

/**
 * Recursive helper — scans NPCs for a position match and returns the cell char.
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
inline TCHAR NpcCellAt(const TArray<FGameNPC> &Npcs, const FPosition &Pos,
                       int32 Index) {
  return Index >= Npcs.Num()
             ? TEXT('.')
             : (Npcs[Index].Position == Pos
                    ? (Npcs[Index].Id == TEXT("miller")
                           ? TEXT('M')
                           : (Npcs[Index].Id == TEXT("doomguard") ? TEXT('D')
                                                                  : TEXT('N')))
                    : NpcCellAt(Npcs, Pos, Index + 1));
}
} // namespace detail

inline TCHAR CellAt(const FPosition &Pos, const FTestGameState &State) {
  /**
   * Check blocked
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  return detail::IsBlocked(State.Grid.Blocked, Pos, 0)
             ? TEXT('#')
             /**
              * Check player
              * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
              */
             : (State.Player.Position == Pos
                    ? TEXT('P')
                    /**
                     * Check NPCs
                     * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
                     */
                    : detail::NpcCellAt(
                          GetNPCAdapter().getSelectors().selectAll(
                              State.NPCs.Entities),
                          Pos, 0));
}

/**
 * Renders the whole tile grid as ASCII rows.
 * User Story: As terminal rendering, I need the full grid rendered so scenario
 * output can show the current world state in text form.
 */
namespace detail {
inline FString RenderRow(const FTestGameState &State, int32 X, int32 Width,
                         const FString &Acc) {
  return X >= Width
             ? Acc
             : RenderRow(State, X + 1, Width,
                         Acc + (X > 0 ? FString(TEXT(" ")) : FString()) +
                             CellAt(FPosition(X, (int32)0), State));
}

inline FString RenderRowAt(const FTestGameState &State, int32 Y, int32 X,
                            int32 Width, const FString &Acc) {
  return X >= Width
             ? Acc
             : RenderRowAt(State, Y, X + 1, Width,
                            Acc + (X > 0 ? FString(TEXT(" ")) : FString()) +
                                CellAt(FPosition(X, Y), State));
}

inline FString RenderRows(const FTestGameState &State, int32 Y, int32 Height,
                           const FString &Acc) {
  return Y >= Height
             ? Acc
             : RenderRows(State, Y + 1, Height,
                           Acc + (Y > 0 ? FString(TEXT("\n")) : FString()) +
                               RenderRowAt(State, Y, 0, State.Grid.Width,
                                           FString()));
}
} // namespace detail

inline FString RenderGrid(const FTestGameState &State) {
  return detail::RenderRows(State, 0, State.Grid.Height, FString());
}

/**
 * Renders a compact legend string.
 * User Story: As terminal rendering, I need a legend string so players can
 * interpret the ASCII symbols shown in the grid output.
 */
inline FString RenderLegend() {
  return TEXT("Legend :: P=Scout  D=Doomguard  M=Miller  #=Blocked  .=Open");
}

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
