#pragma once

#include "Memory/MemoryModule.h"
#include "NativeEngine.h"

namespace MemoryInternal {

/**
 * Native Wrappers
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
namespace Native {
namespace Sqlite {
using Connection = ::Native::Sqlite::DB;
/**
 * Opens the native sqlite connection for the given path.
 * User Story: As native memory storage, I need a thin wrapper so private memory
 * internals can open sqlite handles without leaking engine-specific details.
 */
Connection Open(const FString &Path);
/**
 * Closes the native sqlite connection.
 * User Story: As native memory cleanup, I need connection teardown so private
 * memory internals can release sqlite resources deterministically.
 */
void Close(Connection Db);
/**
 * Executes raw SQL against the native sqlite connection.
 * User Story: As private memory setup, I need SQL execution support so schema
 * and maintenance statements can run through one wrapper.
 */
bool Execute(Connection Db, const FString &Sql);
/**
 * Inserts one memory row through the native sqlite wrapper.
 * User Story: As native memory persistence, I need a wrapper insert function so
 * memory rows can be stored without exposing low-level sqlite calls broadly.
 */
void Insert(Connection Db, const FMemoryItem &Item);
/**
 * Performs vector similarity search through the native sqlite wrapper.
 * User Story: As native memory recall, I need vector search support so private
 * memory internals can retrieve relevant rows from sqlite-vss.
 */
TArray<FMemoryItem> VssSearch(Connection Db, const TArray<float> &Vector,
                              int32 Limit);
} // namespace Sqlite
} // namespace Native

struct FDatabaseOpenResult {
  bool isLeft = false;
  FString left;
  Native::Sqlite::Connection right = nullptr;

  /**
   * Converts the open result into the public memory initialization result type.
   * User Story: As memory initialization flows, I need private open results
   * adapted into the public result type consumed by callers.
   */
  operator MemoryTypes::MemoryStoreInitializationResult() const {
    return MemoryTypes::MemoryStoreInitializationResult{
        isLeft, left, !isLeft && right != nullptr};
  }
};

using FDatabaseMutationResult = MemoryTypes::MemoryStoreInitializationResult;

/**
 * SQLite-VSS Specifics
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
namespace SQLiteVSS {
/**
 * Opens the sqlite-vss database and returns the wrapped handle result.
 * User Story: As sqlite-vss setup, I need a focused open helper so memory
 * initialization can distinguish handle failures cleanly.
 */
FDatabaseOpenResult OpenDatabase(const FString &Path);
/**
 * Closes the sqlite-vss database handle.
 * User Story: As sqlite-vss cleanup, I need a close helper so memory teardown
 * releases native database resources safely.
 */
void CloseDatabase(void *Handle);
/**
 * Creates the required sqlite-vss tables.
 * User Story: As memory schema setup, I need table creation so new databases
 * are ready before store and recall operations begin.
 */
FDatabaseMutationResult CreateTables(void *Handle);
/**
 * Inserts one memory row into sqlite-vss.
 * User Story: As sqlite-vss persistence, I need a dedicated insert helper so
 * memory writes share one storage path.
 */
FDatabaseMutationResult InsertMemory(void *Handle, const FMemoryItem &Item);
/**
 * Runs vector search through sqlite-vss for a text query.
 * User Story: As sqlite-vss recall, I need a search helper so memory recall can
 * translate user queries into ranked memory results.
 */
MemoryTypes::MemoryStoreRecallResult
VectorSearch(void *Handle, const FString &Query, int32 Limit);
/**
 * Generates an embedding vector through sqlite-vss dependencies.
 * User Story: As sqlite-vss indexing, I need embeddings generated so memories
 * and queries can participate in vector search.
 */
MemoryTypes::MemoryStoreEmbeddingResult GenerateEmbedding(void *Handle,
                                                          const FString &Text);
} // namespace SQLiteVSS

/**
 * Resolves the effective database path from memory configuration.
 * User Story: As memory configuration, I need one path resolver so private
 * storage code uses the same database location everywhere.
 */
FString GetDatabasePath(const FMemoryConfig &Config);
/**
 * Validates private memory configuration before native setup.
 * User Story: As memory initialization, I need validation so misconfigured
 * storage paths or limits fail before native resources are opened.
 */
MemoryTypes::MemoryStoreInitializationResult
ValidateConfig(const FMemoryConfig &Config);
/**
 * Computes human-readable statistics for a memory store.
 * User Story: As diagnostics and tooling, I need memory statistics so private
 * storage state can be summarized for operators.
 */
FString GetMemoryStatistics(const FMemoryStore &Store);

} // namespace MemoryInternal
