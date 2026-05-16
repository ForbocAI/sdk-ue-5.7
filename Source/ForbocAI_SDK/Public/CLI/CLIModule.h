#pragma once

#include "CoreMinimal.h"
#include "Core/functional_core.hpp"

/**
 * CLI Module — Pure command router.
 * Parses command keys and delegates to Ops functions.
 * No business logic lives here — all operations go through the store.
 * Mirrors TS cli.ts which routes to sdkOps.ts.
 * User Story: As an SDK integrator, I need this type or module note so I can understand the role of the surrounding API surface quickly.
 */
namespace CLIOps {

/**
 * Dispatches a CLI command through the SDK store.
 * @param CommandKey  Command in "domain_action" format (e.g. "npc_create",
 *                    "memory_list", "ghost_run", "soul_export").
 * @param Args        Positional arguments for the command.
 * @return TestResult indicating success or failure with message.
 * User Story: As an SDK integrator, I need this type or module note so I can understand the role of the surrounding API surface quickly.
 */
FORBOCAI_SDK_API func::TestResult<void>
DispatchCommand(const FString &CommandKey, const TArray<FString> &Args);

} // namespace CLIOps
