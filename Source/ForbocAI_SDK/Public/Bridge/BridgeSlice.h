#pragma once
/**
 * ᚱ bridge traffic should never hide who said what and when
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */

#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "Types.h"

namespace BridgeSlice {

using namespace rtk;
using namespace func;

struct FBridgeSliceState {
  TArray<FDirectiveRuleSet> ActivePresets;
  TArray<FDirectiveRuleSet> AvailableRulesets;
  TArray<FString> AvailablePresetIds;
  FValidationResult LastValidation;
  bool bHasLastValidation;
  FString Status;
  FString Error;

  FBridgeSliceState() : bHasLastValidation(false), Status(TEXT("idle")) {}
};

namespace Actions {

/**
 * Returns the memoized action creator for pending bridge validation.
 * User Story: As bridge validation flows, I need a cached pending action
 * creator so every caller dispatches the same start signal.
 */
inline const EmptyActionCreator &BridgeValidationPendingActionCreator() {
  static const EmptyActionCreator ActionCreator =
      createAction(TEXT("bridge/validationPending"));
  return ActionCreator;
}

/**
 * Returns the memoized action creator for successful bridge validation.
 * User Story: As bridge validation success handling, I need a cached action
 * creator so validated results enter state through one contract.
 */
inline const ActionCreator<FValidationResult> &
BridgeValidationSuccessActionCreator() {
  static const ActionCreator<FValidationResult> ActionCreator =
      createAction<FValidationResult>(TEXT("bridge/validationSuccess"));
  return ActionCreator;
}

/**
 * Returns the memoized action creator for failed bridge validation.
 * User Story: As bridge validation error handling, I need a cached failure
 * action creator so validation problems are surfaced consistently.
 */
inline const ActionCreator<FString> &BridgeValidationFailedActionCreator() {
  static const ActionCreator<FString> ActionCreator =
      createAction<FString>(TEXT("bridge/validationFailed"));
  return ActionCreator;
}

/**
 * Returns the memoized action creator for replacing active presets.
 * User Story: As bridge preset management, I need a cached action creator so
 * active preset lists can be replaced through one reducer contract.
 */
inline const ActionCreator<TArray<FDirectiveRuleSet>> &
SetActivePresetsActionCreator() {
  static const ActionCreator<TArray<FDirectiveRuleSet>> ActionCreator =
      createAction<TArray<FDirectiveRuleSet>>(TEXT("bridge/setActivePresets"));
  return ActionCreator;
}

/**
 * Returns the memoized action creator for adding one active preset.
 * User Story: As bridge preset management, I need a cached action creator so
 * one preset can be appended without rebuilding the full list.
 */
inline const ActionCreator<FDirectiveRuleSet> &AddActivePresetActionCreator() {
  static const ActionCreator<FDirectiveRuleSet> ActionCreator =
      createAction<FDirectiveRuleSet>(TEXT("bridge/addActivePreset"));
  return ActionCreator;
}

/**
 * Returns the memoized action creator for replacing available rulesets.
 * User Story: As bridge rules catalog updates, I need a cached action creator
 * so fetched rulesets can replace stale catalog data consistently.
 */
inline const ActionCreator<TArray<FDirectiveRuleSet>> &
SetAvailableRulesetsActionCreator() {
  static const ActionCreator<TArray<FDirectiveRuleSet>> ActionCreator =
      createAction<TArray<FDirectiveRuleSet>>(
          TEXT("bridge/setAvailableRulesets"));
  return ActionCreator;
}

/**
 * Returns the memoized action creator for replacing preset ids.
 * User Story: As preset discovery flows, I need a cached action creator so
 * available preset ids are updated through a shared contract.
 */
inline const ActionCreator<TArray<FString>> &
SetAvailablePresetIdsActionCreator() {
  static const ActionCreator<TArray<FString>> ActionCreator =
      createAction<TArray<FString>>(TEXT("bridge/setAvailablePresetIds"));
  return ActionCreator;
}

/**
 * Returns the memoized action creator for clearing validation state.
 * User Story: As bridge cleanup flows, I need a cached clear action creator so
 * stale validation results can be reset predictably.
 */
inline const EmptyActionCreator &ClearBridgeValidationActionCreator() {
  static const EmptyActionCreator ActionCreator =
      createAction(TEXT("bridge/clearBridgeValidation"));
  return ActionCreator;
}

/**
 * Builds the action that marks bridge validation as pending.
 * User Story: As bridge status tracking, I need a helper that dispatches the
 * pending action without manual action construction.
 */
inline AnyAction BridgeValidationPending() {
  return BridgeValidationPendingActionCreator()();
}

/**
 * Builds the action that records a successful validation result.
 * User Story: As bridge success handling, I need a helper so valid bridge
 * results can be stored with a single call.
 */
inline AnyAction BridgeValidationSuccess(const FValidationResult &Result) {
  return BridgeValidationSuccessActionCreator()(Result);
}

/**
 * Builds the action that records a validation failure message.
 * User Story: As bridge error handling, I need a helper so validation failures
 * can be dispatched without hand-assembling payloads.
 */
inline AnyAction BridgeValidationFailure(const FString &Error) {
  return BridgeValidationFailedActionCreator()(Error);
}

/**
 * Builds the action that replaces the active bridge presets.
 * User Story: As preset synchronization, I need a helper so the current active
 * preset list can be refreshed in one dispatch.
 */
inline AnyAction SetActivePresets(const TArray<FDirectiveRuleSet> &Presets) {
  return SetActivePresetsActionCreator()(Presets);
}

/**
 * Builds the action that appends an active bridge preset.
 * User Story: As preset editing flows, I need a helper so one preset can be
 * added to the active set without custom action wiring.
 */
inline AnyAction AddActivePreset(const FDirectiveRuleSet &Preset) {
  return AddActivePresetActionCreator()(Preset);
}

/**
 * Builds the action that replaces the available ruleset list.
 * User Story: As rules catalog refresh flows, I need a helper so fetched
 * rulesets replace the current catalog through one action.
 */
inline AnyAction
SetAvailableRulesets(const TArray<FDirectiveRuleSet> &Rulesets) {
  return SetAvailableRulesetsActionCreator()(Rulesets);
}

/**
 * Builds the action that replaces the available preset id list.
 * User Story: As preset discovery flows, I need a helper so available preset
 * ids can be refreshed through one action.
 */
inline AnyAction SetAvailablePresetIds(const TArray<FString> &PresetIds) {
  return SetAvailablePresetIdsActionCreator()(PresetIds);
}

/**
 * Builds the action that clears bridge validation state.
 * User Story: As bridge cleanup flows, I need a helper so old validation
 * results and errors can be cleared before the next run.
 */
inline AnyAction ClearBridgeValidation() {
  return ClearBridgeValidationActionCreator()();
}

} // namespace Actions

/**
 * Builds the bridge slice reducer and extra cases.
 * User Story: As bridge runtime setup, I need one slice factory so validation,
 * rulesets, and presets share a single reducer definition.
 */
inline Slice<FBridgeSliceState> CreateBridgeSlice() {
  return buildSlice(
      sliceBuilder<FBridgeSliceState>(TEXT("bridge"), FBridgeSliceState()) |
      addExtraCase(
          Actions::BridgeValidationPendingActionCreator(),
          [](const FBridgeSliceState &State,
             const Action<rtk::FEmptyPayload> &Action) -> FBridgeSliceState {
            FBridgeSliceState Next = State;
            Next.Status = TEXT("validating");
            Next.Error.Empty();
            return Next;
          })
      | addExtraCase(
          Actions::BridgeValidationSuccessActionCreator(),
          [](const FBridgeSliceState &State,
             const Action<FValidationResult> &Action) -> FBridgeSliceState {
            FBridgeSliceState Next = State;
            Next.Status = TEXT("idle");
            Next.LastValidation = Action.PayloadValue;
            Next.bHasLastValidation = true;
            return Next;
          })
      | addExtraCase(Actions::BridgeValidationFailedActionCreator(),
                    [](const FBridgeSliceState &State,
                       const Action<FString> &Action) -> FBridgeSliceState {
                      FBridgeSliceState Next = State;
                      Next.Status = TEXT("error");
                      Next.Error = Action.PayloadValue;
                      Next.LastValidation =
                          TypeFactory::Invalid(Action.PayloadValue);
                      Next.bHasLastValidation = true;
                      return Next;
                    })
      | addExtraCase(Actions::SetActivePresetsActionCreator(),
                    [](const FBridgeSliceState &State,
                       const Action<TArray<FDirectiveRuleSet>> &Action)
                        -> FBridgeSliceState {
                      FBridgeSliceState Next = State;
                      Next.ActivePresets = Action.PayloadValue;
                      return Next;
                    })
      | addExtraCase(
          Actions::AddActivePresetActionCreator(),
          [](const FBridgeSliceState &State,
             const Action<FDirectiveRuleSet> &Action) -> FBridgeSliceState {
            FBridgeSliceState Next = State;
            const FString TargetId = Action.PayloadValue.Id.IsEmpty()
                                         ? Action.PayloadValue.RulesetId
                                         : Action.PayloadValue.Id;
            return (Next.ActivePresets.IndexOfByPredicate(
                        [&TargetId](const FDirectiveRuleSet &Preset) {
                          const FString ExistingId =
                              Preset.Id.IsEmpty() ? Preset.RulesetId
                                                  : Preset.Id;
                          return ExistingId == TargetId;
                        }) == INDEX_NONE
                        ? (Next.ActivePresets.Add(Action.PayloadValue), void())
                        : void(),
                    Next);
          })
      | addExtraCase(Actions::SetAvailableRulesetsActionCreator(),
                    [](const FBridgeSliceState &State,
                       const Action<TArray<FDirectiveRuleSet>> &Action)
                        -> FBridgeSliceState {
                      FBridgeSliceState Next = State;
                      Next.AvailableRulesets = Action.PayloadValue;
                      return Next;
                    })
      | addExtraCase(
          Actions::SetAvailablePresetIdsActionCreator(),
          [](const FBridgeSliceState &State,
             const Action<TArray<FString>> &Action) -> FBridgeSliceState {
            FBridgeSliceState Next = State;
            Next.AvailablePresetIds = Action.PayloadValue;
            return Next;
          })
      | addExtraCase(
          Actions::ClearBridgeValidationActionCreator(),
          [](const FBridgeSliceState &State,
             const Action<rtk::FEmptyPayload> &Action) -> FBridgeSliceState {
            FBridgeSliceState Next = State;
            Next.LastValidation = FValidationResult();
            Next.bHasLastValidation = false;
            Next.Status = TEXT("idle");
            Next.Error.Empty();
            return Next;
          })
      );
}

} // namespace BridgeSlice
