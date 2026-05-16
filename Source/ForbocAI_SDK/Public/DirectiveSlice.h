#pragma once
/**
 * directive wire carries intent; static is not an excuse
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */

#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "Types.h"

namespace DirectiveSlice {

using namespace rtk;
using namespace func;

struct FDirectiveRunStartedPayload {
  FString Id;
  FString NpcId;
  FString Observation;
};

struct FDirectiveReceivedPayload {
  FString Id;
  FDirectiveResponse Response;
};

struct FContextComposedPayload {
  FString Id;
  FString Prompt;
  FCortexConfig Constraints;
};

struct FVerdictValidatedPayload {
  FString Id;
  FVerdictResponse Verdict;
};

struct FDirectiveRunFailedPayload {
  FString Id;
  FString Error;
};

/**
 * Extracts the stable entity id from a directive run.
 * User Story: As directive entity adapters, I need a single id selector so
 * runs can be indexed and updated by id consistently.
 */
inline FString DirectiveIdSelector(const FDirectiveRun &Run) { return Run.Id; }

/**
 * Returns the entity adapter used to manage directive runs.
 * User Story: As directive reducers and selectors, I need a shared adapter so
 * entity operations stay consistent across the slice.
 */
inline EntityAdapterOps<FDirectiveRun> GetDirectiveAdapter() {
  return createEntityAdapter<FDirectiveRun>(&DirectiveIdSelector);
}

struct FDirectiveSliceState {
  EntityState<FDirectiveRun> Entities;
  /**
   * User Story: As directive orchestration state, I need a selector for the
   * currently active directive run so downstream components can bind to it.
   * (From TS)
   */
  FString ActiveDirectiveId;

  FDirectiveSliceState() : Entities(GetDirectiveAdapter().getInitialState()) {}
};

namespace Actions {

/**
 * Returns the cached action creator for directive start events.
 * User Story: As directive orchestration, I need a stable start action creator
 * so thunks and reducers share one contract.
 */
inline const ActionCreator<FDirectiveRunStartedPayload> &
DirectiveRunStartedActionCreator() {
  static const ActionCreator<FDirectiveRunStartedPayload> ActionCreator =
      createAction<FDirectiveRunStartedPayload>(
          TEXT("directive/directiveRunStarted"));
  return ActionCreator;
}

/**
 * Returns the cached action creator for directive receipt events.
 * User Story: As directive processing, I need a reusable action creator so
 * received directive metadata is dispatched consistently.
 */
inline const ActionCreator<FDirectiveReceivedPayload> &
DirectiveReceivedActionCreator() {
  static const ActionCreator<FDirectiveReceivedPayload> ActionCreator =
      createAction<FDirectiveReceivedPayload>(
          TEXT("directive/directiveReceived"));
  return ActionCreator;
}

/**
 * Returns the cached action creator for composed context payloads.
 * User Story: As context composition flows, I need a stable action creator so
 * prompt and constraint updates reuse one contract.
 */
inline const ActionCreator<FContextComposedPayload> &
ContextComposedActionCreator() {
  static const ActionCreator<FContextComposedPayload> ActionCreator =
      createAction<FContextComposedPayload>(TEXT("directive/contextComposed"));
  return ActionCreator;
}

/**
 * Returns the cached action creator for validated verdict payloads.
 * User Story: As verdict validation flows, I need a reusable action creator so
 * verdict results are dispatched consistently.
 */
inline const ActionCreator<FVerdictValidatedPayload> &
VerdictValidatedActionCreator() {
  static const ActionCreator<FVerdictValidatedPayload> ActionCreator =
      createAction<FVerdictValidatedPayload>(
          TEXT("directive/verdictValidated"));
  return ActionCreator;
}

/**
 * Returns the cached action creator for directive failures.
 * User Story: As directive error handling, I need a reusable failure action
 * creator so broken runs are reported through one contract.
 */
inline const ActionCreator<FDirectiveRunFailedPayload> &
DirectiveRunFailedActionCreator() {
  static const ActionCreator<FDirectiveRunFailedPayload> ActionCreator =
      createAction<FDirectiveRunFailedPayload>(
          TEXT("directive/directiveRunFailed"));
  return ActionCreator;
}

/**
 * Returns the cached action creator for removing one NPC's directives.
 * User Story: As NPC teardown flows, I need a stable clear action creator so
 * directive runs are removed whenever an NPC is deleted.
 */
inline const ActionCreator<FString> &ClearDirectivesForNpcActionCreator() {
  static const ActionCreator<FString> ActionCreator =
      createAction<FString>(TEXT("directive/clearDirectivesForNpc"));
  return ActionCreator;
}

/**
 * Creates an action that records a new directive run.
 * User Story: As directive startup, I need each run captured so later steps can
 * update the right directive entity.
 */
inline AnyAction DirectiveRunStarted(const FString &Id, const FString &NpcId,
                                     const FString &Observation) {
  return DirectiveRunStartedActionCreator()(
      FDirectiveRunStartedPayload{Id, NpcId, Observation});
}

/**
 * Creates an action that stores a received directive response.
 * User Story: As directive processing, I need memory recall metadata stored so
 * later orchestration can use the server response.
 */
inline AnyAction DirectiveReceived(const FString &Id,
                                   const FDirectiveResponse &Response) {
  return DirectiveReceivedActionCreator()(
      FDirectiveReceivedPayload{Id, Response});
}

/**
 * Creates an action that stores composed context details.
 * User Story: As context composition, I need the prompt and constraints saved
 * so the run captures exactly what was sent to cortex.
 */
inline AnyAction ContextComposed(const FString &Id, const FString &Prompt,
                                 const FCortexConfig &Constraints) {
  return ContextComposedActionCreator()(
      FContextComposedPayload{Id, Prompt, Constraints});
}

/**
 * Creates an action that stores a validated verdict response.
 * User Story: As verdict handling, I need the validated verdict recorded so
 * reducers can mark the run as completed with final output.
 */
inline AnyAction VerdictValidated(const FString &Id,
                                  const FVerdictResponse &Verdict) {
  return VerdictValidatedActionCreator()(FVerdictValidatedPayload{Id, Verdict});
}

/**
 * Creates an action that records a directive run failure.
 * User Story: As directive error handling, I need the failure reason stored so
 * operators can inspect why a run stopped.
 */
inline AnyAction DirectiveRunFailed(const FString &Id, const FString &Error) {
  return DirectiveRunFailedActionCreator()(
      FDirectiveRunFailedPayload{Id, Error});
}

/**
 * Creates an action that removes directives for one NPC.
 * User Story: As NPC teardown flows, I need directive cleanup dispatched so
 * deleted NPCs do not leave stale run state behind.
 */
inline AnyAction ClearDirectivesForNpc(const FString &NpcId) {
  return ClearDirectivesForNpcActionCreator()(NpcId);
}

} // namespace Actions

/**
 * Builds the directive slice reducer and initial state.
 * User Story: As directive runtime setup, I need one slice factory so all
 * directive lifecycle actions share a consistent reducer.
 */
inline Slice<FDirectiveSliceState> CreateDirectiveSlice() {
  return buildSlice(sliceBuilder<FDirectiveSliceState>(TEXT("directive"),
                                                       FDirectiveSliceState()) |
                    addExtraCase(Actions::DirectiveRunStartedActionCreator(),
                    [](const FDirectiveSliceState &State,
                       const Action<FDirectiveRunStartedPayload> &Action)
                        -> FDirectiveSliceState {
                      FDirectiveSliceState Next = State;
                      FDirectiveRun Run;
                      Run.Id = Action.PayloadValue.Id;
                      Run.NpcId = Action.PayloadValue.NpcId;
                      Run.Observation = Action.PayloadValue.Observation;
                      Run.Status = EDirectiveStatus::Running;
                      Run.StartedAt = FDateTime::UtcNow().ToUnixTimestamp();
                      Next.Entities =
                          GetDirectiveAdapter().upsertOne(Next.Entities, Run);
                      Next.ActiveDirectiveId = Run.Id;
                      return Next;
                    }) |
                    addExtraCase(Actions::DirectiveReceivedActionCreator(),
                    [](const FDirectiveSliceState &State,
                       const Action<FDirectiveReceivedPayload> &Action)
                        -> FDirectiveSliceState {
                      FDirectiveSliceState Next = State;
                      const FDirectiveReceivedPayload &Payload =
                          Action.PayloadValue;
                      Next.Entities = GetDirectiveAdapter().updateOne(
                          Next.Entities, Payload.Id,
                          [Payload](const FDirectiveRun &Existing) {
                            FDirectiveRun Updated = Existing;
                            Updated.MemoryRecallQuery =
                                Payload.Response.MemoryRecall.Query;
                            Updated.MemoryRecallLimit =
                                Payload.Response.MemoryRecall.Limit;
                            Updated.MemoryRecallThreshold =
                                Payload.Response.MemoryRecall.Threshold;
                            return Updated;
                          });
                      return Next;
                    }) |
                    addExtraCase(Actions::ContextComposedActionCreator(),
                    [](const FDirectiveSliceState &State,
                       const Action<FContextComposedPayload> &Action)
                        -> FDirectiveSliceState {
                      FDirectiveSliceState Next = State;
                      const FContextComposedPayload &Payload =
                          Action.PayloadValue;
                      Next.Entities = GetDirectiveAdapter().updateOne(
                          Next.Entities, Payload.Id,
                          [Payload](const FDirectiveRun &Existing) {
                            FDirectiveRun Updated = Existing;
                            Updated.ContextPrompt = Payload.Prompt;
                            Updated.ContextConstraints = Payload.Constraints;
                            return Updated;
                          });
                      return Next;
                    }) |
                    addExtraCase(
          Actions::VerdictValidatedActionCreator(),
          [](const FDirectiveSliceState &State,
             const Action<FVerdictValidatedPayload> &Action)
              -> FDirectiveSliceState {
            FDirectiveSliceState Next = State;
            const FVerdictValidatedPayload &Payload = Action.PayloadValue;
            Next.Entities = GetDirectiveAdapter().updateOne(
                Next.Entities, Payload.Id,
                [Payload](const FDirectiveRun &Existing) {
                  FDirectiveRun Updated = Existing;
                  Updated.Status = EDirectiveStatus::Completed;
                  Updated.CompletedAt = FDateTime::UtcNow().ToUnixTimestamp();
                  Updated.bVerdictValid = Payload.Verdict.bValid;
                  Updated.VerdictDialogue = Payload.Verdict.Dialogue;
                  Updated.VerdictActionType = Payload.Verdict.bHasAction
                                                  ? Payload.Verdict.Action.Type
                                                  : TEXT("");
                  return Updated;
                });
            return Next;
          }) |
                    addExtraCase(Actions::DirectiveRunFailedActionCreator(),
                    [](const FDirectiveSliceState &State,
                       const Action<FDirectiveRunFailedPayload> &Action)
                        -> FDirectiveSliceState {
                      FDirectiveSliceState Next = State;
                      const FDirectiveRunFailedPayload &Payload =
                          Action.PayloadValue;
                      Next.Entities = GetDirectiveAdapter().updateOne(
                          Next.Entities, Payload.Id,
                          [Payload](const FDirectiveRun &Existing) {
                            FDirectiveRun Updated = Existing;
                            Updated.Status = EDirectiveStatus::Failed;
                            Updated.CompletedAt =
                                FDateTime::UtcNow().ToUnixTimestamp();
                            Updated.Error = Payload.Error;
                            return Updated;
                          });
                      return Next;
                    }) |
                    addExtraCase(
          Actions::ClearDirectivesForNpcActionCreator(),
          [](const FDirectiveSliceState &State,
             const Action<FString> &Action) -> FDirectiveSliceState {
            FDirectiveSliceState Next = State;
            const TArray<FDirectiveRun> Runs =
                GetDirectiveAdapter().getSelectors().selectAll(Next.Entities);
            struct CollectIds {
              static void apply(
                  const TArray<FDirectiveRun> &R,
                  const FString &NpcId,
                  TArray<FString> &Out,
                  int32 Idx) {
                Idx >= R.Num()
                    ? void()
                    : (R[Idx].NpcId == NpcId
                           ? (Out.Add(R[Idx].Id), void())
                           : void(),
                       apply(R, NpcId, Out, Idx + 1), void());
              }
            };
            TArray<FString> IdsToRemove;
            CollectIds::apply(Runs, Action.PayloadValue, IdsToRemove, 0);
            Next.Entities =
                GetDirectiveAdapter().removeMany(Next.Entities, IdsToRemove);
            IdsToRemove.Contains(Next.ActiveDirectiveId)
                ? (Next.ActiveDirectiveId.Empty(), void())
                : void();
            return Next;
          }));
}

/**
 * Returns the directive run with the requested id, if present.
 * User Story: As directive lookups, I need to resolve one run by id so
 * orchestration can update the correct directive entity.
 */
inline func::Maybe<FDirectiveRun> SelectDirectiveById(
    const FDirectiveSliceState &State, const FString &Id) {
  return GetDirectiveAdapter().getSelectors().selectById(State.Entities, Id);
}

/**
 * Returns every directive run currently stored in the slice.
 * User Story: As directive inspection, I need the full run collection so tools
 * and tests can review current directive state.
 */
inline TArray<FDirectiveRun> SelectAllDirectives(
    const FDirectiveSliceState &State) {
  return GetDirectiveAdapter().getSelectors().selectAll(State.Entities);
}

/**
 * Returns the id of the currently active directive run.
 * User Story: As directive UI binding, I need the active run id so views can
 * focus on the current directive turn.
 */
inline FString SelectActiveDirectiveId(const FDirectiveSliceState &State) {
  return State.ActiveDirectiveId;
}

/**
 * Returns the active directive run, if one is selected.
 * User Story: As directive orchestration, I need the active run resolved so
 * downstream steps can operate on the current directive entity.
 */
inline func::Maybe<FDirectiveRun> SelectActiveDirective(
    const FDirectiveSliceState &State) {
  return State.ActiveDirectiveId.IsEmpty()
      ? func::nothing<FDirectiveRun>()
      : SelectDirectiveById(State, State.ActiveDirectiveId);
}

} // namespace DirectiveSlice
