#include "Memory/MemoryModule.h"
#include "Core/functional_core.hpp"
#include "HAL/PlatformFilemanager.h"
#include "Memory/MemoryModuleInternal.h"
#include "Memory/MemorySlice.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RuntimeStore.h"
#include "Serialization/JsonSerializer.h"
#include "Memory/MemoryThunks.h"

/**
 * Factory function implementation
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
FMemoryStore MemoryFactory::CreateStore(const FMemoryConfig &Config) {
  /**
   * Use functional validation
   * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
  */
  auto validationResult =
      func::runValidation(MemoryHelpers::memoryConfigValidationPipeline(),
                          Config);
  validationResult.isLeft
      ? throw std::runtime_error(TCHAR_TO_UTF8(*validationResult.left))
      : (void)0;

  FMemoryStore Store;
  Store.Config = Config;
  Store.bInitialized = false;
  Store.DatabaseHandle = nullptr;
  return Store;
}

/**
 * Initialization implementation
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
MemoryTypes::MemoryStoreInitializationResult
MemoryOps::Initialize(FMemoryStore &Store) {
  try {
    return Store.bInitialized
               ? MemoryTypes::make_right(FString(), true)
               : [&]() -> MemoryTypes::MemoryStoreInitializationResult {
                   auto configValidation =
                       MemoryInternal::ValidateConfig(Store.Config);
                   return configValidation.isLeft
                       ? configValidation
                       : [&]() -> MemoryTypes::MemoryStoreInitializationResult {
                           const FString DatabasePath =
                               MemoryInternal::GetDatabasePath(Store.Config);
                           auto openResult =
                               MemoryInternal::SQLiteVSS::OpenDatabase(
                                   DatabasePath);
                           return openResult.isLeft
                               ? openResult
                               : [&]() -> MemoryTypes::
                                     MemoryStoreInitializationResult {
                                       Store.DatabaseHandle = openResult.right;
                                       auto tableResult =
                                           MemoryInternal::SQLiteVSS::
                                               CreateTables(
                                                   Store.DatabaseHandle);
                                       return tableResult.isLeft
                                           ? [&]() -> MemoryTypes::
                                                 MemoryStoreInitializationResult {
                                                   MemoryInternal::SQLiteVSS::
                                                       CloseDatabase(
                                                           Store
                                                               .DatabaseHandle);
                                                   Store.DatabaseHandle =
                                                       nullptr;
                                                   return tableResult;
                                                 }()
                                           : [&]() -> MemoryTypes::
                                                 MemoryStoreInitializationResult {
                                                   Store.bInitialized = true;
                                                   return MemoryTypes::
                                                       make_right(FString(),
                                                                  true);
                                                 }();
                                     }();
                         }();
                 }();
  } catch (const std::exception &e) {
    return MemoryTypes::make_left(FString(e.what()), false);
  }
}

/**
 * Store implementation
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
TFuture<MemoryTypes::MemoryStoreAddResult>
MemoryOps::Store(const FMemoryStore &Store, const FString &Text,
                 const FString &Type, float Importance) {
  return !Store.bInitialized
             ? [&]() -> TFuture<MemoryTypes::MemoryStoreAddResult> {
                 TPromise<MemoryTypes::MemoryStoreAddResult> Promise;
                 Promise.SetValue(MemoryTypes::make_left(
                     FString(TEXT("Memory store not initialized")),
                     FMemoryStore{}));
                 return Promise.GetFuture();
               }()
             : [&]() -> TFuture<MemoryTypes::MemoryStoreAddResult> {
                 TSharedPtr<TPromise<MemoryTypes::MemoryStoreAddResult>>
                     Promise = MakeShared<
                         TPromise<MemoryTypes::MemoryStoreAddResult>>();

                 FMemoryItem Item;
                 Item.Id = FString::Printf(
                     TEXT("mem_%s"),
                     *FGuid::NewGuid().ToString());
                 Item.Text = Text;
                 Item.Type = Type;
                 Item.Importance =
                     FMath::Clamp(Importance, 0.0f, 1.0f);
                 Item.Timestamp =
                     FDateTime::Now().ToUnixTimestamp();

                 auto RuntimeStore = ConfigureStore();

                 /**
                  * Dispatch the store thunk
                  * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
                  */
                 auto ThunkResult = rtk::nodeMemoryStoreThunk(Item)(
                     [RuntimeStore](const rtk::AnyAction &a) {
                       return RuntimeStore.dispatch(a);
                     },
                     [RuntimeStore]() {
                       return RuntimeStore.getState();
                     });

                 /**
                  * Chain result back to the promise
                  * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
                  */
                 ThunkResult
                     .then([Promise, Store](FMemoryItem SavedItem) {
                       FMemoryStore NewStore = Store;
                       NewStore.Items.Add(SavedItem);
                       Promise->SetValue(MemoryTypes::make_right(
                           FString(), NewStore));
                     })
                     .catch_([Promise](std::string Error) {
                       Promise->SetValue(MemoryTypes::make_left(
                           FString(Error.c_str()), FMemoryStore{}));
                     })
                     .execute();

                 return Promise->GetFuture();
               }();
}

/**
 * Add implementation
 * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
 */
TFuture<MemoryTypes::MemoryStoreAddResult>
MemoryOps::Add(const FMemoryStore &Store, const FMemoryItem &Item) {
  return MemoryOps::Store(Store, Item.Text, Item.Type, Item.Importance);
}

/**
 * Recall implementation
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
TFuture<MemoryTypes::MemoryStoreRecallResult>
MemoryOps::Recall(const FMemoryStore &Store, const FString &Query,
                  int32 Limit) {
  return !Store.bInitialized
             ? [&]() -> TFuture<MemoryTypes::MemoryStoreRecallResult> {
                 TPromise<MemoryTypes::MemoryStoreRecallResult> Promise;
                 Promise.SetValue(MemoryTypes::make_left(
                     FString(TEXT("Memory store not initialized")),
                     TArray<FMemoryItem>{}));
                 return Promise.GetFuture();
               }()
             : [&]() -> TFuture<MemoryTypes::MemoryStoreRecallResult> {
                 TSharedPtr<TPromise<MemoryTypes::MemoryStoreRecallResult>>
                     Promise = MakeShared<
                         TPromise<MemoryTypes::MemoryStoreRecallResult>>();

                 auto RuntimeStore = ConfigureStore();
                 FMemoryRecallRequest RecallRequest;
                 RecallRequest.Query = Query;
                 RecallRequest.Limit =
                     Limit < 0 ? Store.Config.MaxRecallResults : Limit;
                 RecallRequest.Threshold = 0.0f;

                 /**
                  * Dispatch the recall thunk
                  * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
                  */
                 auto ThunkResult = rtk::nodeMemoryRecallThunk(RecallRequest)(
                     [RuntimeStore](const rtk::AnyAction &a) {
                       return RuntimeStore.dispatch(a);
                     },
                     [RuntimeStore]() {
                       return RuntimeStore.getState();
                     });

                 /**
                  * Chain result back to the promise
                  * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
                  */
                 ThunkResult
                     .then([Promise](TArray<FMemoryItem> Results) {
                       Promise->SetValue(
                           MemoryTypes::make_right(FString(), Results));
                     })
                     .catch_([Promise](std::string Error) {
                       Promise->SetValue(MemoryTypes::make_left(
                           FString(Error.c_str()),
                           TArray<FMemoryItem>{}));
                     })
                     .execute();

                 return Promise->GetFuture();
               }();
}

/**
 * Embedding generation implementation
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
TFuture<MemoryTypes::MemoryStoreEmbeddingResult>
MemoryOps::GenerateEmbedding(const FMemoryStore &Store, const FString &Text) {
  return !Store.bInitialized
             ? [&]() -> TFuture<MemoryTypes::MemoryStoreEmbeddingResult> {
                 TPromise<MemoryTypes::MemoryStoreEmbeddingResult> Promise;
                 Promise.SetValue(MemoryTypes::make_left(
                     FString(TEXT("Memory store not initialized")),
                     TArray<float>{}));
                 return Promise.GetFuture();
               }()
             : Async(
                   EAsyncExecution::Thread,
                   [Store,
                    Text]() -> MemoryTypes::MemoryStoreEmbeddingResult {
                     try {
                       return MemoryInternal::SQLiteVSS::GenerateEmbedding(
                           Store.DatabaseHandle, Text);
                     } catch (const std::exception &e) {
                       return MemoryTypes::make_left(FString(e.what()),
                                                     TArray<float>{});
                     }
                   });
}

/**
 * Statistics implementation
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
FString MemoryOps::GetStatistics(const FMemoryStore &Store) {
  return MemoryInternal::GetMemoryStatistics(Store);
}

/**
 * Cleanup implementation
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
void MemoryOps::Cleanup(FMemoryStore &Store) {
  (Store.bInitialized && Store.DatabaseHandle != nullptr)
      ? [&]() {
          MemoryInternal::SQLiteVSS::CloseDatabase(Store.DatabaseHandle);
          Store.DatabaseHandle = nullptr;
          Store.bInitialized = false;
        }()
      : (void)0;
}
