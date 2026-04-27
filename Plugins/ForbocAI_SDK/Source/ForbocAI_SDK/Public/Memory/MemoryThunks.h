#pragma once

#include "Core/ThunkDetail.h"
#include "Cortex/CortexSlice.h"
#include "Memory/MemorySlice.h"

namespace rtk {

/**
 * Forward declarations
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
inline ThunkAction<FMemoryItem, FStoreState>
nodeMemoryStoreThunk(const FMemoryItem &Item);

inline ThunkAction<TArray<FMemoryItem>, FStoreState>
nodeMemoryRecallThunk(const FMemoryRecallRequest &Request);

/**
 * User Story: As memory persistence setup, I need validated DB/table paths so
 * vector storage cannot escape the intended infrastructure directory. (From TS)
 */

inline ThunkAction<rtk::FEmptyPayload, FStoreState>
initNodeMemoryThunk(const FString &DatabasePath = TEXT("")) {
  return [DatabasePath](std::function<AnyAction(const AnyAction &)> Dispatch,
                        std::function<FStoreState()> GetState)
             -> func::AsyncResult<rtk::FEmptyPayload> {
    return func::AsyncResult<rtk::FEmptyPayload>::create(
        [DatabasePath](std::function<void(rtk::FEmptyPayload)> Resolve,
                       std::function<void(std::string)> Reject) {
          Async(EAsyncExecution::Thread, [DatabasePath, Resolve, Reject]() {
            Native::Sqlite::DB &Handle = detail::NodeMemoryHandle();
            Handle
                ? (Native::Sqlite::Close(Handle), (void)(Handle = nullptr))
                : (void)0;

            const FString Path = DatabasePath.IsEmpty()
                                     ? detail::DefaultNodeMemoryPath()
                                     : DatabasePath;
            detail::NodeMemoryPathStorage() = Path;
            Handle = Native::Sqlite::Open(Path);

            AsyncTask(ENamedThreads::GameThread, [Handle, Resolve, Reject]() {
              Handle
                  ? (Resolve(rtk::FEmptyPayload{}), void())
                  : (Reject("Failed to initialize node memory database"),
                     void());
            });
          });
        });
  };
}

inline ThunkAction<FMemoryItem, FStoreState>
nodeMemoryStoreThunk(const FMemoryItem &Item) {
  return [Item](std::function<AnyAction(const AnyAction &)> Dispatch,
                std::function<FStoreState()> GetState)
             -> func::AsyncResult<FMemoryItem> {
    Dispatch(MemorySlice::Actions::MemoryStoreStart());

    return func::AsyncResult<FMemoryItem>::create(
        [Item, Dispatch](std::function<void(FMemoryItem)> Resolve,
                         std::function<void(std::string)> Reject) {
          Async(EAsyncExecution::Thread, [Item, Dispatch, Resolve, Reject]() {
            Native::Sqlite::DB Db = detail::EnsureNodeMemoryDatabase();
            !Db
                ? [&]() {
                    const FString Error =
                        TEXT("Local memory is not initialized");
                    AsyncTask(ENamedThreads::GameThread,
                              [Dispatch, Reject, Error]() {
                                Dispatch(MemorySlice::Actions::
                                             MemoryStoreFailed(Error));
                                Reject(TCHAR_TO_UTF8(*Error));
                              });
                  }()
                : [&]() {
                    FMemoryItem Stored = Item;
                    Stored.Embedding = Native::Llama::Embed(
                        detail::NodeEmbeddingHandle(), Stored.Text);
                    const bool bStored =
                        Native::Sqlite::Upsert(Db, Stored, Stored.Embedding);

                    AsyncTask(
                        ENamedThreads::GameThread,
                        [Dispatch, Resolve, Reject, Stored, bStored]() {
                          bStored
                              ? (Dispatch(MemorySlice::Actions::
                                              MemoryStoreSuccess(Stored)),
                                 Resolve(Stored), void())
                              : [&]() {
                                  const FString Error =
                                      TEXT("Failed to store local memory");
                                  Dispatch(MemorySlice::Actions::
                                               MemoryStoreFailed(Error));
                                  Reject(TCHAR_TO_UTF8(*Error));
                                }();
                        });
                  }();
          });
        });
  };
}

inline ThunkAction<TArray<FMemoryItem>, FStoreState>
nodeMemoryRecallThunk(const FMemoryRecallRequest &Request) {
  return [Request](std::function<AnyAction(const AnyAction &)> Dispatch,
                   std::function<FStoreState()> GetState)
             -> func::AsyncResult<TArray<FMemoryItem>> {
    Dispatch(MemorySlice::Actions::MemoryRecallStart());

    return func::AsyncResult<TArray<FMemoryItem>>::create(
        [Request, Dispatch](std::function<void(TArray<FMemoryItem>)> Resolve,
                            std::function<void(std::string)> Reject) {
          Async(EAsyncExecution::Thread, [Request, Dispatch, Resolve,
                                          Reject]() {
            Native::Sqlite::DB Db = detail::EnsureNodeMemoryDatabase();
            !Db
                ? [&]() {
                    const FString Error =
                        TEXT("Local memory is not initialized");
                    AsyncTask(ENamedThreads::GameThread,
                              [Dispatch, Reject, Error]() {
                                Dispatch(MemorySlice::Actions::
                                             MemoryRecallFailed(Error));
                                Reject(TCHAR_TO_UTF8(*Error));
                              });
                  }()
                : [&]() {
                    const TArray<float> QueryEmbedding =
                        Native::Llama::Embed(detail::NodeEmbeddingHandle(),
                                             Request.Query);
                    TArray<FMemoryItem> Results =
                        Native::Sqlite::Search(Db, QueryEmbedding,
                                               Request.Limit);

                    Request.Threshold > 0.0f
                        ? (void)Results.RemoveAll(
                              [Request](const FMemoryItem &Item) {
                                return Item.Similarity < Request.Threshold;
                              })
                        : (void)0;

                    AsyncTask(ENamedThreads::GameThread,
                              [Dispatch, Resolve, Results]() {
                                Dispatch(MemorySlice::Actions::
                                             MemoryRecallSuccess(Results));
                                Resolve(Results);
                              });
                  }();
          });
        });
  };
}

/**
 * Convenience wrappers
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
inline ThunkAction<FMemoryItem, FStoreState>
storeNodeMemoryThunk(const FString &Text,
                     const FString &Type = TEXT("observation"),
                     float Importance = 0.5f) {
  FMemoryStoreInstruction Instruction;
  Instruction.Text = Text;
  Instruction.Type = Type;
  Instruction.Importance = Importance;
  return nodeMemoryStoreThunk(detail::MakeMemoryItem(Instruction));
}

inline ThunkAction<TArray<FMemoryItem>, FStoreState>
recallNodeMemoryThunk(const FString &Query, int32 Limit = 10,
                      float Threshold = 0.7f) {
  FMemoryRecallRequest Request;
  Request.Query = Query;
  Request.Limit = Limit;
  Request.Threshold = Threshold;
  return nodeMemoryRecallThunk(Request);
}

inline ThunkAction<rtk::FEmptyPayload, FStoreState> clearNodeMemoryThunk() {
  return [](std::function<AnyAction(const AnyAction &)> Dispatch,
            std::function<FStoreState()> GetState)
             -> func::AsyncResult<rtk::FEmptyPayload> {
    return func::AsyncResult<rtk::FEmptyPayload>::create(
        [Dispatch](std::function<void(rtk::FEmptyPayload)> Resolve,
                   std::function<void(std::string)> Reject) {
          Async(EAsyncExecution::Thread, [Dispatch, Resolve]() {
            Native::Sqlite::DB &Handle = detail::NodeMemoryHandle();
            const FString Path = detail::NodeMemoryPathStorage();
            Handle
                ? (Native::Sqlite::Clear(Handle),
                   Native::Sqlite::Close(Handle),
                   (void)(Handle = nullptr))
                : (void)Native::Sqlite::ClearPath(Path);

            IFileManager::Get().Delete(*Path, false, true, true);
            detail::NodeMemoryPathStorage() = detail::DefaultNodeMemoryPath();

            AsyncTask(ENamedThreads::GameThread, [Dispatch, Resolve]() {
              Dispatch(MemorySlice::Actions::MemoryClear());
              Resolve(rtk::FEmptyPayload{});
            });
          });
        });
  };
}

inline ThunkAction<rtk::FEmptyPayload, FStoreState>
initNodeVectorThunk(const FString &EmbeddingModelPath = TEXT("")) {
  return [EmbeddingModelPath](std::function<AnyAction(const AnyAction &)> Dispatch,
                              std::function<FStoreState()> GetState)
             -> func::AsyncResult<rtk::FEmptyPayload> {
    static const FString EmbeddingUrl =
        TEXT("https://huggingface.co/second-state/All-MiniLM-L6-v2-Embedding-GGUF/"
             "resolve/main/all-MiniLM-L6-v2-Q4_K_M.gguf");

    return func::AsyncResult<rtk::FEmptyPayload>::create(
        [EmbeddingModelPath, Dispatch](
            std::function<void(rtk::FEmptyPayload)> Resolve,
            std::function<void(std::string)> Reject) {
#if WITH_FORBOC_NATIVE
          const FString Path = EmbeddingModelPath.IsEmpty()
                                   ? detail::DefaultEmbeddingModelPath()
                                   : EmbeddingModelPath;

          auto LoadOnWorker = [Path, Dispatch, Resolve, Reject]() {
            Async(EAsyncExecution::Thread, [Path, Dispatch, Resolve, Reject]() {
              Native::Llama::Context &Handle = detail::NodeEmbeddingHandle();
              Handle
                  ? (Native::Llama::FreeModel(Handle),
                     (void)(Handle = nullptr))
                  : (void)0;

              Handle = Native::Llama::LoadEmbeddingModel(Path);

              AsyncTask(ENamedThreads::GameThread,
                        [Handle, Dispatch, Resolve, Reject]() {
                          Handle
                              ? (
                                    /**
                                     * G3: Dispatch embedder readiness
                                     * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
                                     */
                                    Dispatch(CortexSlice::Actions::
                                                 SetEmbedderReady(true)),
                                    Resolve(rtk::FEmptyPayload{}), void())
                              : (Dispatch(CortexSlice::Actions::
                                              SetEmbedderReady(false)),
                                 Reject("Failed to load embedding model"),
                                 void());
                        });
            });
          };

          !FPaths::FileExists(Path)
              ? (Native::File::DownloadBinary(EmbeddingUrl, Path)
                     .then([LoadOnWorker](const FString &) mutable {
                       LoadOnWorker();
                     })
                     .catch_([Reject](std::string Error) mutable {
                       Reject(Error.empty()
                                  ? std::string("Embedding download failed")
                                  : Error);
                     })
                     .execute(),
                 void())
              : (LoadOnWorker(), void());
#else
          static_cast<void>(EmbeddingModelPath);
          /**
           * G3: Stub mode — embedder is "ready" immediately
           * User Story: As a maintainer, I need this implementation note so I can understand which milestone behavior the surrounding code is preserving.
           */
          Dispatch(CortexSlice::Actions::SetEmbedderReady(true));
          Resolve(rtk::FEmptyPayload{});
#endif
        });
  };
}

/**
 * Remote memory thunks (mirrors TS core thunks.ts)
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */

inline ThunkAction<rtk::FEmptyPayload, FStoreState>
storeMemoryRemoteThunk(const FString &NpcId, const FString &Observation,
                       float Importance = 0.8f) {
  return [NpcId, Observation,
          Importance](std::function<AnyAction(const AnyAction &)> Dispatch,
                      std::function<FStoreState()> GetState)
             -> func::AsyncResult<rtk::FEmptyPayload> {
    return APISlice::Endpoints::postMemoryStore(
        NpcId, TypeFactory::RemoteMemoryStoreRequest(Observation, Importance))(
        Dispatch, GetState);
  };
}

inline ThunkAction<TArray<FMemoryItem>, FStoreState>
listMemoryRemoteThunk(const FString &NpcId) {
  return [NpcId](std::function<AnyAction(const AnyAction &)> Dispatch,
                 std::function<FStoreState()> GetState)
             -> func::AsyncResult<TArray<FMemoryItem>> {
    return func::AsyncChain::then<TArray<FMemoryItem>, TArray<FMemoryItem>>(
        APISlice::Endpoints::getMemoryList(NpcId)(Dispatch, GetState),
        [Dispatch](const TArray<FMemoryItem> &Items) {
          Dispatch(MemorySlice::Actions::MemoryRecallSuccess(Items));
          return detail::ResolveAsync(Items);
        });
  };
}

inline ThunkAction<TArray<FMemoryItem>, FStoreState>
recallMemoryRemoteThunk(const FString &NpcId, const FString &Query,
                        float Similarity = 0.0f) {
  return [NpcId, Query,
          Similarity](std::function<AnyAction(const AnyAction &)> Dispatch,
                      std::function<FStoreState()> GetState)
             -> func::AsyncResult<TArray<FMemoryItem>> {
    Dispatch(MemorySlice::Actions::MemoryRecallStart());
    return func::AsyncChain::then<TArray<FMemoryItem>, TArray<FMemoryItem>>(
               APISlice::Endpoints::postMemoryRecall(
                   NpcId, TypeFactory::RemoteMemoryRecallRequest(
                              Query, Similarity))(Dispatch, GetState),
               [Dispatch](const TArray<FMemoryItem> &Items) {
                 Dispatch(MemorySlice::Actions::MemoryRecallSuccess(Items));
                 return detail::ResolveAsync(Items);
               })
        .catch_([Dispatch](std::string Error) {
          Dispatch(MemorySlice::Actions::MemoryRecallFailed(
              FString(UTF8_TO_TCHAR(Error.c_str()))));
        });
  };
}

inline ThunkAction<rtk::FEmptyPayload, FStoreState>
clearMemoryRemoteThunk(const FString &NpcId) {
  return [NpcId](std::function<AnyAction(const AnyAction &)> Dispatch,
                 std::function<FStoreState()> GetState)
             -> func::AsyncResult<rtk::FEmptyPayload> {
    return func::AsyncChain::then<rtk::FEmptyPayload, rtk::FEmptyPayload>(
        APISlice::Endpoints::deleteMemoryClear(NpcId)(Dispatch, GetState),
        [Dispatch](const rtk::FEmptyPayload &Payload) {
          Dispatch(MemorySlice::Actions::MemoryClear());
          return detail::ResolveAsync(Payload);
        });
  };
}

} // namespace rtk
