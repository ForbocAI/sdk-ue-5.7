#pragma once
/**
 * G5: IMemory function-pointer struct for testability
 * User Story: As a maintainer, I need this implementation note so I can understand which milestone behavior the surrounding code is preserving.
 */

#include "Memory/MemoryTypes.h"
#include "NativeEngine.h"

namespace Memory {

/**
 * Function-pointer struct wrapping Native::Sqlite:: operations.
 * Defaults to real implementations. Swap individual pointers for
 * test doubles without touching thunk code.
 * Usage:
 *   // Production (default)
 *   auto &Ops = Memory::GetOps();
 *   auto Db = Ops.Open(Path);
 *   // Test override
 *   Memory::FMemoryOps TestOps;
 *   TestOps.Search = [](void*, const TArray<float>&, int32) {
 *       return TArray<FMemoryItem>();
 *   };
 *   Memory::SetOps(TestOps);
 * User Story: As an SDK integrator, I need this type or module note so I can understand the role of the surrounding API surface quickly.
 */
struct FMemoryOps {
  std::function<Native::Sqlite::DB(const FString &)> Open;
  std::function<void(Native::Sqlite::DB)> Close;
  std::function<void(Native::Sqlite::DB)> Clear;
  std::function<void(const FString &)> ClearPath;
  std::function<bool(Native::Sqlite::DB, const FMemoryItem &,
                      const TArray<float> &)>
      Upsert;
  std::function<TArray<FMemoryItem>(Native::Sqlite::DB, const TArray<float> &,
                                     int32)>
      Search;

  FMemoryOps()
      : Open(Native::Sqlite::Open), Close(Native::Sqlite::Close),
        Clear(Native::Sqlite::Clear), ClearPath(Native::Sqlite::ClearPath),
        Upsert([](Native::Sqlite::DB Db, const FMemoryItem &Item,
                   const TArray<float> &Vec) {
          return Native::Sqlite::Upsert(Db, Item, Vec);
        }),
        Search([](Native::Sqlite::DB Db, const TArray<float> &Vec,
                   int32 TopK) {
          return Native::Sqlite::Search(Db, Vec, TopK);
        }) {}
};

inline FMemoryOps &GetOps() {
  static FMemoryOps Ops;
  return Ops;
}

inline void SetOps(const FMemoryOps &Ops) { GetOps() = Ops; }

inline void ResetOps() { GetOps() = FMemoryOps(); }

} // namespace Memory
