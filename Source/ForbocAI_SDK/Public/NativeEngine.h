#pragma once

#include "Core/functional_core.hpp"
#include "CoreMinimal.h"
#include "Cortex/CortexTypes.h"
#include "Memory/MemoryTypes.h"

/**
 * Native Engine Wrappers
 * User Story: As native-runtime integration, I need a single wrapper surface so
 * SDK code can talk to llama.cpp and sqlite helpers without third-party leakage.
 *
 * Isolates 3rd-party library (llama.cpp, sqlite-vec) mechanics from the SDK
 * logic. Requires native binaries — returns errors when unavailable.
 */
namespace Native {
namespace Llama {

using Context = void *;

/**
 * Loads a GGUF model for inference.
 * User Story: As local inference setup, I need a model loader so runtime code
 * can open a GGUF model before generating completions.
 */
FORBOCAI_SDK_API Context LoadModel(const FString &Path);

/**
 * Loads a GGUF embedding model for memory operations.
 * User Story: As local memory setup, I need an embedding model loader so text
 * can be converted into vectors for storage and recall.
 */
FORBOCAI_SDK_API Context LoadEmbeddingModel(const FString &Path);

/**
 * Frees the model context.
 * User Story: As native resource cleanup, I need model contexts released so
 * inference and embedding handles do not leak across runtime sessions.
 */
FORBOCAI_SDK_API void FreeModel(Context Ctx);

/**
 * Performs synchronous inference.
 * User Story: As local completion flows, I need synchronous inference so code
 * can request a generated response from a loaded model immediately.
 */
FORBOCAI_SDK_API FString Infer(Context Ctx, const FString &Prompt,
                               int32 MaxTokens = 512);

/**
 * Performs synchronous inference with SDK completion options.
 * User Story: As configurable completion flows, I need inference with SDK
 * options so prompt execution respects configured constraints.
 */
FORBOCAI_SDK_API FString Infer(Context Ctx, const FString &Prompt,
                               const FCortexConfig &Config);

/**
 * Per-token callback for streaming inference.
 * User Story: As streaming completion consumers, I need a token callback type
 * so UI and gameplay code can react to incremental output uniformly.
 */
using TokenCallback = std::function<void(const FString &Token)>;

/**
 * Performs streaming inference and calls the token callback for each token.
 * User Story: As streaming completion flows, I need per-token callbacks so UI
 * and gameplay code can react while text is still generating.
 */
FORBOCAI_SDK_API FString InferStream(Context Ctx, const FString &Prompt,
                                     const FCortexConfig &Config,
                                     const TokenCallback &OnToken);

/**
 * Generates an embedding vector for text using the loaded embedding model.
 * User Story: As vector memory indexing, I need embeddings for text so native
 * memory backends can store and search semantic representations.
 */
FORBOCAI_SDK_API TArray<float> Embed(Context Ctx, const FString &Text);

} // namespace Llama

namespace File {

/**
 * Downloads a binary file from a URL to a local path asynchronously.
 * Supports simple redirects if needed. Returns empty on failure.
 * User Story: As model bootstrap, I need binary downloads so local runtime
 * assets can be fetched before native inference starts.
 */
FORBOCAI_SDK_API func::AsyncResult<FString>
DownloadBinary(const FString &Url, const FString &DestPath);

} // namespace File

namespace Sqlite {

using DB = void *;

/**
 * Opens a vector-enabled sqlite database.
 * User Story: As local vector memory setup, I need database open support so
 * native search storage can be initialized on demand.
 */
FORBOCAI_SDK_API DB Open(const FString &Path);

/**
 * Closes the database.
 * User Story: As local vector memory cleanup, I need database handles closed
 * so native resources are released when memory use ends.
 */
FORBOCAI_SDK_API void Close(DB Database);

/**
 * Clears all rows for a database handle.
 * User Story: As memory reset flows, I need handle-based clearing so a live
 * vector store can be emptied without reopening it.
 */
FORBOCAI_SDK_API void Clear(DB Database);

/**
 * Clears all rows for a database path.
 * User Story: As memory reset flows, I need path-based clearing so a store can
 * be emptied even when only the database path is available.
 */
FORBOCAI_SDK_API void ClearPath(const FString &Path);

/**
 * Performs vector similarity search and returns full memory rows.
 * User Story: As vector recall flows, I need row-level search so semantic
 * queries return the full stored memory records.
 */
FORBOCAI_SDK_API TArray<FMemoryItem>
SearchRows(DB Database, const TArray<float> &Vector, int32 TopK = 5);

/**
 * Inserts or updates a full memory row.
 * User Story: As vector memory persistence, I need upsert support so stored
 * memories can be inserted or refreshed in one operation.
 */
FORBOCAI_SDK_API bool Upsert(DB Database, const FMemoryItem &Item);

/**
 * Provides the canonical row-based search entrypoint for the SDK.
 * User Story: As SDK vector recall, I need a canonical search entrypoint so
 * thunk code can rely on one native query surface.
 */
FORBOCAI_SDK_API TArray<FMemoryItem>
Search(DB Database, const TArray<float> &Vector, int32 TopK = 5);

/**
 * Provides the compatibility upsert overload during migration.
 * User Story: As migration compatibility, I need the legacy upsert overload so
 * older callers continue working while full-row APIs are adopted.
 */
FORBOCAI_SDK_API bool Upsert(DB Database, const FMemoryItem &Item,
                             const TArray<float> &Vector);

} // namespace Sqlite
} // namespace Native
