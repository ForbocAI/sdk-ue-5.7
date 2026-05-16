#pragma once
#ifndef RTK_HPP
#define RTK_HPP

#include "CoreMinimal.h"
#include "functional_core.hpp"
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace rtk {

struct FEmptyPayload;

namespace payload_debug {

inline FString DebugPayloadString(const FEmptyPayload &) { return TEXT("<none>"); }

inline FString DebugPayloadString(const FString &Value) { return Value; }

inline FString DebugPayloadString(const bool &Value) {
  return Value ? TEXT("true") : TEXT("false");
}

template <typename T>
typename std::enable_if<std::is_integral<T>::value &&
                            !std::is_same<T, bool>::value,
                        FString>::type
DebugPayloadString(const T &Value) {
  return FString::Printf(TEXT("%lld"), static_cast<long long>(Value));
}

template <typename T>
typename std::enable_if<std::is_floating_point<T>::value, FString>::type
DebugPayloadString(const T &Value) {
  return FString::SanitizeFloat(static_cast<double>(Value));
}

template <typename T> class HasToString {
  template <typename U>
  static auto Test(int) -> decltype(std::declval<const U &>().ToString(),
                                    std::true_type());

  template <typename> static std::false_type Test(...);

public:
  static const bool value = decltype(Test<T>(0))::value;
};

template <typename T>
typename std::enable_if<HasToString<T>::value, FString>::type
DebugPayloadString(const T &Value) {
  return Value.ToString();
}

template <typename T>
FString DebugPayloadString(const TArray<T> &Values) {
  return FString::Printf(TEXT("TArray(len=%d)"), Values.Num());
}

template <typename K, typename V> FString DebugPayloadString(const TMap<K, V> &Map) {
  return FString::Printf(TEXT("TMap(len=%d)"), Map.Num());
}

template <typename T>
typename std::enable_if<!HasToString<T>::value &&
                            !std::is_integral<T>::value &&
                            !std::is_floating_point<T>::value &&
                            !std::is_same<T, bool>::value,
                        FString>::type
DebugPayloadString(const T &) {
  return TEXT("<opaque>");
}

} // namespace payload_debug

/**
 * 1.1 Action<Payload>
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
template <typename Payload> struct Action {
  FString Type;
  Payload PayloadValue;
};

struct FEmptyPayload {};

/**
 * Builds a typed action payload envelope.
 * User Story: As reducer and thunk code, I need a typed action envelope so
 * payload-bearing actions move through the store with explicit shape.
 */
template <typename Payload>
Action<Payload> makeAction(const FString &Type, const Payload &PayloadValue) {
  return Action<Payload>{Type, PayloadValue};
}

/**
 * Builds an action that carries no payload data.
 * User Story: As reducer and thunk code, I need an empty-payload action shape
 * so simple lifecycle actions can dispatch without custom structs.
 */
inline Action<FEmptyPayload> makeAction(const FString &Type) {
  return Action<FEmptyPayload>{Type, FEmptyPayload{}};
}

/**
 * Type-erased envelope for heterogeneous root dispatch
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
struct AnyAction {
  FString Type;
  std::shared_ptr<void> PayloadWrapper;
  FString PayloadDebugText;

  /**
   * Constructs an empty type-erased action envelope.
   * User Story: As root dispatch infrastructure, I need a default AnyAction so
   * containers and return paths can be initialized before payload assignment.
   */
  AnyAction() : PayloadDebugText(TEXT("<none>")) {}

  /**
   * Constructs a type-erased action envelope from a type tag and payload wrapper.
   * User Story: As root dispatch infrastructure, I need a type-erased action
   * constructor so heterogeneous payloads can move through one dispatch channel.
   */
  AnyAction(const FString &InType, std::shared_ptr<void> InPayloadWrapper,
            const FString &InPayloadDebugText = TEXT("<opaque>"))
      : Type(InType),
        PayloadWrapper(std::move(InPayloadWrapper)),
        PayloadDebugText(InPayloadDebugText) {}

  /**
   * Extracts a typed payload from the type-erased storage.
   * User Story: As root dispatch consumers, I need a direct payload accessor so
   * infrastructure code can recover stored action data when type ownership is known.
   * Warning: This performs an unchecked static_cast. Callers must ensure the requested
   * payload type matches the stored value. Prefer ActionCreator::extract() when possible.
   */
  template <typename Payload> func::Maybe<Payload> getPayload() const {
    return PayloadWrapper
               ? func::just(*static_cast<Payload *>(PayloadWrapper.get()))
               : func::nothing<Payload>();
  }

  FString describePayload() const {
    return PayloadDebugText.IsEmpty() ? TEXT("<none>") : PayloadDebugText;
  }
};

/**
 * 1.2 Reducer
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
template <typename State, typename ActionT>
using Reducer = std::function<State(const State &, const ActionT &)>;

template <typename State>
using CaseReducer = std::function<State(const State &, const AnyAction &)>;

template <typename State> struct Store;

template <typename State>
Store<State> createCoreStore(State InitialState, CaseReducer<State> ReducerFunc);

template <typename State> const State &getState(const Store<State> &StoreValue);

template <typename State>
AnyAction dispatch(Store<State> &StoreValue, const AnyAction &Action);

template <typename State>
std::function<void()> subscribe(Store<State> &StoreValue,
                                std::function<void()> Callback);

/**
 * 1.3 Store
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
template <typename State> struct Store {
  State CurrentState;
  CaseReducer<State> RootReducer;

  struct Subscriber {
    int64_t Id;
    std::function<void()> Callback;
  };
  std::vector<Subscriber> Subscribers;
  int64_t NextId = 1;

  /**
   * Returns the current store state snapshot.
   * User Story: As store consumers, I need the current state exposed so
   * dispatchers, selectors, and subscribers can inspect runtime data.
   */
  const State &getState() const { return rtk::getState(*this); }

  /**
   * Applies an action and notifies subscribers after the reducer runs.
   * User Story: As store consumers, I need dispatch to update state and notify
   * listeners so runtime flows can react to reducer changes.
   */
  AnyAction dispatch(const AnyAction &action) {
    return rtk::dispatch(*this, action);
  }

  /**
   * Registers a callback and returns an unsubscribe function.
   * User Story: As store consumers, I need subscriptions with unsubscribe
   * handles so runtime code can observe state safely.
   */
  std::function<void()> subscribe(std::function<void()> Callback) {
    return rtk::subscribe(*this, std::move(Callback));
  }
};

namespace detail {
template <typename State>
void notifySubscribersRecursive(
    const std::vector<typename Store<State>::Subscriber> &Subscribers,
    size_t Index) {
  Index == Subscribers.size()
      ? void()
      : (Subscribers[Index].Callback(),
         notifySubscribersRecursive<State>(Subscribers, Index + 1));
}

template <typename State>
void eraseSubscriberAt(std::vector<typename Store<State>::Subscriber> &Subscribers,
                       size_t Index) {
  Subscribers.erase(Subscribers.begin() + Index);
}

template <typename State>
void eraseSubscriberRecursive(
    std::vector<typename Store<State>::Subscriber> &Subscribers, size_t Index,
    int64_t Id) {
  Index == Subscribers.size()
      ? void()
      : (Subscribers[Index].Id == Id
             ? eraseSubscriberAt<State>(Subscribers, Index)
             : eraseSubscriberRecursive<State>(Subscribers, Index + 1, Id),
         void());
}
} // namespace detail

template <typename State>
Store<State> createCoreStore(State InitialState, CaseReducer<State> ReducerFunc) {
  Store<State> StoreValue;
  StoreValue.CurrentState = std::move(InitialState);
  StoreValue.RootReducer = std::move(ReducerFunc);
  return StoreValue;
}

template <typename State> const State &getState(const Store<State> &StoreValue) {
  return StoreValue.CurrentState;
}

template <typename State>
AnyAction dispatch(Store<State> &StoreValue, const AnyAction &Action) {
  StoreValue.CurrentState = StoreValue.RootReducer(StoreValue.CurrentState, Action);
  const std::vector<typename Store<State>::Subscriber> SubsCopy =
      StoreValue.Subscribers;
  detail::notifySubscribersRecursive<State>(SubsCopy, 0);
  return Action;
}

template <typename State>
std::function<void()> subscribe(Store<State> &StoreValue,
                                std::function<void()> Callback) {
  const int64_t Id = StoreValue.NextId++;
  StoreValue.Subscribers.push_back(
      typename Store<State>::Subscriber{Id, std::move(Callback)});
  return [&StoreValue, Id]() {
    detail::eraseSubscriberRecursive<State>(StoreValue.Subscribers, 0, Id);
  };
}

/**
 * 1.4 combineReducers
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
template <typename RootState> struct ReducerCombiner {
  std::vector<
      std::function<bool(RootState &, const RootState &, const AnyAction &)>>
      Bindings;
};

/**
 * Creates an empty reducer combiner for a root state.
 * User Story: As root-store assembly, I need a reducer combiner entry point so
 * slices can be registered incrementally.
 */
template <typename RootState> ReducerCombiner<RootState> combineReducers() {
  return ReducerCombiner<RootState>{{}};
}

/**
 * Adds a reducer binding for a specific slice member on the root state.
 * User Story: As root-store assembly, I need slice bindings declared through
 * free functions so reducer wiring stays outside builder classes.
 */
template <typename RootState, typename SliceState>
ReducerCombiner<RootState>
addReducer(ReducerCombiner<RootState> Combiner, SliceState RootState::*Member,
           CaseReducer<SliceState> ReducerFunc) {
  Combiner.Bindings.push_back(
      [Member, ReducerFunc](RootState &NextState, const RootState &PrevState,
                            const AnyAction &Action) {
        const SliceState &PrevSlice = PrevState.*Member;
        SliceState NextSlice = ReducerFunc(PrevSlice, Action);
        bool bChanged = !(PrevSlice == NextSlice);
        return bChanged ? (NextState.*Member = std::move(NextSlice), true)
                        : false;
      });
  return Combiner;
}

namespace detail {
template <typename RootState>
RootState applyReducerBindingsRecursive(
    const std::vector<
        std::function<bool(RootState &, const RootState &, const AnyAction &)>>
        &Bindings,
    size_t Index, const RootState &PrevState, RootState NextState,
    bool bChanged, const AnyAction &Action);

template <typename RootState>
RootState applyReducerBindingStep(
    const std::vector<
        std::function<bool(RootState &, const RootState &, const AnyAction &)>>
        &Bindings,
    size_t Index, const RootState &PrevState, RootState NextState,
    bool bChanged, const AnyAction &Action) {
  const bool bNextChanged =
      Bindings[Index](NextState, PrevState, Action) ? true : bChanged;
  return applyReducerBindingsRecursive<RootState>(
      Bindings, Index + 1, PrevState, std::move(NextState), bNextChanged,
      Action);
}

template <typename RootState>
RootState applyReducerBindingsRecursive(
    const std::vector<
        std::function<bool(RootState &, const RootState &, const AnyAction &)>>
        &Bindings,
    size_t Index, const RootState &PrevState, RootState NextState,
    bool bChanged, const AnyAction &Action) {
  return Index == Bindings.size()
             ? (bChanged ? NextState : PrevState)
             : applyReducerBindingStep<RootState>(
                   Bindings, Index, PrevState, std::move(NextState), bChanged,
                   Action);
}
} // namespace detail

/**
 * Produces a root reducer that fans an action out to every bound slice.
 * User Story: As root-store assembly, I need one reducer combiner output so
 * bound slices receive the same dispatched action coherently.
 */
template <typename RootState>
CaseReducer<RootState> buildReducer(const ReducerCombiner<RootState> &Combiner) {
  auto Bindings = Combiner.Bindings;
  return [Bindings](const RootState &PrevState,
                    const AnyAction &Action) -> RootState {
    return detail::applyReducerBindingsRecursive<RootState>(
        Bindings, 0, PrevState, PrevState, false, Action);
  };
}

/**
 * 2.1 createAction<P> and Matchers
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
template <typename Payload> struct ActionCreator {
  FString Type;

  AnyAction operator()(const Payload &payload) const {
    return AnyAction(Type, std::make_shared<Payload>(payload),
                     payload_debug::DebugPayloadString(payload));
  }

  /**
   * Reports whether an AnyAction matches this creator's type tag.
   * User Story: As reducer helpers, I need action-type matching so handlers can
   * confirm payload shape before extraction.
   */
  bool match(const AnyAction &action) const { return action.Type == Type; }

  /**
   * Extracts a typed payload when the action type matches this creator.
   * User Story: As reducer helpers, I need safe payload extraction so typed
   * reducers can consume AnyAction without repeating casts.
   */
  func::Maybe<Payload> extract(const AnyAction &action) const {
    return match(action) ? action.getPayload<Payload>()
                         : func::nothing<Payload>();
  }
};

/**
 * Creates a typed action creator for a namespaced action type.
 * User Story: As slice code, I need typed action creators so reducers and
 * thunks can share stable action contracts.
 */
template <typename Payload>
ActionCreator<Payload> createAction(const FString &Type) {
  return ActionCreator<Payload>{Type};
}

struct EmptyActionCreator {
  FString Type;

  AnyAction operator()() const {
    return AnyAction(Type, std::make_shared<FEmptyPayload>(),
                     payload_debug::DebugPayloadString(FEmptyPayload{}));
  }

  /**
   * Reports whether an AnyAction matches this empty action creator.
   * User Story: As reducer helpers, I need empty-action matching so lifecycle
   * actions can be recognized without custom payload structs.
   */
  bool match(const AnyAction &action) const { return action.Type == Type; }
};

/**
 * Creates an empty-payload action creator for a namespaced action type.
 * User Story: As slice code, I need empty action creators so simple lifecycle
 * events can reuse the same RTK-style creation pattern.
 */
inline EmptyActionCreator createAction(const FString &Type) {
  return EmptyActionCreator{Type};
}

/**
 * 2.2 Slice<State>
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
template <typename State> struct Slice {
  FString Name;
  CaseReducer<State> Reducer;
};

/**
 * 2.3 SliceBuilder for generating slices and binding action creators locally
 * User Story: As a maintainer, I need this note so the surrounding code intent
 * stays clear during maintenance and debugging.
 */
template <typename State> struct SliceBuilder {
  FString Name;
  State InitialState;
  TMap<FString, CaseReducer<State>> Reducers;
};

/**
 * Constructs a slice builder with a name and initial state.
 * User Story: As slice authors, I need a builder factory so slice naming and
 * initial state are defined before reducer cases are registered.
 */
template <typename State>
SliceBuilder<State> sliceBuilder(FString InName, State InInitialState) {
  SliceBuilder<State> Builder;
  Builder.Name = MoveTemp(InName);
  Builder.InitialState = MoveTemp(InInitialState);
  return Builder;
}

/**
 * Registers a typed reducer case and returns its local action creator.
 * User Story: As slice authors, I need local case registration so reducers and
 * action creators can be declared together without class methods.
 */
template <typename Payload, typename State, typename ReducerFn>
ActionCreator<Payload>
createCase(SliceBuilder<State> &Builder, const FString &ShortName,
           ReducerFn ReducerFunc) {
  std::function<State(const State &, const Action<Payload> &)> WrappedReducer =
      ReducerFunc;
  FString FullType = Builder.Name + TEXT("/") + ShortName;
  ActionCreator<Payload> Creator{FullType};

  Builder.Reducers.Add(
      FullType, [Creator, WrappedReducer](const State &PrevState,
                                          const AnyAction &AnyActionValue) {
        auto PayloadOpt = Creator.extract(AnyActionValue);
        return PayloadOpt.hasValue
                   ? WrappedReducer(
                         PrevState,
                         makeAction(AnyActionValue.Type, PayloadOpt.value))
                   : PrevState;
      });

  return Creator;
}

/**
 * Registers an empty-payload reducer case and returns its action creator.
 * User Story: As slice authors, I need empty case registration so lifecycle
 * reducers can be declared without custom payload types.
 */
template <typename State, typename ReducerFn>
EmptyActionCreator createCase(SliceBuilder<State> &Builder,
                              const FString &ShortName,
                              ReducerFn ReducerFunc) {
  std::function<State(const State &, const Action<FEmptyPayload> &)>
      WrappedReducer = ReducerFunc;
  FString FullType = Builder.Name + TEXT("/") + ShortName;
  EmptyActionCreator Creator{FullType};

  Builder.Reducers.Add(
      FullType, [Creator, WrappedReducer](const State &PrevState,
                                          const AnyAction &AnyActionValue) {
        return Creator.match(AnyActionValue)
                   ? WrappedReducer(PrevState, makeAction(AnyActionValue.Type))
                   : PrevState;
      });

  return Creator;
}

template <typename CreatorT, typename ReducerFn> struct ExtraCaseBinding {
  CreatorT Creator;
  ReducerFn ReducerFunc;
};

/**
 * Creates a pipe-friendly extra-case binding description.
 * User Story: As slice authors, I need declarative extra-case descriptors so
 * slice construction can remain chained with free functions.
 */
template <typename CreatorT, typename ReducerFn>
ExtraCaseBinding<CreatorT, ReducerFn>
addExtraCase(const CreatorT &Creator, ReducerFn ReducerFunc) {
  return ExtraCaseBinding<CreatorT, ReducerFn>{Creator, ReducerFunc};
}

/**
 * Binds an externally created typed action to this slice.
 * User Story: As slice authors, I need externally owned actions bindable into
 * a slice so cross-slice lifecycle handling stays ergonomic.
 */
template <typename State, typename Payload, typename ReducerFn>
SliceBuilder<State>
addExtraCase(SliceBuilder<State> Builder, const ActionCreator<Payload> &Creator,
             ReducerFn ReducerFunc) {
  std::function<State(const State &, const Action<Payload> &)> WrappedReducer =
      ReducerFunc;
  Builder.Reducers.Add(
      Creator.Type,
      [Creator, WrappedReducer](const State &PrevState,
                                const AnyAction &AnyActionValue) -> State {
        auto PayloadOpt = Creator.extract(AnyActionValue);
        return PayloadOpt.hasValue
                   ? WrappedReducer(
                         PrevState,
                         makeAction(AnyActionValue.Type, PayloadOpt.value))
                   : PrevState;
      });
  return Builder;
}

/**
 * Binds an externally created empty action to this slice.
 * User Story: As slice authors, I need external empty actions bindable into a
 * slice so lifecycle wiring stays consistent across modules.
 */
template <typename State, typename ReducerFn>
SliceBuilder<State>
addExtraCase(SliceBuilder<State> Builder, const EmptyActionCreator &Creator,
             ReducerFn ReducerFunc) {
  std::function<State(const State &, const Action<FEmptyPayload> &)>
      WrappedReducer = ReducerFunc;
  Builder.Reducers.Add(
      Creator.Type,
      [Creator, WrappedReducer](const State &PrevState,
                                const AnyAction &AnyActionValue) -> State {
        return Creator.match(AnyActionValue)
                   ? WrappedReducer(PrevState, makeAction(AnyActionValue.Type))
                   : PrevState;
      });
  return Builder;
}

/**
 * Supports pipe-style extra-case composition for slice builders.
 * User Story: As slice authors, I need free-function chaining so removing
 * builder classes does not force deeply nested construction code.
 */
template <typename State, typename CreatorT, typename ReducerFn>
SliceBuilder<State>
operator|(SliceBuilder<State> Builder,
          const ExtraCaseBinding<CreatorT, ReducerFn> &Binding) {
  return addExtraCase(std::move(Builder), Binding.Creator, Binding.ReducerFunc);
}

/**
 * Finalizes the slice and its reducer lookup table.
 * User Story: As slice authors, I need a final build step so the slice can be
 * exported with its reducer map intact.
 */
template <typename State> Slice<State> buildSlice(const SliceBuilder<State> &Builder) {
  Slice<State> Result;
  Result.Name = Builder.Name;
  auto ReducerMap = Builder.Reducers;

  Result.Reducer =
      [ReducerMap](const State &PrevState, const AnyAction &Action) -> State {
    const CaseReducer<State> *Found = ReducerMap.Find(Action.Type);
    return Found ? (*Found)(PrevState, Action) : PrevState;
  };
  return Result;
}

/**
 * Phase 3: Entity Adapter
 * User Story: As a maintainer, I need this implementation note so I can understand which milestone behavior the surrounding code is preserving.
 */

template <typename T> struct EntityState {
  TArray<FString> ids;
  TMap<FString, T> entities;
};

template <typename T> struct EntitySelectors {
  std::function<TArray<T>(const EntityState<T> &)> selectAll;
  std::function<func::Maybe<T>(const EntityState<T> &, const FString &)>
      selectById;
  std::function<TArray<FString>(const EntityState<T> &)> selectIds;
  std::function<int32_t(const EntityState<T> &)> selectTotal;
};

template <typename T> struct EntityAdapterOps;

namespace detail {
template <typename T>
void addEntityIfMissing(EntityState<T> &Next, const FString &Id,
                        const T &Entity) {
  const bool bMissing = !Next.entities.Find(Id);
  bMissing && (Next.ids.Add(Id), true);
  bMissing && (Next.entities.Add(Id, Entity), true);
}

template <typename T>
void setEntity(EntityState<T> &Next, const FString &Id, const T &Entity) {
  (!Next.entities.Find(Id)) && (Next.ids.Add(Id), true);
  Next.entities.Add(Id, Entity);
}

template <typename T>
void removeEntityIfPresent(EntityState<T> &Next, const FString &Id) {
  (Next.entities.Remove(Id) > 0) && (Next.ids.Remove(Id), true);
}

template <typename T, typename PatchFn>
void updateEntityIfPresent(EntityState<T> &Next, const FString &Id,
                           PatchFn Patch) {
  const T *Existing = Next.entities.Find(Id);
  Existing && (Next.entities.Add(Id, Patch(*Existing)), true);
}

template <typename T>
void appendEntityIfPresent(TArray<T> &Result, const EntityState<T> &State,
                           const FString &Id) {
  const T *Entity = State.entities.Find(Id);
  Entity && (Result.Add(*Entity), true);
}

template <typename T>
func::Maybe<T> findEntityById(const EntityState<T> &State, const FString &Id) {
  const T *Entity = State.entities.Find(Id);
  return Entity ? func::just(*Entity) : func::nothing<T>();
}

template <typename T>
EntityState<T> addManyEntitiesRecursive(const EntityAdapterOps<T> &Ops,
                                        const TArray<T> &NewEntities,
                                        int32 Index, EntityState<T> Next);

template <typename T>
EntityState<T> setAllEntitiesRecursive(const EntityAdapterOps<T> &Ops,
                                       const TArray<T> &NewEntities,
                                       int32 Index, EntityState<T> Next);

template <typename T>
EntityState<T> upsertManyEntitiesRecursive(const EntityAdapterOps<T> &Ops,
                                           const TArray<T> &EntitiesToUpsert,
                                           int32 Index, EntityState<T> Next);

template <typename T>
EntityState<T> removeManyEntitiesRecursive(const TArray<FString> &RemoveIds,
                                           int32 Index, EntityState<T> Next);

template <typename T>
TArray<T> selectAllEntitiesRecursive(const EntityState<T> &State, int32 Index,
                                     TArray<T> Result);
} // namespace detail

template <typename T> struct EntityAdapterOps {
  std::function<FString(const T &)> selectId;

  /**
   * Returns an empty entity-state container.
   * User Story: As entity-backed slices, I need a canonical empty entity state
   * so adapters can initialize predictable reducer storage.
   */
  EntityState<T> getInitialState() const { return EntityState<T>{{}, {}}; }

  /**
   * Adds a single entity when its id is not already present.
   * User Story: As entity-backed slices, I need single-entity insertion so new
   * records can be added without mutating existing adapter state.
   */
  EntityState<T> addOne(const EntityState<T> &state, const T &entity) const {
    EntityState<T> next = state;
    FString id = selectId(entity);
    detail::addEntityIfMissing(next, id, entity);
    return next;
  }

  /**
   * Adds each missing entity from a batch without replacing existing entries.
   * User Story: As entity-backed slices, I need batch insertion so collections
   * can be seeded while preserving existing records.
   */
  EntityState<T> addMany(const EntityState<T> &state,
                         const TArray<T> &newEntities) const {
    return detail::addManyEntitiesRecursive(*this, newEntities, 0, state);
  }

  /**
   * Inserts or replaces a single entity by id.
   * User Story: As entity-backed slices, I need single-entity replacement so
   * reducers can upsert records deterministically.
   */
  EntityState<T> setOne(const EntityState<T> &state, const T &entity) const {
    EntityState<T> next = state;
    FString id = selectId(entity);
    detail::setEntity(next, id, entity);
    return next;
  }

  /**
   * Replaces the full entity set with the provided collection.
   * User Story: As entity-backed slices, I need whole-collection replacement so
   * reducers can resync adapter state from remote payloads.
   */
  EntityState<T> setAll(const EntityState<T> &state,
                        const TArray<T> &newEntities) const {
    return detail::setAllEntitiesRecursive(*this, newEntities, 0,
                                           EntityState<T>());
  }

  /**
   * Upserts a single entity by delegating to setOne.
   * User Story: As entity-backed slices, I need a semantic upsert helper so
   * reducers can express intent without duplicating adapter logic.
   */
  EntityState<T> upsertOne(const EntityState<T> &state, const T &entity) const {
    return setOne(state, entity);
  }

  /**
   * Upserts a batch of entities by id.
   * User Story: As entity-backed slices, I need batch upsert so synced payloads
   * can merge into adapter state efficiently.
   */
  EntityState<T> upsertMany(const EntityState<T> &state,
                            const TArray<T> &entitiesToUpsert) const {
    return detail::upsertManyEntitiesRecursive(*this, entitiesToUpsert, 0,
                                               state);
  }

  /**
   * Removes a single entity and its id when present.
   * User Story: As entity-backed slices, I need record removal so deleted items
   * disappear from both entity maps and id orderings.
   */
  EntityState<T> removeOne(const EntityState<T> &state,
                           const FString &id) const {
    EntityState<T> next = state;
    detail::removeEntityIfPresent(next, id);
    return next;
  }

  /**
   * Removes all entities whose ids appear in the supplied list.
   * User Story: As entity-backed slices, I need batch removal so reducers can
   * clear multiple records in one pure operation.
   */
  EntityState<T> removeMany(const EntityState<T> &state,
                            const TArray<FString> &removeIds) const {
    return detail::removeManyEntitiesRecursive(removeIds, 0, state);
  }

  /**
   * Clears every entity from the adapter state.
   * User Story: As entity-backed slices, I need a reset helper so adapters can
   * return to a clean initial state predictably.
   */
  EntityState<T> removeAll(const EntityState<T> &) const {
    return getInitialState();
  }

  /**
   * Replaces one entity with the result of a patch function.
   * User Story: As entity-backed slices, I need targeted patching so one record
   * can be updated without rebuilding the full collection manually.
   */
  EntityState<T> updateOne(const EntityState<T> &state, const FString &id,
                           std::function<T(const T &)> patch) const {
    EntityState<T> next = state;
    detail::updateEntityIfPresent(next, id, patch);
    return next;
  }

  /**
   * Builds selector helpers for the current adapter shape.
   * User Story: As entity-backed slices, I need selector helpers so callers can
   * read ids, entities, and totals without hand-rolled lookup code.
   */
  EntitySelectors<T> getSelectors() const {
    const auto SelectAll = [](const EntityState<T> &state) -> TArray<T> {
      return detail::selectAllEntitiesRecursive(state, 0, TArray<T>());
    };

    const auto SelectById =
        [](const EntityState<T> &state, const FString &id) -> func::Maybe<T> {
      return detail::findEntityById(state, id);
    };

    const auto SelectIds = [](const EntityState<T> &state) -> TArray<FString> {
      return state.ids;
    };

    const auto SelectTotal = [](const EntityState<T> &state) -> int32_t {
      return state.ids.Num();
    };

    return EntitySelectors<T>{
        SelectAll, SelectById, SelectIds, SelectTotal};
  }
};

namespace detail {
template <typename T>
EntityState<T> addManyEntitiesRecursive(const EntityAdapterOps<T> &Ops,
                                        const TArray<T> &NewEntities,
                                        int32 Index, EntityState<T> Next) {
  return Index >= NewEntities.Num()
             ? Next
             : (addEntityIfMissing(Next, Ops.selectId(NewEntities[Index]),
                                   NewEntities[Index]),
                addManyEntitiesRecursive(Ops, NewEntities, Index + 1,
                                         std::move(Next)));
}

template <typename T>
EntityState<T> setAllEntitiesRecursive(const EntityAdapterOps<T> &Ops,
                                       const TArray<T> &NewEntities,
                                       int32 Index, EntityState<T> Next) {
  return Index >= NewEntities.Num()
             ? Next
             : (Next.ids.Add(Ops.selectId(NewEntities[Index])),
                Next.entities.Add(Ops.selectId(NewEntities[Index]),
                                  NewEntities[Index]),
                setAllEntitiesRecursive(Ops, NewEntities, Index + 1,
                                        std::move(Next)));
}

template <typename T>
EntityState<T> upsertManyEntitiesRecursive(const EntityAdapterOps<T> &Ops,
                                           const TArray<T> &EntitiesToUpsert,
                                           int32 Index, EntityState<T> Next) {
  return Index >= EntitiesToUpsert.Num()
             ? Next
             : upsertManyEntitiesRecursive(Ops, EntitiesToUpsert, Index + 1,
                                           Ops.setOne(Next,
                                                      EntitiesToUpsert[Index]));
}

template <typename T>
EntityState<T> removeManyEntitiesRecursive(const TArray<FString> &RemoveIds,
                                           int32 Index, EntityState<T> Next) {
  return Index >= RemoveIds.Num()
             ? Next
             : (removeEntityIfPresent(Next, RemoveIds[Index]),
                removeManyEntitiesRecursive(RemoveIds, Index + 1,
                                            std::move(Next)));
}

template <typename T>
TArray<T> selectAllEntitiesRecursive(const EntityState<T> &State, int32 Index,
                                     TArray<T> Result) {
  return Index >= State.ids.Num()
             ? Result
             : (appendEntityIfPresent(Result, State, State.ids[Index]),
                selectAllEntitiesRecursive(State, Index + 1,
                                           std::move(Result)));
}
} // namespace detail

/**
 * Creates entity-adapter operations from an id selector.
 * User Story: As slice authors, I need adapter factories so entity state
 * management can be generated from one id-selection rule.
 */
template <typename T>
EntityAdapterOps<T>
createEntityAdapter(std::function<FString(const T &)> selectId) {
  return EntityAdapterOps<T>{std::move(selectId)};
}

/**
 * Phase 4: Async Thunks
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */

template <typename State> struct ThunkApi {
  std::function<AnyAction(const AnyAction &)> dispatch;
  std::function<State()> getState;
};

template <typename Result, typename State>
using ThunkAction = std::function<func::AsyncResult<Result>(
    std::function<AnyAction(const AnyAction &)>, std::function<State()>)>;

template <typename Result, typename Arg, typename State>
struct AsyncThunkConfig {
  FString TypePrefix;
  ActionCreator<Arg> pending;
  ActionCreator<Result> fulfilled;
  ActionCreator<FString> rejected;

  std::function<ThunkAction<Result, State>(const Arg &)> thunkActionCreator;

  ThunkAction<Result, State> operator()(const Arg &arg) const {
    return thunkActionCreator(arg);
  }
};

/**
 * Creates a thunk config with pending, fulfilled, and rejected lifecycle actions.
 * User Story: As async thunk authors, I need lifecycle action wiring generated
 * automatically so pending and result dispatch stay consistent.
 */
template <typename Result, typename Arg, typename State>
AsyncThunkConfig<Result, Arg, State> createAsyncThunk(
    const FString &TypePrefix,
    std::function<func::AsyncResult<Result>(const Arg &,
                                            const ThunkApi<State> &)>
        PayloadCreator) {
  auto pending = createAction<Arg>(TypePrefix + TEXT("/pending"));
  auto fulfilled = createAction<Result>(TypePrefix + TEXT("/fulfilled"));
  auto rejected = createAction<FString>(TypePrefix + TEXT("/rejected"));

  auto thunkActionCreator = [pending, fulfilled, rejected, PayloadCreator](
                                const Arg &arg) -> ThunkAction<Result, State> {
    return [pending, fulfilled, rejected, PayloadCreator,
            arg](std::function<AnyAction(const AnyAction &)> dispatch,
                 std::function<State()> getState) -> func::AsyncResult<Result> {
      /**
       * 1. Dispatch pending synchronously
       * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
       */
      dispatch(pending(arg));

      /**
       * 2. Build the ThunkApi surface
       * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
       */
      ThunkApi<State> api{dispatch, getState};

      /**
       * 3. Execute payload creator
       * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
       */
      auto result = PayloadCreator(arg, api);

      /**
       * 4. Chain lifecycle actions using FP core AsyncChain.
       *    The returned AsyncResult carries both the fulfilled chain and
       *    the catch_ error handler. The caller is responsible for calling
       *    .execute() on the returned result to trigger the full chain.
       * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
       */
      func::AsyncResult<Result> Chained = func::AsyncChain::then<Result, Result>(
          result, [dispatch, fulfilled](Result res) {
            dispatch(fulfilled(res));
            return func::createAsyncResult<Result>(
                [res](std::function<void(Result)> resolve,
                      std::function<void(std::string)> reject) {
                  resolve(res);
                });
          });
      func::catchAsync(Chained, [dispatch, rejected](std::string err) {
        dispatch(rejected(FString(UTF8_TO_TCHAR(err.c_str()))));
      });
      return Chained;
    };
  };

  return AsyncThunkConfig<Result, Arg, State>{TypePrefix, pending, fulfilled,
                                              rejected, thunkActionCreator};
}

/**
 * Phase 5: Middleware
 * User Story: As a maintainer, I need this implementation note so I can understand which milestone behavior the surrounding code is preserving.
 */

template <typename State> struct MiddlewareApi {
  std::function<AnyAction(const AnyAction &)> dispatch;
  std::function<State()> getState;
};

/**
 * Dispatch function type used throughout the middleware chain.
 * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
 */
using Dispatcher = std::function<AnyAction(const AnyAction &)>;

template <typename State>
using Middleware =
    std::function<std::function<Dispatcher(Dispatcher)>(
        const MiddlewareApi<State> &)>;

namespace detail {
template <typename State>
Dispatcher applyMiddlewareRecursive(
    typename std::vector<Middleware<State>>::const_reverse_iterator It,
    typename std::vector<Middleware<State>>::const_reverse_iterator End,
    const MiddlewareApi<State> &Api, Dispatcher CurrentDispatch) {
  return It == End
             ? CurrentDispatch
             : applyMiddlewareRecursive<State>(It + 1, End, Api,
                                               (*It)(Api)(CurrentDispatch));
}
} // namespace detail

/**
 * Wraps a dispatcher with middleware while preserving RTK-style composition order.
 * User Story: As store configuration, I need middleware composition so dispatch
 * can be enhanced without changing reducer semantics.
 */
template <typename State>
Dispatcher
applyMiddleware(Dispatcher baseDispatch,
                std::function<State()> getState,
                const std::vector<Middleware<State>> &middlewares) {
  /**
   * Use shared_ptr so middleware closures can reference the final enhanced
   * dispatch through indirection, matching RTK's actual behavior.
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  auto enhancedDispatch = std::make_shared<Dispatcher>(baseDispatch);

  auto api = MiddlewareApi<State>{
      /**
       * Trampoline: always calls through the fully composed chain
       * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
       */
      [enhancedDispatch](const AnyAction &action) -> AnyAction {
        return (*enhancedDispatch)(action);
      },
      getState};
  Dispatcher currentDispatch =
      detail::applyMiddlewareRecursive<State>(middlewares.rbegin(),
                                             middlewares.rend(), api,
                                             baseDispatch);

  /**
  * Point the shared dispatch at the fully composed chain
  * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
  */
  *enhancedDispatch = currentDispatch;

  return currentDispatch;
}

template <typename State> struct ListenerMiddleware {
  using EffectCallback = std::function<void(const AnyAction &action,
                                            const MiddlewareApi<State> &api)>;

  TMap<FString, TArray<EffectCallback>> listeners;
};

namespace detail {
template <typename State>
void invokeListenerEffectsRecursive(
    const TArray<typename ListenerMiddleware<State>::EffectCallback> &Effects,
    int32 Index, const AnyAction &Action, const MiddlewareApi<State> &Api) {
  Index >= Effects.Num()
      ? void()
      : (Effects[Index](Action, Api),
         invokeListenerEffectsRecursive<State>(Effects, Index + 1, Action,
                                               Api));
}

template <typename State>
void runListenerEffects(
    const TMap<FString, TArray<typename ListenerMiddleware<State>::EffectCallback>>
        &Listeners,
    const AnyAction &Action, const MiddlewareApi<State> &Api) {
  const TArray<typename ListenerMiddleware<State>::EffectCallback>
      *ActiveListeners = Listeners.Find(Action.Type);
  ActiveListeners
      ? invokeListenerEffectsRecursive<State>(*ActiveListeners, 0, Action, Api)
      : void();
}
} // namespace detail

template <typename State>
ListenerMiddleware<State> createListenerMiddleware() {
  return ListenerMiddleware<State>();
}

template <typename State>
ListenerMiddleware<State>
addListener(ListenerMiddleware<State> MiddlewareValue,
            const FString &ActionType,
            typename ListenerMiddleware<State>::EffectCallback Effect) {
  MiddlewareValue.listeners.FindOrAdd(ActionType).Add(Effect);
  return MiddlewareValue;
}

template <typename State>
Middleware<State>
buildListenerMiddleware(const ListenerMiddleware<State> &MiddlewareValue) {
  const TMap<FString, TArray<typename ListenerMiddleware<State>::EffectCallback>>
      ListenersCopy = MiddlewareValue.listeners;
  return [ListenersCopy](const MiddlewareApi<State> &api)
             -> std::function<Dispatcher(Dispatcher)> {
    return [ListenersCopy, api](Dispatcher next) -> Dispatcher {
      return [ListenersCopy, api, next](const AnyAction &action) -> AnyAction {
        const AnyAction ResultAction = next(action);
        detail::runListenerEffects<State>(ListenersCopy, action, api);
        return ResultAction;
      };
    };
  };
}

/**
 * Phase 6: Selectors
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */

namespace detail {
/**
 * Evaluates a selector combiner against each input selector in a tuple.
 * User Story: As memoized selectors, I need tuple-driven combiner evaluation
 * so composed selectors can reuse one generic implementation.
 */
template <typename Result, typename State, typename Combiner,
          typename InputTuple, size_t... Is>
Result evaluateSelector(const State &state, Combiner &combiner,
                        const InputTuple &inputs, func::seq<Is...>) {
  return combiner(std::get<Is>(inputs)(state)...);
}
} // namespace detail

/**
 * Creates a memoized selector from input selectors and a combiner.
 * User Story: As selector authors, I need memoized selector composition so
 * derived state only recomputes when its inputs change.
 */
template <typename State, typename Result, typename... InSelectors>
std::function<Result(const State &)> createSelector(
    const std::tuple<InSelectors...> &inputSelectors,
    std::function<
        Result(decltype(std::declval<InSelectors>()(
            std::declval<const State &>()))...)>
        combiner) {
  /**
   * Memoize the combiner using FP core's memoizeLast.
   * It will automatically track the tuple of inputs across calls.
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  auto memoizedCombiner = func::memoizeLast(combiner);

  return
      [inputSelectors, memoizedCombiner](const State &state) mutable -> Result {
        return detail::evaluateSelector<Result>(
            state, memoizedCombiner, inputSelectors,
            func::gen_seq<sizeof...(InSelectors)>());
      };
}

/**
 * Phase 7: RTK Query Equivalent (API Slice)
 * User Story: As a maintainer, I need this implementation note so I can understand which milestone behavior the surrounding code is preserving.
 */

struct FApiEndpointTag {
  FString Type;
  FString Id;
};

template <typename Arg, typename Result> struct ApiEndpoint {
  FString EndpointName;
  TArray<FApiEndpointTag> ProvidesTags;
  TArray<FApiEndpointTag> InvalidatesTags;

  /**
   * Abstract request builder/executor
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  std::function<func::AsyncResult<func::HttpResult<Result>>(const Arg &)>
      RequestBuilder;
};

/**
 * Simplified dynamic slice registry mapped by string path
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
template <typename State> struct ApiSlice {
  FString ReducerPath;
  TArray<FString> TagTypes;
};

template <typename State>
ApiSlice<State> createApiSlice(const FString &ReducerPath,
                               const TArray<FString> &TagTypes) {
  ApiSlice<State> Slice;
  Slice.ReducerPath = ReducerPath;
  Slice.TagTypes = TagTypes;
  return Slice;
}

template <typename Result>
func::AsyncResult<Result>
unwrapEndpointResult(func::HttpResult<Result> HttpResultValue) {
  return HttpResultValue.bSuccess
             ? func::createAsyncResult<Result>(
                   [HttpResultValue](std::function<void(Result)> Resolve,
                                     std::function<void(std::string)> Reject) {
                     Resolve(HttpResultValue.data);
                   })
             : func::createAsyncResult<Result>(
                   [HttpResultValue](std::function<void(Result)> Resolve,
                                     std::function<void(std::string)> Reject) {
                     Reject(HttpResultValue.error);
                   });
}

template <typename State, typename Arg, typename Result>
AsyncThunkConfig<Result, Arg, State>
injectEndpoint(const ApiSlice<State> &Slice,
               const ApiEndpoint<Arg, Result> &EndpointDesc) {
  const FString ThunkPrefix = Slice.ReducerPath + TEXT("/") + EndpointDesc.EndpointName;
  return createAsyncThunk<Result, Arg, State>(
      ThunkPrefix,
      [EndpointDesc](const Arg &arg, const ThunkApi<State> &api)
          -> func::AsyncResult<Result> {
        return func::AsyncChain::then<func::HttpResult<Result>, Result>(
            EndpointDesc.RequestBuilder(arg), unwrapEndpointResult<Result>);
      });
}

/**
 * Phase 8: Store Configuration
 * User Story: As a maintainer, I need this implementation note so I can understand which milestone behavior the surrounding code is preserving.
 */

template <typename State> struct EnhancedStore {
  std::shared_ptr<Store<State>> CoreStore;
  Dispatcher Dispatch;

  /**
   * Returns the current root-state snapshot.
   * User Story: As enhanced-store consumers, I need root-state access so
   * middleware, thunks, and selectors can inspect current runtime data.
   */
  const State &getState() const { return CoreStore->getState(); }

  /**
   * Registers a subscriber on the underlying core store.
   * User Story: As enhanced-store consumers, I need subscription support so
   * callers can react to state changes after middleware composition.
   */
  std::function<void()> subscribe(std::function<void()> Callback) {
    return CoreStore->subscribe(std::move(Callback));
  }

  /**
   * Dispatches a plain AnyAction through the enhanced middleware chain.
   * User Story: As enhanced-store consumers, I need action dispatch routed
   * through middleware so store behavior matches RTK-style semantics.
   */
  AnyAction dispatch(const AnyAction &action) const { return Dispatch(action); }

  /**
   * Dispatches a thunk using the enhanced dispatch and current getState accessors.
   * User Story: As thunk callers, I need thunk dispatch integrated with the
   * enhanced store so async flows can reuse middleware and state access.
   */
  template <typename Result>
  func::AsyncResult<Result>
  dispatch(const ThunkAction<Result, State> &thunk) const {
    auto dispatchAny = Dispatch;
    auto getState = [this]() -> State { return CoreStore->getState(); };
    return thunk(dispatchAny, getState);
  }
};

/**
 * Configures an enhanced store with middleware and preloaded state.
 * User Story: As runtime bootstrapping, I need a configured enhanced store so
 * reducers, middleware, and preload state come together in one helper.
 */
template <typename State>
EnhancedStore<State>
configureStore(CaseReducer<State> rootReducer, State preloadedState,
               const std::vector<Middleware<State>> &middlewares) {
  EnhancedStore<State> enhanced;
  auto coreStore = std::make_shared<Store<State>>(
      createCoreStore(std::move(preloadedState), std::move(rootReducer)));
  enhanced.CoreStore = coreStore;

  Dispatcher coreDispatch = [coreStore](const AnyAction &action) -> AnyAction {
    return coreStore->dispatch(action);
  };

  auto getState = [coreStore]() -> State { return coreStore->getState(); };

  enhanced.Dispatch =
      applyMiddleware<State>(coreDispatch, getState, middlewares);

  return enhanced;
}

/**
 * Configures an enhanced store without middleware.
 * User Story: As simple runtime bootstrapping, I need a no-middleware overload
 * so tests and small setups can create stores with less ceremony.
 */
template <typename State>
EnhancedStore<State> configureStore(CaseReducer<State> rootReducer,
                                    State preloadedState) {
  return configureStore<State>(rootReducer, preloadedState,
                               std::vector<Middleware<State>>());
}

} // namespace rtk

#endif // RTK_HPP
