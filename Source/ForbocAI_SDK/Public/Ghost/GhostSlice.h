#pragma once
/**
 * ᚷ ghost traffic stays traceable even when it feels supernatural
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */

#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "Types.h"

namespace GhostSlice {

using namespace rtk;

struct FGhostSessionStartedPayload {
  FString SessionId;
  FString Status;
};

struct FGhostSessionProgressPayload {
  FString SessionId;
  FString Status;
  float Progress;

  FGhostSessionProgressPayload() : Progress(0.0f) {}
};

struct FGhostSessionFailedPayload {
  FString SessionId;
  FString Error;
};

struct FGhostSliceState {
  FString ActiveSessionId;
  FString Status;
  float Progress;
  FGhostTestReport Results;
  bool bHasResults;
  TArray<FGhostHistoryEntry> History;
  bool bLoading;
  FString Error;

  FGhostSliceState()
      : Status(TEXT("idle")), Progress(0.0f), bHasResults(false),
        bLoading(false) {}
};

namespace Actions {

/**
 * Returns the cached action creator for ghost session start events.
 * User Story: As ghost session orchestration, I need a stable action creator
 * so reducers and middleware can reuse the same start contract.
 */
inline const ActionCreator<FGhostSessionStartedPayload> &
GhostSessionStartedActionCreator() {
  static const ActionCreator<FGhostSessionStartedPayload> ActionCreator =
      createAction<FGhostSessionStartedPayload>(TEXT("ghost/sessionStarted"));
  return ActionCreator;
}

/**
 * Returns the cached action creator for ghost session progress events.
 * User Story: As ghost progress tracking, I need a reusable action creator so
 * progress updates stay consistent across dispatch sites.
 */
inline const ActionCreator<FGhostSessionProgressPayload> &
GhostSessionProgressActionCreator() {
  static const ActionCreator<FGhostSessionProgressPayload> ActionCreator =
      createAction<FGhostSessionProgressPayload>(TEXT("ghost/sessionProgress"));
  return ActionCreator;
}

/**
 * Returns the cached action creator for completed ghost sessions.
 * User Story: As ghost reporting, I need a reusable completion action creator
 * so finished runs can be stored with one contract.
 */
inline const ActionCreator<FGhostTestReport> &
GhostSessionCompletedActionCreator() {
  static const ActionCreator<FGhostTestReport> ActionCreator =
      createAction<FGhostTestReport>(TEXT("ghost/sessionCompleted"));
  return ActionCreator;
}

/**
 * Returns the cached action creator for failed ghost sessions.
 * User Story: As ghost failure handling, I need a reusable failure action
 * creator so session errors can be reported consistently.
 */
inline const ActionCreator<FGhostSessionFailedPayload> &
GhostSessionFailedActionCreator() {
  static const ActionCreator<FGhostSessionFailedPayload> ActionCreator =
      createAction<FGhostSessionFailedPayload>(TEXT("ghost/sessionFailed"));
  return ActionCreator;
}

/**
 * Returns the cached action creator for loading ghost history.
 * User Story: As ghost history views, I need a stable action creator so prior
 * runs can be loaded without custom action wiring.
 */
inline const ActionCreator<TArray<FGhostHistoryEntry>> &
GhostHistoryLoadedActionCreator() {
  static const ActionCreator<TArray<FGhostHistoryEntry>> ActionCreator =
      createAction<TArray<FGhostHistoryEntry>>(TEXT("ghost/historyLoaded"));
  return ActionCreator;
}

/**
 * Returns the cached action creator for clearing ghost state.
 * User Story: As ghost session reset flows, I need one clear action creator so
 * teardown can restore the slice predictably.
 */
inline const EmptyActionCreator &ClearGhostSessionActionCreator() {
  static const EmptyActionCreator ActionCreator =
      createAction(TEXT("ghost/clearGhostSession"));
  return ActionCreator;
}

/**
 * Creates an action that opens a new ghost test session.
 * User Story: As ghost run startup, I need session metadata captured so the UI
 * and reducers know which run is active.
 */
inline AnyAction GhostSessionStarted(const FString &SessionId,
                                     const FString &Status = TEXT("running")) {
  return GhostSessionStartedActionCreator()(
      FGhostSessionStartedPayload{SessionId, Status});
}

/**
 * Creates an action that updates ghost session progress state.
 * User Story: As ghost progress reporting, I need each progress tick recorded
 * so observers can render current status and percentage.
 */
inline AnyAction GhostSessionProgress(const FString &SessionId,
                                      const FString &Status, float Progress) {
  FGhostSessionProgressPayload Payload;
  Payload.SessionId = SessionId;
  Payload.Status = Status;
  Payload.Progress = Progress;
  return GhostSessionProgressActionCreator()(Payload);
}

/**
 * Creates an action that stores a completed ghost test report.
 * User Story: As ghost result consumers, I need the finished report preserved
 * so results can be reviewed after execution.
 */
inline AnyAction GhostSessionCompleted(const FGhostTestReport &Report) {
  return GhostSessionCompletedActionCreator()(Report);
}

/**
 * Creates an action that stores a ghost session failure.
 * User Story: As ghost error handling, I need failed sessions recorded so the
 * UI can explain why a run stopped.
 */
inline AnyAction GhostSessionFailed(const FString &SessionId,
                                    const FString &Error) {
  return GhostSessionFailedActionCreator()(
      FGhostSessionFailedPayload{SessionId, Error});
}

/**
 * Creates an action that replaces the cached ghost history.
 * User Story: As history views, I need the latest run history loaded so users
 * can inspect recent ghost sessions.
 */
inline AnyAction GhostHistoryLoaded(const TArray<FGhostHistoryEntry> &History) {
  return GhostHistoryLoadedActionCreator()(History);
}

/**
 * Creates an action that resets ghost session state.
 * User Story: As cleanup flows, I need ghost state cleared so a new run starts
 * from a known baseline.
 */
inline AnyAction ClearGhostSession() {
  return ClearGhostSessionActionCreator()();
}

} // namespace Actions

/**
 * Builds the ghost slice reducer and initial state.
 * User Story: As ghost runtime setup, I need one slice factory so store
 * creation wires ghost actions and state transitions consistently.
 */
inline Slice<FGhostSliceState> CreateGhostSlice() {
  return buildSlice(sliceBuilder<FGhostSliceState>(TEXT("ghost"),
                                                   FGhostSliceState()) |
                    addExtraCase(Actions::GhostSessionStartedActionCreator(),
                    [](const FGhostSliceState &State,
                       const Action<FGhostSessionStartedPayload> &Action)
                        -> FGhostSliceState {
                      FGhostSliceState Next = State;
                      Next.ActiveSessionId = Action.PayloadValue.SessionId;
                      Next.Status = Action.PayloadValue.Status;
                      Next.Progress = 0.0f;
                      Next.bLoading = false;
                      Next.Error.Empty();
                      Next.bHasResults = false;
                      return Next;
                    }) |
                    addExtraCase(Actions::GhostSessionProgressActionCreator(),
                    [](const FGhostSliceState &State,
                       const Action<FGhostSessionProgressPayload> &Action)
                        -> FGhostSliceState {
                      FGhostSliceState Next = State;
                      Next.ActiveSessionId = Action.PayloadValue.SessionId;
                      Next.Status = Action.PayloadValue.Status;
                      Next.Progress = Action.PayloadValue.Progress;
                      return Next;
                    }) |
                    addExtraCase(
          Actions::GhostSessionCompletedActionCreator(),
          [](const FGhostSliceState &State,
             const Action<FGhostTestReport> &Action) -> FGhostSliceState {
            FGhostSliceState Next = State;
            Next.Results = Action.PayloadValue;
            Next.bHasResults = true;
            Next.Status = TEXT("completed");
            Next.Progress = 1.0f;
            Next.bLoading = false;
            return Next;
          }) |
                    addExtraCase(Actions::GhostSessionFailedActionCreator(),
                    [](const FGhostSliceState &State,
                       const Action<FGhostSessionFailedPayload> &Action)
                        -> FGhostSliceState {
                      FGhostSliceState Next = State;
                      Next.ActiveSessionId = Action.PayloadValue.SessionId;
                      Next.Status = TEXT("failed");
                      Next.bLoading = false;
                      Next.Error = Action.PayloadValue.Error;
                      return Next;
                    }) |
                    addExtraCase(Actions::GhostHistoryLoadedActionCreator(),
                    [](const FGhostSliceState &State,
                       const Action<TArray<FGhostHistoryEntry>> &Action)
                        -> FGhostSliceState {
                      FGhostSliceState Next = State;
                      Next.History = Action.PayloadValue;
                      return Next;
                    }) |
                    addExtraCase(
          Actions::ClearGhostSessionActionCreator(),
          [](const FGhostSliceState &State,
             const Action<rtk::FEmptyPayload> &Action) -> FGhostSliceState {
            return FGhostSliceState();
          }));
}

} // namespace GhostSlice
