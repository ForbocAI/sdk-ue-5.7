#pragma once

#include "Core/functional_core.hpp"
#include "CoreMinimal.h"
#include "Memory/MemoryTypes.h"

/**
 * Native Storage Wrappers
 * User Story: As native-runtime integration, I need a single wrapper surface so
 * SDK code can talk to sqlite and file helpers without third-party leakage.
 */
namespace Native {

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