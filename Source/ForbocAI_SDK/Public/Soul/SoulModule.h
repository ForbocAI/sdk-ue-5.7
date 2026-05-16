#pragma once

#include "Core/functional_core.hpp"
#include "CoreMinimal.h"
#include "Types.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include <functional>

/**
 * Functional Core Type Aliases for Soul operations
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
namespace SoulTypes {
using func::AsyncResult;
using func::ConfigBuilder;
using func::Curried;
using func::Either;
using func::Lazy;
using func::Maybe;
using func::Pipeline;
using func::TestResult;
using func::ValidationPipeline;

using func::make_left;
using func::make_right;

/**
 * Type aliases for Soul operations
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
using SoulCreationResult = Either<FString, FSoul>;
using SoulSerializationResult = Either<FString, FString>;
using SoulDeserializationResult = Either<FString, FSoul>;
using SoulValidationResult = Either<FString, FSoul>;
using SoulExportResult = AsyncResult<FSoulExportResult>;
} // namespace SoulTypes

/**
 * Soul Operations - Arweave serialization and transport facade.
 * User Story: As soul import and export flows, I need one namespace for soul
 * operations so serialization, validation, and publishing stay cohesive.
 */
namespace SoulOps {

/**
 * Creates a soul from agent state, memories, id, and persona.
 * User Story: As soul export preparation, I need a pure soul factory so agent
 * data can be packaged into a portable soul record consistently.
 * @param State The agent's state.
 * @param Memories The agent's memories.
 * @param AgentId The agent's ID.
 * @param Persona The agent's persona.
 * @return A Result containing the new FSoul instance or error.
 */
FORBOCAI_SDK_API SoulTypes::SoulCreationResult
FromAgent(const FAgentState &State, const TArray<FMemoryItem> &Memories,
          const FString &AgentId, const FString &Persona);

/**
 * Serializes a soul to JSON.
 * User Story: As soul persistence and transport, I need JSON serialization so
 * a soul can be signed, stored, or transmitted over the API.
 * @param Soul The Soul object to serialize.
 * @return A Result containing the JSON string or error.
 */
FORBOCAI_SDK_API SoulTypes::SoulSerializationResult
Serialize(const FSoul &Soul);

/**
 * Deserializes a soul from JSON.
 * User Story: As soul import flows, I need JSON deserialization so stored soul
 * payloads can be reconstructed into runtime types.
 * @param Json The JSON string to deserialize.
 * @return A Result containing the new FSoul instance or error.
 */
FORBOCAI_SDK_API SoulTypes::SoulDeserializationResult
Deserialize(const FString &Json);

/**
 * Validates a soul structure before further processing.
 * User Story: As soul validation, I need invalid soul payloads rejected early
 * so export and import flows do not continue with broken data.
 * @param Soul The Soul object to validate.
 * @return A validation result containing the Soul if valid, or error.
 */
FORBOCAI_SDK_API SoulTypes::SoulValidationResult Validate(const FSoul &Soul);

/**
 * Triggers soul export to Arweave through the API.
 * User Story: As soul publishing, I need an async export function so a local
 * soul can be uploaded and persisted remotely.
 * @param Soul The soul to export.
 * @param ApiUrl The endpoint URL.
 * @return An AsyncResult that can be chained with .then() and .catch_().
 */
FORBOCAI_SDK_API SoulTypes::SoulExportResult
ExportToArweave(const FSoul &Soul, const FString &ApiUrl);

} // namespace SoulOps

/**
 * Functional Core Helper Functions for Soul operations
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
namespace SoulHelpers {
/**
 * Creates a lazy soul factory from agent state and memories.
 * User Story: As deferred soul creation, I need soul construction postponed so
 * callers can build pipelines without materializing the soul immediately.
 */
inline SoulTypes::Lazy<FSoul>
createLazySoul(const FAgentState &state, const TArray<FMemoryItem> &memories,
               const FString &id, const FString &persona) {
  return func::lazy([state, memories, id, persona]() -> FSoul {
    return SoulOps::FromAgent(state, memories, id, persona).right;
  });
}

/**
 * Builds the validation pipeline for soul values.
 * User Story: As soul validation flows, I need a reusable pipeline so missing
 * identifiers or persona data fail before serialization and export.
 */
inline SoulTypes::ValidationPipeline<FSoul, FString> soulValidationPipeline() {
  return func::validationPipeline<FSoul, FString>() |
         [](const FSoul &soul) -> SoulTypes::Either<FString, FSoul> {
           return soul.Id.IsEmpty()
                      ? SoulTypes::make_left(FString(TEXT("Missing Soul ID")),
                                             FSoul{})
                      : SoulTypes::make_right(FString(), soul);
         } |
         [](const FSoul &soul) -> SoulTypes::Either<FString, FSoul> {
           return soul.Persona.IsEmpty()
                      ? SoulTypes::make_left(FString(TEXT("Missing Persona")),
                                             FSoul{})
                      : SoulTypes::make_right(FString(), soul);
         };
}

/**
 * Builds the pipeline wrapper for soul serialization steps.
 * User Story: As functional soul helpers, I need a pipe-ready value so later
 * serialization steps can compose around the current soul.
 */
inline SoulTypes::Pipeline<FSoul> soulSerializationPipeline(const FSoul &soul) {
  return func::pipe(soul);
}

/**
 * Builds the curried soul creation helper.
 * User Story: As functional soul construction, I need a curried creator so
 * soul creation can be partially applied inside pipelines.
 */
typedef decltype(func::curry<4>(std::function<SoulTypes::SoulCreationResult(
    FAgentState, TArray<FMemoryItem>, FString, FString)>()))
    FCurriedSoulCreation;

inline FCurriedSoulCreation curriedSoulCreation() {
  std::function<SoulTypes::SoulCreationResult(
      FAgentState, TArray<FMemoryItem>, FString, FString)>
      Creator = [](FAgentState state, TArray<FMemoryItem> memories,
                   FString id, FString persona) -> SoulTypes::SoulCreationResult {
    return SoulOps::FromAgent(state, memories, id, persona);
  };
  return func::curry<4>(Creator);
}
} // namespace SoulHelpers
