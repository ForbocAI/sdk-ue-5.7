#pragma once

#include "Core/functional_core.hpp"
#include "CoreMinimal.h"
#include "Cortex/CortexTypes.h"
#include "Memory/MemoryTypes.h"

/**
 * Native Engine Wrappers
 * User Story: As native-runtime integration, I need a single wrapper surface so
 * SDK code can talk to llama.cpp without third-party leakage.
 *
 * Isolates 3rd-party library (llama.cpp) mechanics from the SDK
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
FORBOCAI_SDK_LOCALCORTEX_API Context LoadModel(const FString &Path);

/**
 * Loads a GGUF embedding model for memory operations.
 * User Story: As local memory setup, I need an embedding model loader so text
 * can be converted into vectors for storage and recall.
 */
FORBOCAI_SDK_LOCALCORTEX_API Context LoadEmbeddingModel(const FString &Path);

/**
 * Frees the model context.
 * User Story: As native resource cleanup, I need model contexts released so
 * inference and embedding handles do not leak across runtime sessions.
 */
FORBOCAI_SDK_LOCALCORTEX_API void FreeModel(Context Ctx);

/**
 * Performs synchronous inference.
 * User Story: As local completion flows, I need synchronous inference so code
 * can request a generated response from a loaded model immediately.
 */
FORBOCAI_SDK_LOCALCORTEX_API FString Infer(Context Ctx, const FString &Prompt,
                               int32 MaxTokens = 512);

/**
 * Performs synchronous inference with SDK completion options.
 * User Story: As configurable completion flows, I need inference with SDK
 * options so prompt execution respects configured constraints.
 */
FORBOCAI_SDK_LOCALCORTEX_API FString Infer(Context Ctx, const FString &Prompt,
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
FORBOCAI_SDK_LOCALCORTEX_API FString InferStream(Context Ctx, const FString &Prompt,
                                     const FCortexConfig &Config,
                                     const TokenCallback &OnToken);

/**
 * Generates an embedding vector for text using the loaded embedding model.
 * User Story: As vector memory indexing, I need embeddings for text so native
 * memory backends can store and search semantic representations.
 */
FORBOCAI_SDK_LOCALCORTEX_API TArray<float> Embed(Context Ctx, const FString &Text);

} // namespace Llama
} // namespace Native