#pragma once

#include "Async/Async.h"
#include "Core/functional_core.hpp"
#include "CoreMinimal.h"
#include "Types.h"

/**
 * Functional Core Type Aliases for Cortex operations
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
namespace CortexTypes {
using func::AsyncResult;
using func::ConfigBuilder;
using func::Curried;
using func::Either;
using func::just;
using func::Lazy;
using func::make_left;
using func::make_right;
using func::Maybe;
using func::nothing;
using func::Pipeline;
using func::TestResult;
using func::ValidationPipeline;

/**
 * Type aliases for Cortex operations
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
using CortexCreationResult = Either<FString, FCortex>;
using CortexInitResult = Either<FString, bool>;
using CortexCompletionResult = Either<FString, FCortexResponse>;
using CortexStreamResult = Either<FString, TArray<FString>>;
} // namespace CortexTypes

/**
 * Cortex Operations — Stateless free functions.
 * User Story: As an SDK integrator, I need this type or module note so I can understand the role of the surrounding API surface quickly.
 */
namespace CortexOps {

/**
 * Creates a cortex instance from configuration.
 * User Story: As local inference setup, I need a pure cortex factory so model
 * configuration can be turned into a runtime value consistently.
 * @param Config The configuration to use.
 * @return A new Cortex instance.
 */
FORBOCAI_SDK_API FCortex Create(const FCortexConfig &Config);

/**
 * Initializes the cortex engine and loads its model asynchronously.
 * User Story: As local inference startup, I need initialization to load the
 * model before completions can be requested.
 * @param Cortex The Cortex instance to initialize.
 * @return A validation result indicating success or failure.
 */
FORBOCAI_SDK_API TFuture<CortexTypes::CortexInitResult> Init(FCortex &Cortex);

/**
 * Generates a text completion from a prompt.
 * User Story: As inference callers, I need a completion function so prompts
 * can be run against the local cortex with optional context.
 * @param Cortex The Cortex instance to use.
 * @param Prompt The input prompt text.
 * @param Context Optional context data.
 * @return The generated response as a `TFuture`.
 */
FORBOCAI_SDK_API TFuture<CortexTypes::CortexCompletionResult>
Complete(const FCortex &Cortex, const FString &Prompt,
         const TMap<FString, FString> &Context = TMap<FString, FString>());

/**
 * Per-token callback type for streaming inference.
 * User Story: As an SDK integrator, I need this type or module note so I can understand the role of the surrounding API surface quickly.
 */
using FOnCortexToken = std::function<void(const FString &Token)>;

/**
 * Streams text generation token by token via callback.
 * User Story: As streaming inference callers, I need token callbacks so UI and
 * gameplay systems can react while generation is in progress.
 * @param Cortex The Cortex instance to use.
 * @param Prompt The input prompt text.
 * @param OnToken Called for each generated token on the game thread.
 * @param Context Optional context data.
 * @return Future resolving to the full accumulated response.
 */
FORBOCAI_SDK_API TFuture<CortexTypes::CortexCompletionResult>
CompleteStream(const FCortex &Cortex, const FString &Prompt,
               const FOnCortexToken &OnToken,
               const TMap<FString, FString> &Context = TMap<FString, FString>());

/**
 * Gets the current status of the cortex engine.
 * User Story: As runtime diagnostics, I need cortex status so tooling can show
 * whether local inference is idle, loading, or failed.
 * @param Cortex The Cortex instance to check.
 * @return The current status information.
 */
FORBOCAI_SDK_API FString GetStatus(const FCortex &Cortex);

/**
 * Cleans up resources and shuts down the engine.
 * User Story: As inference shutdown, I need cortex cleanup so native resources
 * are released when local inference is no longer needed.
 * @param Cortex The Cortex instance to clean up.
 */
FORBOCAI_SDK_API void Shutdown(FCortex &Cortex);

} // namespace CortexOps

/**
 * Cortex Helpers (Functional)
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
namespace CortexHelpers {
/**
 * Creates a lazy cortex factory from configuration.
 * User Story: As deferred inference setup, I need lazy construction so cortex
 * creation can be postponed until the runtime actually needs it.
 */
inline CortexTypes::Lazy<FCortex>
createLazyCortex(const FCortexConfig &config) {
  return func::lazy(
      [config]() -> FCortex { return CortexOps::Create(config); });
}

/**
 * Builds the validation pipeline for cortex configuration.
 * User Story: As cortex config validation, I need one reusable pipeline so bad
 * model paths or generation settings fail before startup.
 */
inline CortexTypes::ValidationPipeline<FCortexConfig, FString>
cortexConfigValidationPipeline() {
  return func::validationPipeline<FCortexConfig, FString>() |
         [](const FCortexConfig &config)
             -> CortexTypes::Either<FString, FCortexConfig> {
           return config.Model.IsEmpty()
                      ? CortexTypes::make_left(
                            FString(TEXT("Model cannot be empty")),
                            FCortexConfig{})
                      : CortexTypes::make_right(FString(), config);
         } |
         [](const FCortexConfig &config)
             -> CortexTypes::Either<FString, FCortexConfig> {
           return (config.MaxTokens < 1 || config.MaxTokens > 2048)
                      ? CortexTypes::make_left(
                            FString(TEXT("Max tokens must be between 1 and 2048")),
                            FCortexConfig{})
                      : CortexTypes::make_right(FString(), config);
         } |
         [](const FCortexConfig &config)
             -> CortexTypes::Either<FString, FCortexConfig> {
           return (config.Temperature < 0.0f || config.Temperature > 2.0f)
                      ? CortexTypes::make_left(
                            FString(TEXT("Temperature must be between 0.0 and 2.0")),
                            FCortexConfig{})
                      : CortexTypes::make_right(FString(), config);
         };
}

/**
 * Builds the pipeline wrapper for cortex completion composition.
 * User Story: As functional cortex helpers, I need a pipe-ready cortex value so
 * later completion steps can compose around it.
 */
inline CortexTypes::Pipeline<FCortex>
cortexCompletionPipeline(const FCortex &cortex) {
  return func::pipe(cortex);
}

/**
 * Builds the curried cortex creation helper.
 * User Story: As functional cortex construction, I need a curried creator so
 * cortex creation can participate in composable pipelines.
 */
typedef decltype(func::curry<1>(
    std::function<CortexTypes::CortexCreationResult(FCortexConfig)>()))
    FCurriedCortexCreation;

inline FCurriedCortexCreation curriedCortexCreation() {
  std::function<CortexTypes::CortexCreationResult(FCortexConfig)> Creator =
      [](FCortexConfig config) -> CortexTypes::CortexCreationResult {
    try {
      FCortex cortex = CortexOps::Create(config);
      return CortexTypes::make_right(FString(), cortex);
    } catch (const std::exception &e) {
      return CortexTypes::make_left(FString(e.what()), FCortex{});
    }
  };
  return func::curry<1>(Creator);
}
} // namespace CortexHelpers

/**
 * Factory namespace for consistency with other modules
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
namespace CortexFactory {
/**
 * Creates a cortex value through the shared factory surface.
 * User Story: As callers standardizing module access, I need a factory wrapper
 * so cortex creation follows the same shape as other SDK modules.
 */
inline FCortex Create(const FCortexConfig &Config) {
  return CortexOps::Create(Config);
}
} // namespace CortexFactory
