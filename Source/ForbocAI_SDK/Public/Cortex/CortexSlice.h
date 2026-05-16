#pragma once

#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "Types.h"

namespace CortexSlice {

using namespace rtk;
using namespace func;

enum class ECortexEngineStatus : uint8 { Idle, Initializing, Ready, Error };

struct FCortexSliceState {
  ECortexEngineStatus Status;
  FCortexStatus EngineStatus;
  FString LastPrompt;
  FString LastResponseText;
  FString Error;
  /**
   * G2: Download progress tracking (mirrors TS setDownloadState)
   * User Story: As a maintainer, I need this implementation note so I can understand which milestone behavior the surrounding code is preserving.
   */
  bool bIsDownloading;
  float DownloadProgress;
  /**
   * G3: Embedding readiness tracking (mirrors TS embedderReady)
   * User Story: As a maintainer, I need this implementation note so I can understand which milestone behavior the surrounding code is preserving.
   */
  bool bEmbedderReady;
  /**
   * G7: Streaming state (mirrors TS stream.ts token-by-token)
   * User Story: As a maintainer, I need this implementation note so I can understand which milestone behavior the surrounding code is preserving.
   */
  bool bIsStreaming;
  FString StreamAccumulated;

  FCortexSliceState()
      : Status(ECortexEngineStatus::Idle), bIsDownloading(false),
        DownloadProgress(0.0f), bEmbedderReady(false), bIsStreaming(false) {}
};

namespace Actions {

/**
 * Returns the memoized action creator for cortex initialization start events.
 * User Story: As cortex reducers and tests, I need stable action creators so
 * initialization actions stay consistent across dispatch sites.
 */
inline const ActionCreator<FString> &CortexInitPendingActionCreator() {
  static const ActionCreator<FString> ActionCreator =
      createAction<FString>(TEXT("cortex/initPending"));
  return ActionCreator;
}

/**
 * Returns the memoized action creator for successful cortex initialization.
 * User Story: As cortex reducers and tests, I need stable action creators so
 * success handling can key off the same action contract everywhere.
 */
inline const ActionCreator<FCortexStatus> &CortexInitSuccessActionCreator() {
  static const ActionCreator<FCortexStatus> ActionCreator =
      createAction<FCortexStatus>(TEXT("cortex/initSuccess"));
  return ActionCreator;
}

/**
 * Returns the memoized action creator for failed cortex initialization.
 * User Story: As cortex reducers and tests, I need stable action creators so
 * initialization failures are handled consistently.
 */
inline const ActionCreator<FString> &CortexInitFailedActionCreator() {
  static const ActionCreator<FString> ActionCreator =
      createAction<FString>(TEXT("cortex/initFailed"));
  return ActionCreator;
}

/**
 * Returns the memoized action creator for cortex completion start events.
 * User Story: As cortex reducers and tests, I need stable action creators so
 * completion lifecycles stay consistent across dispatch sites.
 */
inline const ActionCreator<FString> &CortexCompletePendingActionCreator() {
  static const ActionCreator<FString> ActionCreator =
      createAction<FString>(TEXT("cortex/completePending"));
  return ActionCreator;
}

/**
 * Returns the memoized action creator for successful cortex completion.
 * User Story: As cortex reducers and tests, I need stable action creators so
 * completion results can be processed with one shared contract.
 */
inline const ActionCreator<FCortexResponse> &
CortexCompleteSuccessActionCreator() {
  static const ActionCreator<FCortexResponse> ActionCreator =
      createAction<FCortexResponse>(TEXT("cortex/completeSuccess"));
  return ActionCreator;
}

/**
 * Returns the memoized action creator for failed cortex completion.
 * User Story: As cortex reducers and tests, I need stable action creators so
 * completion failures stay explicit and predictable.
 */
inline const ActionCreator<FString> &CortexCompleteFailedActionCreator() {
  static const ActionCreator<FString> ActionCreator =
      createAction<FString>(TEXT("cortex/completeFailed"));
  return ActionCreator;
}

/**
 * Builds the action that marks cortex initialization as pending.
 * User Story: As cortex orchestration, I need typed action helpers so startup
 * state transitions stay explicit and easy to test.
 */
inline AnyAction CortexInitPending(const FString &ModelPath) {
  return CortexInitPendingActionCreator()(ModelPath);
}

/**
 * Builds the action that records a ready cortex status.
 * User Story: As cortex orchestration, I need typed action helpers so ready
 * state can be published without repeating payload wiring.
 */
inline AnyAction CortexInitFulfilled(const FCortexStatus &Status) {
  return CortexInitSuccessActionCreator()(Status);
}

/**
 * Builds the action that records a cortex initialization error.
 * User Story: As cortex orchestration, I need typed action helpers so startup
 * failures propagate through a predictable reducer path.
 */
inline AnyAction CortexInitRejected(const FString &Error) {
  return CortexInitFailedActionCreator()(Error);
}

/**
 * Builds the action that marks a completion request as pending.
 * User Story: As cortex orchestration, I need typed action helpers so request
 * progress is tracked before generation completes.
 */
inline AnyAction CortexCompletePending(const FString &Prompt) {
  return CortexCompletePendingActionCreator()(Prompt);
}

/**
 * Builds the action that records a completion response payload.
 * User Story: As cortex orchestration, I need typed action helpers so
 * generated output reaches reducers in a consistent shape.
 */
inline AnyAction CortexCompleteFulfilled(const FCortexResponse &Response) {
  return CortexCompleteSuccessActionCreator()(Response);
}

/**
 * Builds the action that records a cortex completion error.
 * User Story: As cortex orchestration, I need typed action helpers so failed
 * completions surface through the same reducer contract as successes.
 */
inline AnyAction CortexCompleteRejected(const FString &Error) {
  return CortexCompleteFailedActionCreator()(Error);
}

/**
 * G2: Download state action
 * User Story: As a maintainer, I need this implementation note so I can understand which milestone behavior the surrounding code is preserving.
 */
struct FDownloadState {
  bool bIsDownloading;
  float Progress;
};

/**
 * Returns the memoized action creator for cortex download state changes.
 * User Story: As cortex reducers and tests, I need stable action creators so
 * download progress wiring stays reusable and consistent.
 */
inline const ActionCreator<FDownloadState> &SetDownloadStateActionCreator() {
  static const ActionCreator<FDownloadState> ActionCreator =
      createAction<FDownloadState>(TEXT("cortex/setDownloadState"));
  return ActionCreator;
}

/**
 * Builds the action that updates download progress state.
 * User Story: As model download flows, I need a typed action helper so UI can
 * track progress without bespoke payload construction.
 */
inline AnyAction SetDownloadState(bool bIsDownloading, float Progress) {
  FDownloadState State;
  State.bIsDownloading = bIsDownloading;
  State.Progress = Progress;
  return SetDownloadStateActionCreator()(State);
}

/**
 * Returns the memoized action creator for embedder readiness changes.
 * User Story: As cortex reducers and tests, I need stable action creators so
 * embedder readiness can be updated through one shared contract.
 */
inline const ActionCreator<bool> &SetEmbedderReadyActionCreator() {
  static const ActionCreator<bool> ActionCreator =
      createAction<bool>(TEXT("cortex/setEmbedderReady"));
  return ActionCreator;
}

/**
 * Builds the action that updates embedder readiness.
 * User Story: As embedding startup flows, I need a typed action helper so the
 * slice can publish readiness without ad hoc bool payloads everywhere.
 */
inline AnyAction SetEmbedderReady(bool bReady) {
  return SetEmbedderReadyActionCreator()(bReady);
}

/**
 * Returns the memoized action creator for stream-start events.
 * User Story: As cortex reducers and tests, I need stable action creators so
 * streaming lifecycle actions remain consistent across producers.
 */
inline const ActionCreator<FString> &CortexStreamStartActionCreator() {
  static const ActionCreator<FString> ActionCreator =
      createAction<FString>(TEXT("cortex/streamStart"));
  return ActionCreator;
}

/**
 * Returns the memoized action creator for streamed token events.
 * User Story: As cortex reducers and tests, I need stable action creators so
 * streamed token updates can be accumulated predictably.
 */
inline const ActionCreator<FString> &CortexStreamTokenActionCreator() {
  static const ActionCreator<FString> ActionCreator =
      createAction<FString>(TEXT("cortex/streamToken"));
  return ActionCreator;
}

/**
 * Returns the memoized action creator for stream-complete events.
 * User Story: As cortex reducers and tests, I need stable action creators so
 * stream completion uses the same contract everywhere.
 */
inline const ActionCreator<FString> &CortexStreamCompleteActionCreator() {
  static const ActionCreator<FString> ActionCreator =
      createAction<FString>(TEXT("cortex/streamComplete"));
  return ActionCreator;
}

/**
 * Builds the action that marks streaming completion as started.
 * User Story: As streaming orchestration, I need typed action helpers so the
 * slice can begin token accumulation with a known prompt.
 */
inline AnyAction CortexStreamStart(const FString &Prompt) {
  return CortexStreamStartActionCreator()(Prompt);
}

/**
 * Builds the action that appends a streamed token.
 * User Story: As streaming orchestration, I need typed action helpers so each
 * token update is dispatched in a reducer-friendly shape.
 */
inline AnyAction CortexStreamToken(const FString &Token) {
  return CortexStreamTokenActionCreator()(Token);
}

/**
 * Builds the action that finalizes a streamed completion.
 * User Story: As streaming orchestration, I need typed action helpers so the
 * full generated text can close the stream consistently.
 */
inline AnyAction CortexStreamComplete(const FString &FullText) {
  return CortexStreamCompleteActionCreator()(FullText);
}

} // namespace Actions

/**
 * Builds the cortex slice reducer and extra cases.
 * User Story: As runtime store configuration, I need the cortex slice built in
 * one place so initialization, completion, and streaming state stay coherent.
 */
inline Slice<FCortexSliceState> CreateCortexSlice() {
  return buildSlice(
      sliceBuilder<FCortexSliceState>(TEXT("cortex"), FCortexSliceState()) |
      addExtraCase(
          Actions::CortexInitPendingActionCreator(),
          [](const FCortexSliceState &State,
             const Action<FString> &Action) -> FCortexSliceState {
            FCortexSliceState Next = State;
            Next.Status = ECortexEngineStatus::Initializing;
            Next.Error.Empty();
            Next.EngineStatus.Model = Action.PayloadValue;
            return Next;
          })
      | addExtraCase(
          Actions::CortexInitSuccessActionCreator(),
          [](const FCortexSliceState &State,
             const Action<FCortexStatus> &Action) -> FCortexSliceState {
            FCortexSliceState Next = State;
            Next.Status = ECortexEngineStatus::Ready;
            Next.EngineStatus = Action.PayloadValue;
            return Next;
          })
      | addExtraCase(
          Actions::CortexInitFailedActionCreator(),
          [](const FCortexSliceState &State,
             const Action<FString> &Action) -> FCortexSliceState {
            FCortexSliceState Next = State;
            Next.Status = ECortexEngineStatus::Error;
            Next.Error = Action.PayloadValue;
            Next.EngineStatus.Error = Action.PayloadValue;
            return Next;
          })
      | addExtraCase(
          Actions::CortexCompletePendingActionCreator(),
          [](const FCortexSliceState &State,
             const Action<FString> &Action) -> FCortexSliceState {
            FCortexSliceState Next = State;
            Next.LastPrompt = Action.PayloadValue;
            Next.Error.Empty();
            return Next;
          })
      | addExtraCase(
          Actions::CortexCompleteSuccessActionCreator(),
          [](const FCortexSliceState &State,
             const Action<FCortexResponse> &Action) -> FCortexSliceState {
            FCortexSliceState Next = State;
            Next.LastResponseText = Action.PayloadValue.Text;
            return Next;
          })
      | addExtraCase(
          Actions::CortexCompleteFailedActionCreator(),
          [](const FCortexSliceState &State,
             const Action<FString> &Action) -> FCortexSliceState {
            FCortexSliceState Next = State;
            Next.Error = Action.PayloadValue;
            return Next;
          })
      /**
       * G2: Download state reducer
       * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
       */
      | addExtraCase(
          Actions::SetDownloadStateActionCreator(),
          [](const FCortexSliceState &State,
             const Action<Actions::FDownloadState> &Action) -> FCortexSliceState {
            FCortexSliceState Next = State;
            Next.bIsDownloading = Action.PayloadValue.bIsDownloading;
            Next.DownloadProgress = Action.PayloadValue.Progress;
            return Next;
          })
      /**
       * G3: Embedder readiness reducer
       * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
       */
      | addExtraCase(
          Actions::SetEmbedderReadyActionCreator(),
          [](const FCortexSliceState &State,
             const Action<bool> &Action) -> FCortexSliceState {
            FCortexSliceState Next = State;
            Next.bEmbedderReady = Action.PayloadValue;
            return Next;
          })
      /**
       * G7: Streaming reducers
       * User Story: As a maintainer, I need this implementation note so I can understand which milestone behavior the surrounding code is preserving.
       */
      | addExtraCase(
          Actions::CortexStreamStartActionCreator(),
          [](const FCortexSliceState &State,
             const Action<FString> &Action) -> FCortexSliceState {
            FCortexSliceState Next = State;
            Next.bIsStreaming = true;
            Next.StreamAccumulated.Empty();
            Next.LastPrompt = Action.PayloadValue;
            Next.Error.Empty();
            return Next;
          })
      | addExtraCase(
          Actions::CortexStreamTokenActionCreator(),
          [](const FCortexSliceState &State,
             const Action<FString> &Action) -> FCortexSliceState {
            FCortexSliceState Next = State;
            Next.StreamAccumulated += Action.PayloadValue;
            return Next;
          })
      | addExtraCase(
          Actions::CortexStreamCompleteActionCreator(),
          [](const FCortexSliceState &State,
             const Action<FString> &Action) -> FCortexSliceState {
            FCortexSliceState Next = State;
            Next.bIsStreaming = false;
            Next.LastResponseText = Action.PayloadValue;
            Next.StreamAccumulated.Empty();
            return Next;
          })
      );
}

} // namespace CortexSlice
