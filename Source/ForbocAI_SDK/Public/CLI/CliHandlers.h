#pragma once

#include "CoreMinimal.h"
#include "Core/functional_core.hpp"
#include "RuntimeStore.h"

/**
 * Per-domain CLI command handlers.
 * User Story: As CLI command routing, I need handlers grouped by domain so
 * each command family can be resolved without one monolithic dispatcher.
 * Each handler returns Maybe<TestResult<void>>:
 *   - just(result) when the command belongs to that domain.
 *   - nothing() when the command is not for that domain.
 *
 * F.1 — Split monolithic CLIModule into domain-specific handlers.
 * All dispatch through store thunks via Ops; no raw module calls.
 */
namespace CLIOps {
namespace Handlers {

using Result = func::TestResult<void>;
using HandlerResult = func::Maybe<Result>;

/**
 * Handles system-scoped CLI commands such as version and doctor.
 * User Story: As CLI users, I need system commands routed together so runtime
 * health and version checks are dispatched through one handler.
 */
HandlerResult HandleSystem(rtk::EnhancedStore<FStoreState> &Store,
                          const FString &CommandKey,
                          const TArray<FString> &Args);

/**
 * Handles NPC lifecycle and interaction CLI commands.
 * User Story: As CLI users, I need NPC commands grouped under one handler so
 * agent creation, updates, and chat share consistent routing.
 */
HandlerResult HandleNpc(rtk::EnhancedStore<FStoreState> &Store,
                       const FString &CommandKey,
                       const TArray<FString> &Args);

/**
 * Handles memory-related CLI commands.
 * User Story: As CLI users, I need memory commands routed together so store,
 * recall, and clear operations share consistent parsing.
 */
HandlerResult HandleMemory(rtk::EnhancedStore<FStoreState> &Store,
                          const FString &CommandKey,
                          const TArray<FString> &Args);

/**
 * Handles cortex initialization and completion CLI commands.
 * User Story: As CLI users, I need cortex commands grouped so model setup and
 * completion flows route through one handler.
 */
HandlerResult HandleCortex(rtk::EnhancedStore<FStoreState> &Store,
                          const FString &CommandKey,
                          const TArray<FString> &Args);

/**
 * Handles ghost QA CLI commands.
 * User Story: As CLI users, I need ghost commands grouped so run control and
 * history lookups share one command handler.
 */
HandlerResult HandleGhost(rtk::EnhancedStore<FStoreState> &Store,
                         const FString &CommandKey,
                         const TArray<FString> &Args);

/**
 * Handles bridge validation and ruleset CLI commands.
 * User Story: As CLI users, I need bridge commands grouped so validation and
 * ruleset management share consistent dispatch logic.
 */
HandlerResult HandleBridge(rtk::EnhancedStore<FStoreState> &Store,
                          const FString &CommandKey,
                          const TArray<FString> &Args);

/**
 * Handles soul import, export, and verification CLI commands.
 * User Story: As CLI users, I need soul commands grouped so lifecycle actions
 * on souls share one routing surface.
 */
HandlerResult HandleSoul(rtk::EnhancedStore<FStoreState> &Store,
                        const FString &CommandKey,
                        const TArray<FString> &Args);

/**
 * Handles runtime configuration CLI commands.
 * User Story: As CLI users, I need config commands grouped so reads and writes
 * of SDK configuration use one handler.
 */
HandlerResult HandleConfig(rtk::EnhancedStore<FStoreState> &Store,
                          const FString &CommandKey,
                          const TArray<FString> &Args);

/**
 * Handles vector-store setup CLI commands.
 * User Story: As CLI users, I need vector initialization routed separately so
 * local infrastructure setup has a focused handler.
 */
HandlerResult HandleVector(rtk::EnhancedStore<FStoreState> &Store,
                          const FString &CommandKey,
                          const TArray<FString> &Args);

/**
 * Handles dependency setup CLI commands.
 * User Story: As CLI users, I need setup commands grouped so dependency checks
 * and installation flows share one dispatch path.
 */
HandlerResult HandleSetup(rtk::EnhancedStore<FStoreState> &Store,
                         const FString &CommandKey,
                         const TArray<FString> &Args);

} // namespace Handlers
} // namespace CLIOps
