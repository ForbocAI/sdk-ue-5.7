#pragma once
/**
 * Test-game ASCII grid rendering helpers.
 *
 * This header is intentionally narrow: it renders the test-game world as a
 * legend + grid of ASCII cells. It is NOT an executor boundary and contains
 * no command dispatch. All test-game command execution must flow through
 * TestGame::CommandSurface (defined in TestGame/TestGameCommandSurface.h),
 * which delegates to the canonical CLIOps command surface.
 *
 * Background: this file was split out of the legacy TestGameLib.h, which
 * previously combined ASCII rendering, runtime URL helpers, and an
 * in-process command executor on one surface. The executor was retired
 * in favor of the first-class commandlet/command-surface boundary; the
 * remaining pure helpers were split into TestGameRuntime.h and
 * TestGameGridRender.h so test-game code can include only what it needs
 * without re-importing executor-shaped concerns.
 *
 * User Story: As terminal rendering, I need an ASCII grid view so scenario
 * output can show the current world state without coupling to the
 * command-execution path.
 */

#include "CoreMinimal.h"
#include "Core/functional_core.hpp"
#include "RuntimeStore.h"
#include "TestGame/TestGameStore.h"
#include "TestGame/TestGameTypes.h"

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

} // namespace TestGame
