#pragma once

/**
 * DEPRECATED: Include "NPC/NPCBaseTypes.h" instead.
 * This forwarding header exists for backward compatibility. All struct
 * definitions (FAgentState, FAgentAction, FAgent, FAgentConfig,
 * FAgentResponse, FImportedNpc) have moved to NPCBaseTypes.h as part
 * of the Agent → NPC terminology migration.
 * Struct names are unchanged for Blueprint compatibility.
 * User Story: As an integrator upgrading SDK includes, I need this deprecation note so I can move to the supported header without breaking compatibility.
 */

#include "NPCBaseTypes.h"
