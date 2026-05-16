#pragma once

#include "Async/Async.h"
#include "Core/functional_core.hpp"
#include "CoreMinimal.h"
#include "Types.h"

/**
 * Functional Core Type Aliases for Memory operations
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
namespace MemoryTypes {
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
 * Type aliases for Memory operations
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
using MemoryStoreResult = Either<FString, FMemoryStore>;
using MemoryStoreCreationResult = Either<FString, FMemoryStore>;
using MemoryStoreInitializationResult = Either<FString, bool>;
using MemoryStoreAddResult = Either<FString, FMemoryStore>;
using MemoryStoreRecallResult = Either<FString, TArray<FMemoryItem>>;
using MemoryStoreEmbeddingResult = Either<FString, TArray<float>>;
} // namespace MemoryTypes

/**
 * Memory Operations — Stateless free functions.
 * User Story: As memory runtime flows, I need a single namespace for memory
 * operations so store, recall, and cleanup functions stay discoverable.
 */
namespace MemoryOps {

/**
 * Initializes the memory store and database.
 * User Story: As memory startup, I need store initialization to prepare the
 * database before writes and recalls can run safely.
 * @param Store The memory store to initialize.
 * @return A validation result indicating success or failure.
 */
FORBOCAI_SDK_API MemoryTypes::MemoryStoreInitializationResult
Initialize(FMemoryStore &Store);

/**
 * Stores a new memory from raw text input.
 * User Story: As memory persistence, I need a store operation that accepts raw
 * text so observations can become searchable memories.
 * @param Store The current memory store.
 * @param Text The memory content.
 * @param Type The memory type.
 * @param Importance The importance score.
 * @return A new memory store with the memory added.
 */
FORBOCAI_SDK_API TFuture<MemoryTypes::MemoryStoreAddResult>
Store(const FMemoryStore &Store, const FString &Text, const FString &Type,
      float Importance);

/**
 * Stores a pre-built memory item.
 * User Story: As memory persistence, I need an item-based store operation so
 * callers with fully shaped memory records can persist them directly.
 * @param Store The current memory store.
 * @param Item The memory item to add.
 * @return A new memory store with the item added.
 */
FORBOCAI_SDK_API TFuture<MemoryTypes::MemoryStoreAddResult>
Add(const FMemoryStore &Store, const FMemoryItem &Item);

/**
 * Performs semantic recall using vector search.
 * User Story: As memory recall, I need semantic search so relevant memories can
 * be retrieved from natural-language queries.
 * @param Store The memory store to search.
 * @param Query The search query text.
 * @param Limit The maximum number of results to return.
 * @return The ranked memory results.
 */
FORBOCAI_SDK_API TFuture<MemoryTypes::MemoryStoreRecallResult>
Recall(const FMemoryStore &Store, const FString &Query, int32 Limit = -1);

/**
 * Generates a vector embedding for text.
 * User Story: As memory indexing, I need embeddings for text so memories and
 * recall queries can participate in vector search.
 * @param Store The memory store.
 * @param Text The text to embed.
 * @return The vector embedding.
 */
FORBOCAI_SDK_API TFuture<MemoryTypes::MemoryStoreEmbeddingResult>
GenerateEmbedding(const FMemoryStore &Store, const FString &Text);

/**
 * Gets the current memory statistics.
 * User Story: As runtime inspection, I need memory statistics so tooling can
 * report the current health and size of the memory store.
 * @param Store The memory store.
 * @return The memory statistics.
 */
FORBOCAI_SDK_API FString GetStatistics(const FMemoryStore &Store);

/**
 * Cleans up resources and closes the database.
 * User Story: As memory shutdown, I need cleanup so database handles and other
 * resources are released when the store is no longer needed.
 * @param Store The memory store to clean up.
 */
FORBOCAI_SDK_API void Cleanup(FMemoryStore &Store);

} // namespace MemoryOps

/**
 * Memory Factory
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
namespace MemoryFactory {
/**
 * Creates a memory store from configuration.
 * User Story: As memory setup, I need a factory so store creation is
 * standardized before initialization and use.
 * @param Config The memory configuration.
 * @return A new memory store.
 */
FORBOCAI_SDK_API FMemoryStore CreateStore(const FMemoryConfig &Config);
} // namespace MemoryFactory

/**
 * Memory Helpers (Functional)
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
namespace MemoryHelpers {
/**
 * Creates a lazy memory store factory from configuration.
 * User Story: As deferred memory setup, I need lazy construction so pipelines
 * can assemble store creation without opening resources immediately.
 */
inline MemoryTypes::Lazy<FMemoryStore>
createLazyMemoryStore(const FMemoryConfig &config) {
  return func::lazy([config]() -> FMemoryStore {
    return MemoryFactory::CreateStore(config);
  });
}

/**
 * Builds the validation pipeline for memory configuration.
 * User Story: As memory config validation, I need one reusable pipeline so
 * invalid paths or limits fail before store creation begins.
 */
inline MemoryTypes::ValidationPipeline<FMemoryConfig, FString>
memoryConfigValidationPipeline() {
  return func::validationPipeline<FMemoryConfig, FString>() |
         [](const FMemoryConfig &config)
             -> MemoryTypes::Either<FString, FMemoryConfig> {
           return config.DatabasePath.IsEmpty()
                      ? MemoryTypes::make_left(
                            FString(TEXT("Database path cannot be empty")),
                            FMemoryConfig{})
                      : MemoryTypes::make_right(FString(), config);
         } |
         [](const FMemoryConfig &config)
             -> MemoryTypes::Either<FString, FMemoryConfig> {
           return config.MaxMemories < 1
                      ? MemoryTypes::make_left(
                            FString(TEXT("Max memories must be at least 1")),
                            FMemoryConfig{})
                      : MemoryTypes::make_right(FString(), config);
         } |
         [](const FMemoryConfig &config)
             -> MemoryTypes::Either<FString, FMemoryConfig> {
           return config.VectorDimension != 384
                      ? MemoryTypes::make_left(
                            FString(TEXT("Vector dimension must be 384")),
                            FMemoryConfig{})
                      : MemoryTypes::make_right(FString(), config);
         } |
         [](const FMemoryConfig &config)
             -> MemoryTypes::Either<FString, FMemoryConfig> {
           return config.MaxRecallResults < 1
                      ? MemoryTypes::make_left(
                            FString(TEXT("Max recall results must be at least 1")),
                            FMemoryConfig{})
                      : MemoryTypes::make_right(FString(), config);
         };
}

/**
 * Builds the pipeline wrapper for memory store creation.
 * User Story: As functional memory helpers, I need a pipe-ready store value so
 * later creation steps can compose around it.
 */
inline MemoryTypes::Pipeline<FMemoryStore>
memoryStoreCreationPipeline(const FMemoryStore &store) {
  return func::pipe(store);
}

/**
 * Builds the curried memory store creation helper.
 * User Story: As functional memory construction, I need a curried creator so
 * store creation can be partially applied in pipelines.
 */
typedef decltype(func::curry<1>(
    std::function<MemoryTypes::MemoryStoreCreationResult(FMemoryConfig)>()))
    FCurriedMemoryStoreCreation;

inline FCurriedMemoryStoreCreation curriedMemoryStoreCreation() {
  std::function<MemoryTypes::MemoryStoreCreationResult(FMemoryConfig)> Creator =
      [](FMemoryConfig config) -> MemoryTypes::MemoryStoreCreationResult {
    try {
      FMemoryStore store = MemoryFactory::CreateStore(config);
      return MemoryTypes::make_right(FString(), store);
    } catch (const std::exception &e) {
      return MemoryTypes::make_left(FString(e.what()), FMemoryStore{});
    }
  };
  return func::curry<1>(Creator);
}
} // namespace MemoryHelpers
