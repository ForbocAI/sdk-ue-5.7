#pragma once

#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Core/functional_core.hpp" // C++11 Functional Core Library
#include "CoreMinimal.h"
#include "Types.h"

/**
 * Functional Core Type Aliases for Agent operations
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
namespace AgentTypes {
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
 * Type aliases for Agent operations
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
using AgentCreationResult = Either<FString, FAgent>;
using AgentValidationResult = Either<FString, FAgentState>;
using AgentProcessResult = Either<FString, FAgentResponse>;
using AgentExportResult = Either<FString, FSoul>;
} // namespace AgentTypes

/**
 * Factory functions for creating Agents.
 * User Story: As an SDK integrator, I need this type or module note so I can understand the role of the surrounding API surface quickly.
 */
namespace AgentFactory {

/**
 * Creates a new agent from configuration.
 * User Story: As agent setup, I need a pure agent factory so configuration can
 * be turned into an immutable runtime agent consistently.
 * @param Config The configuration to transform into an Agent.
 * @return A new immutable FAgent instance.
 */
FORBOCAI_SDK_API FAgent Create(const FAgentConfig &Config);

/**
 * Creates an agent from a soul.
 * User Story: As soul import flows, I need a rehydration function so a stored
 * soul can become a runtime agent bound to an API endpoint.
 * @param Soul The Soul to rehydrate an Agent from.
 * @param ApiUrl The API URL to bind the Agent to.
 * @return A rehydrated FAgent instance.
 */
FORBOCAI_SDK_API FAgent FromSoul(const FSoul &Soul, const FString &ApiUrl);

} // namespace AgentFactory

/**
 * Operations on Agents — stateless free functions.
 * User Story: As an SDK integrator, I need this type or module note so I can understand the role of the surrounding API surface quickly.
 */
namespace AgentOps {

/**
 * Returns a new agent with updated state.
 * User Story: As agent state updates, I need immutable state replacement so
 * callers can evolve agent state without mutating the original value.
 * @param Agent The original Agent.
 * @param NewState The new state to apply.
 * @return A new FAgent with the updated state.
 */
FORBOCAI_SDK_API FAgent WithState(const FAgent &Agent,
                                  const FAgentState &NewState);

/**
 * Returns a new agent with updated memories.
 * User Story: As memory updates, I need immutable memory replacement so agent
 * state can evolve without mutating the existing agent value.
 * @param Agent The original Agent.
 * @param NewMemories The new list of memories.
 * @return A new FAgent with updated memories.
 */
FORBOCAI_SDK_API FAgent WithMemories(const FAgent &Agent,
                                     const TArray<FMemoryItem> &NewMemories);

/**
 * Calculates the next state by merging updates into the current state.
 * User Story: As agent state transition logic, I need a merge helper so delta
 * updates can be applied consistently across runtime flows.
 * @param Current The current state.
 * @param Updates The partial state updates to apply.
 * @return The merged new state.
 */
FORBOCAI_SDK_API FAgentState CalculateNewState(const FAgentState &Current,
                                               const FAgentState &Updates);

/**
 * Processes input and returns an async response.
 * User Story: As gameplay and chat flows, I need a processing entrypoint so an
 * agent can respond to player or world input with isolated side effects.
 * @param Agent The agent to process the input.
 * @param Input The input string from the user/world.
 * @param Context Additional context for the processing.
 * @return An AsyncResult containing the response.
 */
FORBOCAI_SDK_API AgentTypes::AsyncResult<FAgentResponse>
Process(const FAgent &Agent, const FString &Input,
        const TMap<FString, FString> &Context);

/**
 * Exports agent data to a soul.
 * User Story: As soul export flows, I need agent data converted into a soul so
 * the runtime state can be serialized and transferred.
 * @param Agent The agent to export.
 * @return A new FSoul containing the agent's data.
 */
FORBOCAI_SDK_API FSoul Export(const FAgent &Agent);

} // namespace AgentOps

/**
 * Functional Core Helper Functions for Agent operations
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
namespace AgentHelpers {
/**
 * Creates a lazy agent factory from configuration.
 * User Story: As deferred agent setup, I need lazy construction so agent
 * creation can be postponed until a pipeline consumes it.
 */
inline AgentTypes::Lazy<FAgent> createLazyAgent(const FAgentConfig &config) {
  return func::lazy([config]() -> FAgent { return AgentFactory::Create(config); });
}

/**
 * Builds the validation pipeline for agent state.
 * User Story: As agent state validation, I need a reusable pipeline so empty or
 * malformed state payloads fail before processing begins.
 */
inline AgentTypes::ValidationPipeline<FAgentState, FString>
agentStateValidationPipeline() {
  return func::validationPipeline<FAgentState, FString>() |
         [](const FAgentState &state)
             -> AgentTypes::Either<FString, FAgentState> {
           return (state.JsonData.IsEmpty() || state.JsonData == TEXT("{}"))
                      ? AgentTypes::make_left(FString(TEXT("Empty agent state")),
                                              FAgentState{})
                      : AgentTypes::make_right(FString(), state);
         } |
         [](const FAgentState &state)
             -> AgentTypes::Either<FString, FAgentState> {
           return AgentTypes::make_right(FString(), state);
         };
}

/**
 * Builds the pipeline wrapper for agent processing composition.
 * User Story: As functional agent helpers, I need a pipe-ready agent value so
 * later processing steps can compose around it.
 */
inline AgentTypes::Pipeline<FAgent>
agentProcessingPipeline(const FAgent &agent) {
  return func::pipe(agent);
}

/**
 * Builds the curried agent creation helper.
 * User Story: As functional agent construction, I need a curried creator so
 * agent creation can participate in composable pipelines.
 */
typedef decltype(func::curry<1>(
    std::function<AgentTypes::AgentCreationResult(FAgentConfig)>()))
    FCurriedAgentCreation;

inline FCurriedAgentCreation curriedAgentCreation() {
  std::function<AgentTypes::AgentCreationResult(FAgentConfig)> Creator =
      [](FAgentConfig config) -> AgentTypes::AgentCreationResult {
    try {
      return AgentTypes::make_right(FString(), AgentFactory::Create(config));
    } catch (const std::exception &e) {
      return AgentTypes::make_left(FString(e.what()), FAgent{});
    }
  };
  return func::curry<1>(Creator);
}
} // namespace AgentHelpers
