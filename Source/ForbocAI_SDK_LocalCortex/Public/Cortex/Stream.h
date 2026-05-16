#pragma once
/**
 * G7: Stream helpers — mirrors TS stream.ts
 * Convenience free functions for consuming streaming cortex completions
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */

#include "Core/functional_core.hpp"
#include "Cortex/CortexModule.h"
#include "CoreMinimal.h"

namespace StreamHelpers {

using FOnChunk = std::function<void(const FString &Chunk)>;
using FOnComplete = std::function<void(const FString &FullText)>;
using FOnError = std::function<void(const FString &Error)>;

/**
 * Stream tokens from a cortex with a per-chunk callback.
 * Mirrors TS `streamFromCortex(cortex, prompt, onChunk, options)`.
 * Returns a Future resolving to the full response.
 * User Story: As an SDK integrator, I need this type or module note so I can understand the role of the surrounding API surface quickly.
 */
inline TFuture<CortexTypes::CortexCompletionResult>
StreamFromCortex(const FCortex &Cortex, const FString &Prompt,
                 const FOnChunk &OnChunk) {
  return CortexOps::CompleteStream(Cortex, Prompt, OnChunk);
}

/**
 * Stream tokens from a cortex with completion and error callbacks.
 * Mirrors TS `streamToCallback` + error handling pattern.
 * Fire-and-forget: callbacks run on the game thread.
 * User Story: As a maintainer, I need this note so the surrounding API intent stays clear during maintenance and integration.
 */
inline void StreamFromCortexWithCallbacks(const FCortex &Cortex,
                                          const FString &Prompt,
                                          const FOnChunk &OnChunk,
                                          const FOnComplete &OnComplete,
                                          const FOnError &OnError) {
  auto Future = CortexOps::CompleteStream(Cortex, Prompt, OnChunk);
  Async(EAsyncExecution::Thread, [Future = MoveTemp(Future), OnComplete,
                                   OnError]() mutable {
    auto Result = Future.Get();
    AsyncTask(ENamedThreads::GameThread, [Result, OnComplete, OnError]() {
      Result.isLeft
          ? (OnError ? (OnError(Result.left), void()) : void())
          : (OnComplete ? (OnComplete(Result.right.Text), void()) : void());
    });
  });
}

} // namespace StreamHelpers
