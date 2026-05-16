#include "Ghost/GhostModule.h"
#include "Ghost/GhostModuleInternal.h"
#include "Ghost/GhostSlice.h"
#include "Ghost/GhostThunks.h"
#include "Core/functional_core.hpp"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NPC/NPCModule.h"
#include "RuntimeStore.h"
#include "Serialization/JsonSerializer.h"

/**
 * Ghost Module Implementation — Strict FP (Reduced)
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */

FGhost GhostOps::Create(const FGhostConfig &Config) {
  FGhost Ghost;
  Ghost.Config = Config;
  Ghost.bInitialized = true;
  return Ghost;
}

/**
 * Single test — G.6: dispatch runLocalGhostTestThunk
 * User Story: As a maintainer, I need this implementation note so I can understand which milestone behavior the surrounding code is preserving.
 */
GhostTypes::GhostTestRunResult GhostOps::RunTest(const FGhost &Ghost,
                                                 const FString &Scenario) {
  return !Ghost.bInitialized
             ? GhostTypes::AsyncResult<FGhostTestResult>::create(
                   [](std::function<void(FGhostTestResult)>,
                      std::function<void(std::string)> Reject) {
                     Reject("Ghost not initialized");
                   })
             : ConfigureStore().dispatch(
                   rtk::runLocalGhostTestThunk(Ghost.Config.Agent, Scenario));
}

/**
 * All tests implementation (Async)
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
GhostTypes::GhostTestRunAllResult GhostOps::RunAllTests(const FGhost &Ghost) {
  return GhostTypes::AsyncResult<FGhostTestReport>::create(
      [Ghost](std::function<void(FGhostTestReport)> resolve,
              std::function<void(std::string)> reject) {
        auto Report = MakeShared<FGhostTestReport>();
        Report->Config = Ghost.Config;

        GhostInternal::RunTestsSequentially(
            Ghost, Report, 0,
            [resolve](FGhostTestReport FinalReport) {
              auto Store = ConfigureStore();
              Store.dispatch(
                  GhostSlice::Actions::GhostSessionCompleted(FinalReport));
              resolve(FinalReport);
            },
            [reject](std::string Error) {
              auto Store = ConfigureStore();
              Store.dispatch(GhostSlice::Actions::GhostSessionFailed(
                  TEXT("ghost-run"), FString(Error.c_str())));
              reject(Error);
            });
      });
}

/**
 * Configuration validation implementation
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
GhostTypes::GhostValidationResult
GhostOps::ValidateConfig(const FGhostConfig &Config) {
  return GhostInternal::ValidateTestConfig(Config);
}

/**
 * Summary generation implementation
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
GhostTypes::Either<FString, FString>
GhostOps::GenerateSummary(const FGhostTestReport &Report) {
  return GhostInternal::GenerateTestSummary(Report);
}

/**
 * JSON export implementation
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
GhostTypes::Either<FString, FString>
GhostOps::ExportToJson(const FGhostTestReport &Report) {
  return GhostInternal::ExportResultsToJson(Report);
}

/**
 * CSV export implementation
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
GhostTypes::Either<FString, FString>
GhostOps::ExportToCsv(const FGhostTestReport &Report) {
  return GhostInternal::ExportResultsToCsv(Report);
}
