#include "Memory/MemoryModuleInternal.h"
#include "Misc/Paths.h"

namespace MemoryInternal {

namespace Native {
namespace Sqlite {

/**
 * Opens a native sqlite-backed memory database connection.
 * User Story: As native-memory setup, I need the low-level sqlite connection
 * opened in one place so higher-level storage code stays backend-agnostic.
 */
Connection Open(const FString &Path) { return ::Native::Sqlite::Open(Path); }

/**
 * Closes a native sqlite-backed memory database connection.
 * User Story: As native-memory cleanup, I need database handles closed through
 * one helper so shutdown does not leak sqlite resources.
 */
void Close(Connection Db) { ::Native::Sqlite::Close(Db); }

/**
 * Validates whether a sqlite command is eligible to execute.
 * User Story: As native-memory helpers, I need a lightweight eligibility check
 * so invalid connections or empty SQL do not proceed into sqlite calls.
 */
bool Execute(Connection Db, const FString &Sql) {
  return Db != nullptr && !Sql.IsEmpty();
}

/**
 * Inserts or replaces a memory item in the native sqlite store.
 * User Story: As native-memory writes, I need a shared insert helper so local
 * storage uses one consistent sqlite upsert path.
 */
void Insert(Connection Db, const FMemoryItem &Item) {
  ::Native::Sqlite::Upsert(Db, Item);
}

} // namespace Sqlite
} // namespace Native

namespace SQLiteVSS {

/**
 * Opens the sqlite-vec handle used by the higher-level memory store.
 * User Story: As memory-store initialization, I need sqlite-vec handles opened
 * with structured results so setup failures can be surfaced cleanly.
 */
FDatabaseOpenResult OpenDatabase(const FString &Path) {
  try {
    void *Handle = Native::Sqlite::Open(Path);
    return Handle == nullptr
               ? FDatabaseOpenResult{true,
                                     TEXT("Failed to open memory database"),
                                     nullptr}
               : FDatabaseOpenResult{false, FString(), Handle};
  } catch (const std::exception &e) {
    return {true, FString(e.what()), nullptr};
  }
}

/**
 * Closes an open sqlite-vec handle when the store shuts down.
 * User Story: As memory-store shutdown, I need handles closed in one helper so
 * teardown remains safe even when initialization only partially succeeded.
 */
void CloseDatabase(void *Handle) {
  Handle != nullptr ? (Native::Sqlite::Close(Handle), void()) : void();
}

/**
 * Ensures the sqlite-vec schema exists for persisted memory rows.
 * User Story: As memory-store initialization, I need the required schema
 * created before writes so persisted memories have the expected table layout.
 */
FDatabaseMutationResult CreateTables(void *Handle) {
  try {
    const FString Sql = TEXT(
        "CREATE TABLE IF NOT EXISTS items("
        "id TEXT PRIMARY KEY,"
        "text TEXT,"
        "type TEXT,"
        "importance REAL,"
        "timestamp INTEGER,"
        "similarity REAL,"
        "embedding BLOB);");

    const bool bSuccess = Native::Sqlite::Execute(Handle, Sql);
    return FDatabaseMutationResult{
        !bSuccess,
        bSuccess ? FString()
                 : FString(TEXT("Failed to create memory tables")),
        bSuccess};
  } catch (const std::exception &e) {
    return FDatabaseMutationResult{true, FString(e.what()), false};
  }
}

/**
 * Writes a single memory item into the sqlite-vec store.
 * User Story: As memory-store writes, I need individual memories persisted
 * through a structured helper so write failures return typed results.
 */
FDatabaseMutationResult InsertMemory(void *Handle, const FMemoryItem &Item) {
  try {
    return Handle == nullptr
               ? FDatabaseMutationResult{
                     true, TEXT("Memory database is not open"), false}
               : (Native::Sqlite::Insert(Handle, Item),
                  FDatabaseMutationResult{false, FString(), true});
  } catch (const std::exception &e) {
    return FDatabaseMutationResult{true, FString(e.what()), false};
  }
}

} // namespace SQLiteVSS

/**
 * Resolves a memory database path into the runtime storage location.
 * User Story: As memory-store setup, I need database paths normalized so
 * relative project paths resolve safely into local infrastructure storage.
 * Directory traversal segments are rejected before resolution.
 */
FString GetDatabasePath(const FMemoryConfig &Config) {
  return Config.DatabasePath.IsEmpty()
             ? FString()
         : Config.DatabasePath.Contains(TEXT(".."))
             ? [&]() -> FString {
                 UE_LOG(LogTemp, Error,
                        TEXT("ForbocAI: Rejected database path containing "
                             "'..': %s"),
                        *Config.DatabasePath);
                 return FString();
               }()
         : FPaths::IsRelative(Config.DatabasePath)
             ? FPaths::Combine(FPaths::ProjectDir(),
                               TEXT("local_infrastructure"),
                               Config.DatabasePath)
             : Config.DatabasePath;
}

/**
 * Validates memory-store config before initialization proceeds.
 * User Story: As memory-store setup, I need config validated up front so
 * initialization fails fast with actionable error messages.
 */
MemoryTypes::MemoryStoreInitializationResult
ValidateConfig(const FMemoryConfig &Config) {
  try {
    return Config.DatabasePath.IsEmpty()
               ? MemoryTypes::MemoryStoreInitializationResult{
                     true, TEXT("Database path cannot be empty"), false}
           : Config.MaxMemories <= 0
               ? MemoryTypes::MemoryStoreInitializationResult{
                     true,
                     TEXT("Max memories must be greater than 0"), false}
           : Config.VectorDimension != 384
               ? MemoryTypes::MemoryStoreInitializationResult{
                     true, TEXT("Vector dimension must be 384"), false}
           : Config.MaxRecallResults <= 0
               ? MemoryTypes::MemoryStoreInitializationResult{
                     true,
                     TEXT("Max recall results must be greater than 0"),
                     false}
               : MemoryTypes::MemoryStoreInitializationResult{
                     false, FString(), true};
  } catch (const std::exception &e) {
    return MemoryTypes::MemoryStoreInitializationResult{
        true, FString(e.what()), false};
  }
}

/**
 * Formats a human-readable memory store statistics report.
 * User Story: As diagnostics flows, I need readable memory-store statistics so
 * operators can inspect storage health from logs or CLI output.
 */
FString GetMemoryStatistics(const FMemoryStore &Store) {
  FString Statistics = FString::Printf(TEXT("Memory Statistics\n"));
  Statistics +=
      FString::Printf(TEXT("  Total Memories: %d\n"), Store.Items.Num());
  Statistics += FString::Printf(TEXT("  Database: %s\n"),
                                Store.bInitialized ? TEXT("Connected")
                                                   : TEXT("Disconnected"));
  Statistics +=
      FString::Printf(TEXT("  Max Memories: %d\n"), Store.Config.MaxMemories);
  Statistics += FString::Printf(TEXT("  Vector Dimension: %d\n"),
                                Store.Config.VectorDimension);
  Statistics += FString::Printf(TEXT("  Use GPU: %s\n"),
                                Store.Config.UseGPU ? TEXT("Yes") : TEXT("No"));
  Statistics += FString::Printf(TEXT("  Max Recall Results: %d\n"),
                                Store.Config.MaxRecallResults);

  return Statistics;
}

} // namespace MemoryInternal
