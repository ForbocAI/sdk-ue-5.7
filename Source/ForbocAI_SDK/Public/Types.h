#pragma once

#include "Core/functional_core.hpp"
#include "CoreMinimal.h"

/**
 * Subdomain Types
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
#include "Bridge/BridgeTypes.h"
#include "Core/SystemTypes.h"
#include "Cortex/CortexTypes.h"
#include "Ghost/GhostTypes.h"
#include "Memory/MemoryTypes.h"
#include "NPC/NPCBaseTypes.h"
#include "Protocol/ProtocolRequestTypes.h"
#include "Protocol/ProtocolTypes.h"
#include "Soul/SoulTypes.h"

/**
 * Functional Core Type Aliases for SDK types
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
namespace SDKTypes {
using func::AsyncResult;
using func::ConfigBuilder;
using func::Curried;
using func::Either;
using func::Lazy;
using func::Maybe;
using func::Pipeline;
using func::TestResult;
using func::ValidationPipeline;

/**
 * Type aliases for SDK operations
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
using AgentCreationResult = Either<FString, FAgent>;
using AgentValidationResult = Either<FString, FAgentState>;
using AgentProcessResult = Either<FString, FAgentResponse>;
using AgentExportResult = Either<FString, FSoul>;
using MemoryStoreResult = Either<FString, FMemoryStore>;
using CortexCreationResult = Either<FString, FCortex>;
using GhostTestResult = TestResult<FGhostTestResult>;
using BridgeValidationResult = Either<FString, FValidationResult>;
} // namespace SDKTypes
