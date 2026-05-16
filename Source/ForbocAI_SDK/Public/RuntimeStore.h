#pragma once

#include "API/APISlice.h"
#include "Bridge/BridgeSlice.h"
#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "Core/functional_core.hpp"
#include "DirectiveSlice.h"
#include "ForbocAILog.h"
#include "Ghost/GhostSlice.h"
#include "Memory/MemorySlice.h"
#include "NPC/NPCSlice.h"
#include "Soul/SoulSlice.h"

struct FStoreState {
  NPCSlice::FNPCSliceState NPCs;
  MemorySlice::FMemorySliceState Memory;
  DirectiveSlice::FDirectiveSliceState Directives;
  BridgeSlice::FBridgeSliceState Bridge;
  SoulSlice::FSoulSliceState Soul;
  GhostSlice::FGhostSliceState Ghost;
  APISlice::FAPIState API;

  /**
   * G8: Generic state bag for game-specific slices.
   * User Story: As game-specific runtime extensions, I need a shared extra bag
   * so custom slice state can live beside SDK-managed state.
   * Games can store serialized state keyed by slice name.
   * Extra reducers operate on this map alongside SDK reducers.
   */
  TMap<FString, FString> Extra;
};

namespace StoreInternal {

/**
 * Returns the singleton NPC slice definition.
 * User Story: As store assembly, I need one shared NPC slice instance so every
 * store uses the same reducer wiring.
 */
inline const rtk::Slice<NPCSlice::FNPCSliceState> &GetNPCSlice() {
  static const rtk::Slice<NPCSlice::FNPCSliceState> Slice =
      NPCSlice::CreateNPCSlice();
  return Slice;
}

/**
 * Returns the singleton memory slice definition.
 * User Story: As store assembly, I need one shared memory slice instance so
 * memory reducers stay consistent across stores.
 */
inline const rtk::Slice<MemorySlice::FMemorySliceState> &GetMemorySlice() {
  static const rtk::Slice<MemorySlice::FMemorySliceState> Slice =
      MemorySlice::CreateMemorySlice();
  return Slice;
}

/**
 * Returns the singleton directive slice definition.
 * User Story: As store assembly, I need one shared directive slice instance so
 * directive lifecycle updates are wired uniformly.
 */
inline const rtk::Slice<DirectiveSlice::FDirectiveSliceState> &
GetDirectiveSlice() {
  static const rtk::Slice<DirectiveSlice::FDirectiveSliceState> Slice =
      DirectiveSlice::CreateDirectiveSlice();
  return Slice;
}

/**
 * Returns the singleton bridge slice definition.
 * User Story: As store assembly, I need one shared bridge slice instance so
 * validation state uses a single reducer definition.
 */
inline const rtk::Slice<BridgeSlice::FBridgeSliceState> &GetBridgeSlice() {
  static const rtk::Slice<BridgeSlice::FBridgeSliceState> Slice =
      BridgeSlice::CreateBridgeSlice();
  return Slice;
}

/**
 * Returns the singleton soul slice definition.
 * User Story: As store assembly, I need one shared soul slice instance so
 * export and import state uses the same reducer wiring.
 */
inline const rtk::Slice<SoulSlice::FSoulSliceState> &GetSoulSlice() {
  static const rtk::Slice<SoulSlice::FSoulSliceState> Slice =
      SoulSlice::CreateSoulSlice();
  return Slice;
}

/**
 * Returns the singleton ghost slice definition.
 * User Story: As store assembly, I need one shared ghost slice instance so QA
 * state transitions are defined once for the runtime.
 */
inline const rtk::Slice<GhostSlice::FGhostSliceState> &GetGhostSlice() {
  static const rtk::Slice<GhostSlice::FGhostSliceState> Slice =
      GhostSlice::CreateGhostSlice();
  return Slice;
}

/**
 * Returns the singleton API slice definition.
 * User Story: As store assembly, I need one shared API slice instance so
 * endpoint cache state is wired consistently.
 */
inline const rtk::Slice<APISlice::FAPIState> &GetAPISlice() {
  static const rtk::Slice<APISlice::FAPIState> Slice =
      APISlice::CreateAPISlice();
  return Slice;
}

} // namespace StoreInternal

/**
 * G8: Extra reducer type for game slices.
 * User Story: As game extension points, I need a reducer hook type so custom
 * game reducers can participate without replacing SDK reducers.
 * Receives current state + action, returns updated state.
 * Only the Extra map should be modified; SDK slice state is managed
 * by SDK reducers and must not be overwritten.
 */
using ExtraReducerFn =
    std::function<FStoreState(const FStoreState &, const rtk::AnyAction &)>;

namespace StoreInternal {

/**
 * Returns the extra reducers registered by game-specific extensions.
 * User Story: As store extensibility, I need a shared reducer registry so
 * game-specific reducers can be mounted before store creation.
 */
inline std::vector<ExtraReducerFn> &ExtraReducers() {
  static std::vector<ExtraReducerFn> Reducers;
  return Reducers;
}

inline FString SummarizeNPCState(const NPCSlice::FNPCSliceState &State) {
  return FString::Printf(TEXT("ids=%d active=%s"), State.Entities.ids.Num(),
                         *State.ActiveNpcId);
}

inline FString SummarizeMemoryState(const MemorySlice::FMemorySliceState &State) {
  return FString::Printf(TEXT("ids=%d store=%s recall=%s recalled=%d error=%s"),
                         State.Entities.ids.Num(), *State.StorageStatus,
                         *State.RecallStatus, State.LastRecalledIds.Num(),
                         *State.Error);
}

inline FString
SummarizeDirectiveState(const DirectiveSlice::FDirectiveSliceState &State) {
  return FString::Printf(TEXT("ids=%d active=%s"), State.Entities.ids.Num(),
                         *State.ActiveDirectiveId);
}

inline FString SummarizeBridgeState(const BridgeSlice::FBridgeSliceState &State) {
  return FString::Printf(
      TEXT("status=%s presets=%d rulesets=%d presetIds=%d validated=%s error=%s"),
      *State.Status, State.ActivePresets.Num(), State.AvailableRulesets.Num(),
      State.AvailablePresetIds.Num(),
      State.bHasLastValidation ? TEXT("true") : TEXT("false"), *State.Error);
}

inline FString SummarizeSoulState(const SoulSlice::FSoulSliceState &State) {
  return FString::Printf(
      TEXT("export=%s import=%s hasExport=%s hasImport=%s available=%d error=%s"),
      *State.ExportStatus, *State.ImportStatus,
      State.bHasLastExport ? TEXT("true") : TEXT("false"),
      State.bHasLastImport ? TEXT("true") : TEXT("false"),
      State.AvailableSouls.Num(), *State.Error);
}

inline FString SummarizeGhostState(const GhostSlice::FGhostSliceState &State) {
  return FString::Printf(
      TEXT("session=%s status=%s progress=%.2f hasResults=%s history=%d loading=%s error=%s"),
      *State.ActiveSessionId, *State.Status, State.Progress,
      State.bHasResults ? TEXT("true") : TEXT("false"), State.History.Num(),
      State.bLoading ? TEXT("true") : TEXT("false"), *State.Error);
}

inline FString SummarizeAPIState(const APISlice::FAPIState &State) {
  return FString::Printf(TEXT("endpoint=%s status=%s error=%s"),
                         *State.LastEndpoint, *State.Status, *State.Error);
}

inline FString SummarizeExtraState(const TMap<FString, FString> &Extra) {
  return FString::Printf(TEXT("entries=%d"), Extra.Num());
}

inline void AppendDeltaIfChanged(TArray<FString> &Changes, const FString &Label,
                                 const FString &Before, const FString &After) {
  Before == After
      ? void()
      : (Changes.Add(FString::Printf(TEXT("%s{%s -> %s}"), *Label, *Before,
                                     *After)),
         void());
}

inline FString DescribeStateDelta(const FStoreState &Before,
                                  const FStoreState &After) {
  TArray<FString> Changes;
  AppendDeltaIfChanged(Changes, TEXT("NPCs"), SummarizeNPCState(Before.NPCs),
                       SummarizeNPCState(After.NPCs));
  AppendDeltaIfChanged(Changes, TEXT("Memory"),
                       SummarizeMemoryState(Before.Memory),
                       SummarizeMemoryState(After.Memory));
  AppendDeltaIfChanged(Changes, TEXT("Directives"),
                       SummarizeDirectiveState(Before.Directives),
                       SummarizeDirectiveState(After.Directives));
  AppendDeltaIfChanged(Changes, TEXT("Bridge"),
                       SummarizeBridgeState(Before.Bridge),
                       SummarizeBridgeState(After.Bridge));
  AppendDeltaIfChanged(Changes, TEXT("Soul"), SummarizeSoulState(Before.Soul),
                       SummarizeSoulState(After.Soul));
  AppendDeltaIfChanged(Changes, TEXT("Ghost"),
                       SummarizeGhostState(Before.Ghost),
                       SummarizeGhostState(After.Ghost));
  AppendDeltaIfChanged(Changes, TEXT("API"), SummarizeAPIState(Before.API),
                       SummarizeAPIState(After.API));
  AppendDeltaIfChanged(Changes, TEXT("Extra"),
                       SummarizeExtraState(Before.Extra),
                       SummarizeExtraState(After.Extra));
  return Changes.Num() == 0 ? TEXT("<none>") : FString::Join(Changes, TEXT("; "));
}

} // namespace StoreInternal (extension)

/**
 * Runs the SDK reducers, then applies any registered extra reducers.
 * User Story: As root store reduction, I need SDK and game reducers composed
 * together so one dispatch updates all registered state.
 */
inline FStoreState StoreReducer(const FStoreState &State,
                                const rtk::AnyAction &Action) {
  FStoreState Next = State;
  Next.NPCs = StoreInternal::GetNPCSlice().Reducer(State.NPCs, Action);
  Next.Memory = StoreInternal::GetMemorySlice().Reducer(State.Memory, Action);
  Next.Directives =
      StoreInternal::GetDirectiveSlice().Reducer(State.Directives, Action);
  Next.Bridge = StoreInternal::GetBridgeSlice().Reducer(State.Bridge, Action);
  Next.Soul = StoreInternal::GetSoulSlice().Reducer(State.Soul, Action);
  Next.Ghost = StoreInternal::GetGhostSlice().Reducer(State.Ghost, Action);
  Next.API = StoreInternal::GetAPISlice().Reducer(State.API, Action);

  /**
   * G8: Run extra reducers (game slices) — recursive application.
   * User Story: As a maintainer, I need this implementation note so I can understand which milestone behavior the surrounding code is preserving.
   */
  return [&]() -> FStoreState {
    struct ApplyReducers {
      static FStoreState apply(FStoreState S, const rtk::AnyAction &A,
                               const std::vector<ExtraReducerFn> &Rs,
                               size_t Index) {
        return Index >= Rs.size() ? S
                                 : apply(Rs[Index](S, A), A, Rs, Index + 1);
      }
    };
    return ApplyReducers::apply(Next, Action, StoreInternal::ExtraReducers(),
                                0);
  }();
}

/**
 * Builds middleware that clears dependent state when an NPC is removed.
 * User Story: As NPC teardown, I need dependent slices cleaned up
 * automatically so removed NPCs do not leave stale state behind.
 */
inline rtk::Middleware<FStoreState> createNpcRemovalListener() {
  return [](const rtk::MiddlewareApi<FStoreState> &Api)
             -> std::function<rtk::Dispatcher(rtk::Dispatcher)> {
    return [Api](rtk::Dispatcher Next) -> rtk::Dispatcher {
      return [Api, Next](const rtk::AnyAction &Action) -> rtk::AnyAction {
        const FString ActiveNpcIdBefore = Api.getState().NPCs.ActiveNpcId;
        const rtk::AnyAction Result = Next(Action);

        NPCSlice::Actions::RemoveNPCActionCreator().match(Action)
            ? [&]() {
                const auto RemovedNpcId =
                    NPCSlice::Actions::RemoveNPCActionCreator().extract(Action);
                RemovedNpcId.hasValue
                    ? (Api.dispatch(
                           DirectiveSlice::Actions::ClearDirectivesForNpc(
                               RemovedNpcId.value)),
                       Api.dispatch(
                           BridgeSlice::Actions::ClearBridgeValidation()),
                       Api.dispatch(
                           GhostSlice::Actions::ClearGhostSession()),
                       Api.dispatch(SoulSlice::Actions::ClearSoulState()),
                       Api.dispatch(
                           NPCSlice::Actions::ClearBlock(RemovedNpcId.value)),
                       RemovedNpcId.value == ActiveNpcIdBefore
                           ? (Api.dispatch(
                                  MemorySlice::Actions::MemoryClear()),
                              void())
                           : void(),
                       void())
                    : void();
              }()
            : void();

        return Result;
      };
    };
  };
}

inline rtk::Middleware<FStoreState> createReduxLoggerMiddleware() {
  return [](const rtk::MiddlewareApi<FStoreState> &Api)
             -> std::function<rtk::Dispatcher(rtk::Dispatcher)> {
    return [Api](rtk::Dispatcher Next) -> rtk::Dispatcher {
      return [Api, Next](const rtk::AnyAction &Action) -> rtk::AnyAction {
        const FStoreState Before = Api.getState();
        const rtk::AnyAction Result = Next(Action);
        const FString Delta = StoreInternal::DescribeStateDelta(Before, Api.getState());
        UE_LOG(LogForbocAIRedux, Display,
               TEXT("[ForbocAI][Redux] action=%s payload=%s delta=%s"),
               *Action.Type, *Action.describePayload(), *Delta);
        return Result;
      };
    };
  };
}

/**
 * G8: Register an extra reducer before store creation.
 * User Story: As game integration, I need a registration hook so custom game
 * reducers can extend the store without replacing SDK behavior.
 * Game slices call this to mount their reducers alongside SDK slices.
 *
 * Example:
 *   addExtraReducer([](const FStoreState &S, const rtk::AnyAction &A) {
 *       FStoreState Next = S;
 *       if (A.Type == TEXT("game/setScore")) {
 *           Next.Extra.Add(TEXT("score"), A.Type);
 *       }
 *       return Next;
 *   });
 */
inline void addExtraReducer(const ExtraReducerFn &Reducer) {
  StoreInternal::ExtraReducers().push_back(Reducer);
}

/**
 * Creates a store with optional preloaded state and additional middleware.
 * User Story: As runtime bootstrap, I need a configurable store factory so
 * tests and games can start from custom state and middleware.
 */
inline rtk::EnhancedStore<FStoreState>
createStore(func::Maybe<FStoreState> PreloadedState =
                func::nothing<FStoreState>(),
            std::vector<rtk::Middleware<FStoreState>> ExtraMiddlewares = {}) {
  std::vector<rtk::Middleware<FStoreState>> Middlewares;
  Middlewares.push_back(createReduxLoggerMiddleware());
  Middlewares.push_back(createNpcRemovalListener());

  /**
   * G8: Append game-provided middleware — recursive merge.
   * User Story: As a maintainer, I need this implementation note so I can understand which milestone behavior the surrounding code is preserving.
   */
  struct AppendMiddlewares {
    static void apply(std::vector<rtk::Middleware<FStoreState>> &Target,
                      const std::vector<rtk::Middleware<FStoreState>> &Source,
                      size_t Index) {
      Index < Source.size()
          ? (Target.push_back(Source[Index]),
             apply(Target, Source, Index + 1), void())
          : void();
    }
  };
  AppendMiddlewares::apply(Middlewares, ExtraMiddlewares, 0);

  return rtk::configureStore<FStoreState>(
      &StoreReducer,
      PreloadedState.hasValue ? PreloadedState.value : FStoreState(),
      Middlewares);
}

/**
 * Returns the process-wide singleton runtime store.
 * User Story: As shared runtime access, I need a singleton store so Blueprint,
 * CLI, and subsystem helpers all dispatch through the same state container.
 */
inline rtk::EnhancedStore<FStoreState> ConfigureStore() {
  static rtk::EnhancedStore<FStoreState> GlobalStore = createStore();
  return GlobalStore;
}
