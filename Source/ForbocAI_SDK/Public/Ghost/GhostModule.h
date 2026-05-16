#pragma once

#include "NPC/NPCModule.h"
#include "Core/functional_core.hpp"
#include "CoreMinimal.h"
#include "Types.h"

/**
 * Functional Core Type Aliases for Ghost operations
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
namespace GhostTypes {
using func::AsyncResult;
using func::ConfigBuilder;
using func::Curried;
using func::Either;
using func::Lazy;
using func::Maybe;
using func::Pipeline;
using func::TestResult;
using func::ValidationPipeline;

using func::just;
using func::make_left;
using func::make_right;
using func::nothing;

/**
 * Type aliases for Ghost operations
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
using GhostCreationResult = Either<FString, FGhost>;
using GhostTestRunResult = AsyncResult<FGhostTestResult>;
using GhostTestRunAllResult = AsyncResult<FGhostTestReport>;
using GhostValidationResult = Either<FString, FGhostConfig>;
} // namespace GhostTypes

/**
 * Ghost Operations — Stateless free functions.
 * User Story: As SDK QA flows, I need a single namespace for ghost operations
 * so test creation, execution, and export stay discoverable and consistent.
 */
namespace GhostOps {

/**
 * Creates a ghost test instance from configuration.
 * User Story: As automated QA setup, I need a pure ghost factory so test runs
 * can start from validated configuration without hidden side effects.
 * @param Config The test configuration.
 * @return A new Ghost test instance.
 */
FORBOCAI_SDK_API FGhost Create(const FGhostConfig &Config);

/**
 * Runs a single ghost test scenario.
 * User Story: As scenario-level QA, I need one run function so a specific
 * behavior script can be exercised and measured independently.
 * @param Ghost The Ghost test instance.
 * @param Scenario The scenario to test.
 * @return The async test result.
 */
FORBOCAI_SDK_API GhostTypes::GhostTestRunResult
RunTest(const FGhost &Ghost, const FString &Scenario);

/**
 * Runs all configured ghost test scenarios.
 * User Story: As full-suite QA, I need one function that executes every
 * scenario so I can collect a complete ghost report in one call.
 * @param Ghost The Ghost test instance.
 * @return The complete test report (async).
 */
FORBOCAI_SDK_API GhostTypes::GhostTestRunAllResult
RunAllTests(const FGhost &Ghost);

/**
 * Validates the ghost test configuration.
 * User Story: As ghost configuration checks, I need validation before execution
 * so invalid scenarios fail before a test run starts.
 * @param Config The test configuration to validate.
 * @return The validation result.
 */
FORBOCAI_SDK_API GhostTypes::GhostValidationResult
ValidateConfig(const FGhostConfig &Config);

/**
 * Generates a summary string from a ghost report.
 * User Story: As report consumers, I need a compact summary so I can review
 * the overall ghost outcome without parsing the full report payload.
 * @param Report The test report.
 * @return The summary string or error.
 */
FORBOCAI_SDK_API GhostTypes::Either<FString, FString>
GenerateSummary(const FGhostTestReport &Report);

/**
 * Exports ghost test results to JSON.
 * User Story: As report export flows, I need JSON serialization so ghost
 * results can be saved or transmitted to other tools.
 * @param Report The test report.
 * @return The JSON string or error.
 */
FORBOCAI_SDK_API GhostTypes::Either<FString, FString>
ExportToJson(const FGhostTestReport &Report);

/**
 * Exports ghost test results to CSV.
 * User Story: As report export flows, I need CSV output so results can be
 * reviewed in spreadsheets and external QA tooling.
 * @param Report The test report.
 * @return The CSV string or error.
 */
FORBOCAI_SDK_API GhostTypes::Either<FString, FString>
ExportToCsv(const FGhostTestReport &Report);

} // namespace GhostOps

/**
 * Factory namespace for consistency with other modules
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
namespace GhostFactory {
/**
 * Creates a ghost value from configuration through GhostOps.
 * User Story: As callers standardizing module access, I need a factory wrapper
 * so ghost construction follows the same pattern as other SDK modules.
 */
inline FGhost Create(const FGhostConfig &Config) {
  return GhostOps::Create(Config);
}
} // namespace GhostFactory

/**
 * GhostHelpers moved to end of file to ensure type visibility
 * Functional Core Helper Functions for Ghost operations
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
namespace GhostHelpers {
/**
 * Creates a lazy ghost factory that defers GhostOps construction.
 * User Story: As deferred ghost setup, I need lazy construction so expensive
 * ghost creation can be postponed until the value is required.
 */
inline GhostTypes::Lazy<FGhost> createLazyGhost(const FGhostConfig &config) {
  return func::lazy([config]() -> FGhost { return GhostOps::Create(config); });
}

/**
 * Builds the validation pipeline for ghost configuration.
 * User Story: As ghost config validation, I need one reusable pipeline so
 * invalid agent or scenario data fails before test execution starts.
 */
inline GhostTypes::ValidationPipeline<FGhostConfig, FString>
ghostConfigValidationPipeline() {
  return func::validationPipeline<FGhostConfig, FString>() |
         [](const FGhostConfig &config)
             -> GhostTypes::Either<FString, FGhostConfig> {
           return (config.Agent.Id.IsEmpty() || config.Agent.Persona.IsEmpty())
                      ? GhostTypes::make_left(
                            FString(TEXT("Agent must have valid Id and Persona")),
                            FGhostConfig{})
                      : GhostTypes::make_right(FString(), config);
         } |
         [](const FGhostConfig &config)
             -> GhostTypes::Either<FString, FGhostConfig> {
           return config.Scenarios.Num() == 0
                      ? GhostTypes::make_left(
                            FString(TEXT("At least one test scenario must be provided")),
                            FGhostConfig{})
                      : GhostTypes::make_right(FString(), config);
         } |
         [](const FGhostConfig &config)
             -> GhostTypes::Either<FString, FGhostConfig> {
           return config.MaxIterations < 1
                      ? GhostTypes::make_left(
                            FString(TEXT("Max iterations must be at least 1")),
                            FGhostConfig{})
                      : GhostTypes::make_right(FString(), config);
         };
}

/**
 * Builds the pipeline wrapper for ghost test execution.
 * User Story: As functional ghost helpers, I need a pipe-ready value so
 * downstream composition can extend ghost execution ergonomically.
 */
inline GhostTypes::Pipeline<FGhost> ghostTestPipeline(const FGhost &ghost) {
  return func::pipe(ghost);
}

/**
 * Builds the curried ghost creation helper.
 * User Story: As functional construction flows, I need a curried creator so
 * ghost initialization can participate in composable pipelines.
 */
typedef decltype(func::curry<1>(
    std::function<GhostTypes::GhostCreationResult(FGhostConfig)>()))
    FCurriedGhostCreation;

inline FCurriedGhostCreation curriedGhostCreation() {
  std::function<GhostTypes::GhostCreationResult(FGhostConfig)> Creator =
      [](FGhostConfig config) -> GhostTypes::GhostCreationResult {
    try {
      FGhost ghost = GhostOps::Create(config);
      return GhostTypes::make_right(FString(), ghost);
    } catch (const std::exception &e) {
      return GhostTypes::make_left(FString(e.what()), FGhost{});
    }
  };
  return func::curry<1>(Creator);
}
} // namespace GhostHelpers
