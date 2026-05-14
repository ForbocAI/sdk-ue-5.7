#pragma once
/**
 * Test-game store composition — mirrors TS test-game/src/store.ts
 * Combines 13 game slices into a single Redux store
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */

#include "CoreMinimal.h"
#include "Core/rtk.hpp"
#include "TestGame/TestGameSlices.h"

namespace TestGame {

/**
 * Root state — all 13 slice states
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */

struct FTestGameState {
  /**
   * Domain 1: Entities
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  FNPCsSliceState NPCs;
  FPlayerState Player;

  /**
   * Domain 2: Mechanics
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  FGridState Grid;
  FStealthState Stealth;
  FSocialState Social;
  FBridgeRulesState Bridge;

  /**
   * Domain 3: Store
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  FGameMemorySliceState Memory;
  FInventoryState Inventory;
  FSoulTrackingState Soul;

  /**
   * Domain 4: Terminal
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  FUIState UI;
  FTranscriptState Transcript;

  /**
   * Domain 5: Autoplay
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  FScenarioSliceState Scenario;
  FHarnessState Harness;
};

/**
 * Slice singletons
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */

namespace GameSlices {

/**
 * Returns the NPC slice singleton for the test game store.
 * User Story: As root reducer composition, I need access to the NPC slice so
 * root state reduction reuses one canonical slice instance.
 */
inline const rtk::Slice<FNPCsSliceState> &NPCs() {
  static const auto S = CreateNPCsSlice();
  return S;
}
/**
 * Returns the player slice singleton for the test game store.
 * User Story: As root reducer composition, I need access to the player slice
 * so player state reduction reuses one canonical slice instance.
 */
inline const rtk::Slice<FPlayerState> &Player() {
  static const auto S = CreatePlayerSlice();
  return S;
}
/**
 * Returns the grid slice singleton for the test game store.
 * User Story: As root reducer composition, I need access to the grid slice so
 * world layout state is reduced through one shared slice instance.
 */
inline const rtk::Slice<FGridState> &Grid() {
  static const auto S = CreateGridSlice();
  return S;
}
/**
 * Returns the stealth slice singleton for the test game store.
 * User Story: As root reducer composition, I need access to the stealth slice
 * so alert and door state are reduced through one shared slice instance.
 */
inline const rtk::Slice<FStealthState> &Stealth() {
  static const auto S = CreateStealthSlice();
  return S;
}
/**
 * Returns the social slice singleton for the test game store.
 * User Story: As root reducer composition, I need access to the social slice
 * so dialogue and trade state are reduced through one shared slice instance.
 */
inline const rtk::Slice<FSocialState> &Social() {
  static const auto S = CreateSocialSlice();
  return S;
}
/**
 * Returns the bridge slice singleton for the test game store.
 * User Story: As root reducer composition, I need access to the bridge slice
 * so local bridge rules are reduced through one shared slice instance.
 */
inline const rtk::Slice<FBridgeRulesState> &Bridge() {
  static const auto S = CreateGameBridgeSlice();
  return S;
}
/**
 * Returns the memory slice singleton for the test game store.
 * User Story: As root reducer composition, I need access to the memory slice
 * so local memory records are reduced through one shared slice instance.
 */
inline const rtk::Slice<FGameMemorySliceState> &Memory() {
  static const auto S = CreateGameMemorySlice();
  return S;
}
/**
 * Returns the inventory slice singleton for the test game store.
 * User Story: As root reducer composition, I need access to the inventory
 * slice so owner item lists are reduced through one shared slice instance.
 */
inline const rtk::Slice<FInventoryState> &Inventory() {
  static const auto S = CreateInventorySlice();
  return S;
}
/**
 * Returns the soul slice singleton for the test game store.
 * User Story: As root reducer composition, I need access to the soul slice so
 * export and import tracking are reduced through one shared slice instance.
 */
inline const rtk::Slice<FSoulTrackingState> &Soul() {
  static const auto S = CreateGameSoulSlice();
  return S;
}
/**
 * Returns the UI slice singleton for the test game store.
 * User Story: As root reducer composition, I need access to the UI slice so
 * mode and message state are reduced through one shared slice instance.
 */
inline const rtk::Slice<FUIState> &UI() {
  static const auto S = CreateUISlice();
  return S;
}
/**
 * Returns the transcript slice singleton for the test game store.
 * User Story: As root reducer composition, I need access to the transcript
 * slice so command history is reduced through one shared slice instance.
 */
inline const rtk::Slice<FTranscriptState> &Transcript() {
  static const auto S = CreateTranscriptSlice();
  return S;
}
/**
 * Returns the scenario slice singleton for the test game store.
 * User Story: As root reducer composition, I need access to the scenario slice
 * so the default step list is reduced through one shared slice instance.
 */
inline const rtk::Slice<FScenarioSliceState> &Scenario() {
  static const auto S = CreateScenarioSlice();
  return S;
}
/**
 * Returns the harness slice singleton for the test game store.
 * User Story: As root reducer composition, I need access to the harness slice
 * so CLI coverage state is reduced through one shared slice instance.
 */
inline const rtk::Slice<FHarnessState> &Harness() {
  static const auto S = CreateHarnessSlice();
  return S;
}

} // namespace GameSlices

/**
 * Reduces one action across all test-game slices.
 * User Story: As test-game store updates, I need a root reducer so every slice
 * receives the same action and the combined state stays in sync.
 */
inline FTestGameState TestGameReducer(const FTestGameState &State,
                                      const rtk::AnyAction &Action) {
  FTestGameState Next;
  Next.NPCs = GameSlices::NPCs().Reducer(State.NPCs, Action);
  Next.Player = GameSlices::Player().Reducer(State.Player, Action);
  Next.Grid = GameSlices::Grid().Reducer(State.Grid, Action);
  Next.Stealth = GameSlices::Stealth().Reducer(State.Stealth, Action);
  Next.Social = GameSlices::Social().Reducer(State.Social, Action);
  Next.Bridge = GameSlices::Bridge().Reducer(State.Bridge, Action);
  Next.Memory = GameSlices::Memory().Reducer(State.Memory, Action);
  Next.Inventory = GameSlices::Inventory().Reducer(State.Inventory, Action);
  Next.Soul = GameSlices::Soul().Reducer(State.Soul, Action);
  Next.UI = GameSlices::UI().Reducer(State.UI, Action);
  Next.Transcript =
      GameSlices::Transcript().Reducer(State.Transcript, Action);
  Next.Scenario = GameSlices::Scenario().Reducer(State.Scenario, Action);
  Next.Harness = GameSlices::Harness().Reducer(State.Harness, Action);
  return Next;
}

inline FTestGameState CreateInitialTestGameState() {
  FTestGameState Initial;
  Initial.Scenario.Steps = {};
  return Initial;
}

/**
 * Creates a fresh test-game store instance.
 * User Story: As deterministic game runs, I need a new store per session so
 * each run starts from a clean, reproducible state baseline.
 */
inline rtk::EnhancedStore<FTestGameState> createTestGameStore() {
  std::vector<rtk::Middleware<FTestGameState>> Middlewares;
  /**
   * Listener middleware will be added via TestGameListeners.h
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  return rtk::configureStore<FTestGameState>(&TestGameReducer,
                                             CreateInitialTestGameState(),
                                             Middlewares);
}

} // namespace TestGame
