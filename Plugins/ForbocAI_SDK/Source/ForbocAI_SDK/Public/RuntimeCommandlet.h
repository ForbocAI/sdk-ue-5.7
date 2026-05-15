#pragma once

#include "CLI/CLIModule.h"
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "Core/functional_core.hpp"
#include "TestGame/TestGameCommandSurface.h"
#include "RuntimeCommandlet.generated.h"

namespace CLITypes {
using func::AsyncResult;
using func::Either;
using func::TestResult;
using func::ValidationPipeline;
using func::make_left;
using func::make_right;
} // namespace CLITypes

/**
 * ForbocAI Commandlet.
 * User Story: As Unreal command-line automation, I need one commandlet entry
 * point so SDK operations can run from editor and CI command invocations.
 * Usage:
 *   ./UnrealEditorCmd -run=ForbocAI -Command=<CommandName> [Args...]
 *
 * Canonical commands:
 *   doctor
 *   npc_list
 *   npc_create -Persona="..."
 *   npc_process -Id="..." -Input="..."
 *   soul_export -Id="..."
 *   config_set -Key="..." -Value="..."
 *   config_get -Key="..."
 *   test_game -Mode=autoplay
 */
UCLASS()
class UForbocAICommandlet : public UCommandlet {
  GENERATED_BODY()

public:
  /**
   * Constructs the ForbocAI commandlet with its metadata defaults.
   * User Story: As editor command execution, I need the commandlet initialized
   * with predictable defaults before parsing CLI input.
   */
  UForbocAICommandlet();

  /**
   * Runs the requested CLI command from raw commandlet parameters.
   * User Story: As commandlet execution, I need raw params translated into a
   * command result so Unreal CLI entrypoints can drive SDK operations.
   */
  virtual int32 Main(const FString &Params) override;
  /**
   * Functional Core Type Aliases for Commandlet operations
   * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
   */
  using CommandResult = CLITypes::TestResult<void>;
  using CommandExecution = CLITypes::AsyncResult<void>;

  /**
   * Executes one validated CLI command with parsed arguments.
   * User Story: As command dispatch, I need a single execution entrypoint so
   * validated commands can be run uniformly.
   */
  CommandResult executeCommand(const FString &Command,
                               const TArray<FString> &Args);

  /**
   * Builds the async execution pipeline for one CLI command.
   * User Story: As command orchestration, I need an async pipeline wrapper so
   * CLI handlers can compose work without blocking the calling code directly.
   */
  CommandExecution createCommandPipeline(const FString &Command,
                                         const TArray<FString> &Args);

  /**
   * Builds the async execution pipeline for one CLI command, with an optional
   * injected test-game executor for deterministic harness testing.
   * User Story: As commandlet test coverage, I need the test-game command to
   * accept a stub executor so the CLI entrypoint can be validated without
   * depending on live runtime/network state.
   */
  CommandExecution createCommandPipeline(
      const FString &Command, const TArray<FString> &Args,
      const TestGame::CommandSurface::FCommandExecutor &TestGameExecutor);

  /**
   * Builds the validation pipeline for incoming CLI commands.
   * User Story: As command parsing, I need validation staged before execution
   * so invalid commands fail early with useful errors.
   */
  CLITypes::ValidationPipeline<FString, FString> commandValidationPipeline();
};
