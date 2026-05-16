#pragma once
/**
 * asynchronous moves, zero hidden side quests
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */

#include "Core/ThunkDetail.h"
#include "Core/functional_core.hpp"
#include "Cortex/CortexSlice.h"
#include "Errors.h"
#include "RuntimeConfig.h"

namespace rtk {

/**
 * Forward declarations for cross-thunk references
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
inline ThunkAction<FCortexResponse, FStoreState>
completeNodeCortexThunk(const FString &Prompt,
                        const FCortexConfig &Config = FCortexConfig());

inline ThunkAction<FCortexResponse, FStoreState>
completeRemoteThunk(const FString &CortexId, const FString &Prompt,
                    const FCortexConfig &Config);

/**
 * User Story: As model bootstrap logic, I need reliable file download with
 * redirect handling so required GGUF assets arrive before init. (From TS)
 */

inline ThunkAction<FCortexStatus, FStoreState>
initNodeCortexThunk(const FString &ModelPath) {
  return [ModelPath](std::function<AnyAction(const AnyAction &)> Dispatch,
                     std::function<FStoreState()> GetState)
             -> func::AsyncResult<FCortexStatus> {
    Dispatch(CortexSlice::Actions::CortexInitPending(ModelPath));

    return func::AsyncResult<FCortexStatus>::create(
        [ModelPath, Dispatch](std::function<void(FCortexStatus)> Resolve,
                              std::function<void(std::string)> Reject) {
#if WITH_FORBOC_NATIVE
          /**
           * Map known model ids to Hugging Face GGUF URLs, mirroring TS SDK.
           * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
           */
          const FString DefaultModelId = TEXT("smollm2-135m");
          const FString SmolUrl =
              TEXT("https://huggingface.co/bartowski/SmolLM2-135M-Instruct-"
                   "GGUF/resolve/main/SmolLM2-135M-Instruct-Q4_K_M.gguf");
          const FString Llama3Url =
              TEXT("https://huggingface.co/lmstudio-community/Meta-Llama-3-8B-"
                   "Instruct-GGUF/resolve/main/Meta-Llama-3-8B-Instruct-"
                   "Q4_K_M.gguf");

          FString EffectiveModel = ModelPath.IsEmpty() ? DefaultModelId : ModelPath;
          const FString Url = func::or_else(
              func::multi_match<FString, FString>(
                  EffectiveModel,
                  {func::when<FString, FString>(
                       [](const FString &M) {
                         return M.Equals(TEXT("smollm2-135m"),
                                         ESearchCase::IgnoreCase);
                       },
                       [&SmolUrl](const FString &) { return SmolUrl; }),
                   func::when<FString, FString>(
                       [](const FString &M) {
                         return M.Equals(TEXT("llama3-8b"),
                                         ESearchCase::IgnoreCase);
                       },
                       [&Llama3Url](const FString &) {
                         return Llama3Url;
                       })}),
              FString());

          const FString ModelsDir =
              detail::GetLocalInfrastructureDir() + TEXT("models/");
          const FString FileName =
              Url.IsEmpty() ? EffectiveModel : FPaths::GetCleanFilename(Url);
          const FString LocalPath =
              FPaths::ConvertRelativePathToFull(ModelsDir / FileName);

          auto LoadOnWorker = [LocalPath, EffectiveModel, Dispatch, Resolve,
                               Reject]() {
            Async(EAsyncExecution::Thread,
                  [LocalPath, EffectiveModel, Dispatch, Resolve, Reject]() {
                    Native::Llama::Context &Handle = detail::NodeCortexHandle();
                    Handle
                        ? (Native::Llama::FreeModel(Handle),
                           (void)(Handle = nullptr))
                        : (void)0;

                    Handle = Native::Llama::LoadModel(LocalPath);

                    FCortexStatus Status;
                    Status.Id = TEXT("local-llama");
                    Status.Model = EffectiveModel;
                    Status.bReady = (Handle != nullptr);
                    Status.Engine = ECortexEngine::NodeLlamaCpp;
                    Status.DownloadProgress = Status.bReady ? 1.0f : 0.0f;
                    Status.Error = Status.bReady
                                       ? TEXT("")
                                       : TEXT("Failed to load model");

                    AsyncTask(ENamedThreads::GameThread,
                              [Dispatch, Resolve, Reject, Status]() {
                                Status.bReady
                                    ? (Dispatch(CortexSlice::Actions::
                                                    CortexInitFulfilled(
                                                        Status)),
                                       Resolve(Status), void())
                                    : (Dispatch(CortexSlice::Actions::
                                                    CortexInitRejected(
                                                        Status.Error)),
                                       Reject(TCHAR_TO_UTF8(*Status.Error)),
                                       void());
                              });
                  });
          };

          /**
           * Download first when URL is known and file is missing.
           * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
           */
          (!Url.IsEmpty() && !FPaths::FileExists(LocalPath))
              ? (
                    /**
                     * G2: Dispatch download start
                     * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
                     */
                    Dispatch(
                        CortexSlice::Actions::SetDownloadState(true, 0.0f)),
                    Native::File::DownloadBinary(Url, LocalPath)
                        .then([LoadOnWorker,
                               Dispatch](const FString &) mutable {
                          /**
                           * G2: Dispatch download complete
                           * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
                           */
                          Dispatch(CortexSlice::Actions::SetDownloadState(
                              false, 100.0f));
                          LoadOnWorker();
                        })
                        .catch_([Dispatch,
                                 Reject](std::string Error) mutable {
                          /**
                           * G2: Clear download state on failure
                           * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
                           */
                          Dispatch(CortexSlice::Actions::SetDownloadState(
                              false, 0.0f));
                          const FString Msg =
                              Error.empty()
                                  ? TEXT("Model download failed")
                                  : FString(UTF8_TO_TCHAR(Error.c_str()));
                          Dispatch(
                              CortexSlice::Actions::CortexInitRejected(Msg));
                          Reject(TCHAR_TO_UTF8(*Msg));
                        })
                        .execute(),
                    void())
              : (
                    /**
                     * Either local path already exists, or user passed a custom path.
                     * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
                     */
                    LoadOnWorker(), void());
#else
          static_cast<void>(ModelPath);
          FCortexStatus Status;
          Status.Id = TEXT("local-llama");
          Status.Model = ModelPath;
          Status.bReady = false;
          Status.Engine = ECortexEngine::NodeLlamaCpp;
          Status.DownloadProgress = 0.0f;
          Status.Error = TEXT("Native cortex not available on this build");
          Dispatch(CortexSlice::Actions::CortexInitRejected(Status.Error));
          Reject(TCHAR_TO_UTF8(*Status.Error));
#endif
        });
  };
}

inline ThunkAction<FCortexResponse, FStoreState>
completeNodeCortexThunk(const FString &Prompt, const FCortexConfig &Config) {
  return [Prompt, Config](std::function<AnyAction(const AnyAction &)> Dispatch,
                          std::function<FStoreState()> GetState)
             -> func::AsyncResult<FCortexResponse> {
    Dispatch(CortexSlice::Actions::CortexCompletePending(Prompt));

    return func::AsyncResult<FCortexResponse>::create(
        [Prompt, Config, Dispatch](std::function<void(FCortexResponse)> Resolve,
                                   std::function<void(std::string)> Reject) {
          Async(EAsyncExecution::Thread, [Prompt, Config, Dispatch, Resolve,
                                          Reject]() {
            Native::Llama::Context Handle = detail::NodeCortexHandle();
            !Handle
                ? [&]() {
                    const FString Error =
                        TEXT("Local cortex is not initialized");
                    AsyncTask(ENamedThreads::GameThread,
                              [Dispatch, Reject, Error]() {
                                Dispatch(CortexSlice::Actions::
                                             CortexCompleteRejected(Error));
                                Reject(TCHAR_TO_UTF8(*Error));
                              });
                  }()
                : [&]() {
                    FCortexResponse Response;
                    Response.Id = FGuid::NewGuid().ToString();
                    Response.Text =
                        Native::Llama::Infer(Handle, Prompt, Config);
                    Response.Stats = TEXT("local-node");

                    AsyncTask(ENamedThreads::GameThread,
                              [Dispatch, Resolve, Response]() {
                                Dispatch(CortexSlice::Actions::
                                             CortexCompleteFulfilled(
                                                 Response));
                                Resolve(Response);
                              });
                  }();
          });
        });
  };
}

/**
 * User Story: As completion thunks and adapters, I need access to cached node
 * context sessions by model key after initialization. (From TS)
 */
inline ThunkAction<TArray<float>, FStoreState>
generateNodeEmbeddingThunk(const FString &Text) {
  return [Text](std::function<AnyAction(const AnyAction &)> Dispatch,
                std::function<FStoreState()> GetState)
             -> func::AsyncResult<TArray<float>> {
    return func::AsyncResult<TArray<float>>::create(
        [Text](std::function<void(TArray<float>)> Resolve,
               std::function<void(std::string)> Reject) {
          Async(EAsyncExecution::Thread, [Text, Resolve]() {
            TArray<float> Embedding =
                Native::Llama::Embed(detail::NodeEmbeddingHandle(), Text);
            AsyncTask(ENamedThreads::GameThread,
                      [Resolve, Embedding]() { Resolve(Embedding); });
          });
        });
  };
}

/**
 * G7: Streaming node cortex thunk (mirrors TS stream.ts)
 * Token-by-token callback, dispatches StreamToken per token on game thread
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */

using FOnTokenCallback = std::function<void(const FString &Token)>;

inline ThunkAction<FCortexResponse, FStoreState>
streamNodeCortexThunk(const FString &Prompt, const FCortexConfig &Config,
                      const FOnTokenCallback &OnToken) {
  return [Prompt, Config, OnToken](
             std::function<AnyAction(const AnyAction &)> Dispatch,
             std::function<FStoreState()> GetState)
             -> func::AsyncResult<FCortexResponse> {
    Dispatch(CortexSlice::Actions::CortexStreamStart(Prompt));

    return func::AsyncResult<FCortexResponse>::create(
        [Prompt, Config, OnToken, Dispatch](
            std::function<void(FCortexResponse)> Resolve,
            std::function<void(std::string)> Reject) {
          Async(EAsyncExecution::Thread,
                [Prompt, Config, OnToken, Dispatch, Resolve, Reject]() {
                  Native::Llama::Context Handle = detail::NodeCortexHandle();
                  !Handle
                      ? [&]() {
                          const FString Error =
                              TEXT("Local cortex is not initialized");
                          AsyncTask(ENamedThreads::GameThread,
                                    [Dispatch, Reject, Error]() {
                                      Dispatch(CortexSlice::Actions::
                                                   CortexCompleteRejected(
                                                       Error));
                                      Reject(TCHAR_TO_UTF8(*Error));
                                    });
                        }()
                      : [&]() {
                          FString FullText = Native::Llama::InferStream(
                              Handle, Prompt, Config,
                              [&Dispatch, &OnToken](const FString &Token) {
                                /**
                                 * Forward each token to game thread
                                 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
                                 */
                                AsyncTask(
                                    ENamedThreads::GameThread,
                                    [Dispatch, OnToken, Token]() {
                                      Dispatch(CortexSlice::Actions::
                                                   CortexStreamToken(Token));
                                      OnToken
                                          ? (OnToken(Token), void())
                                          : void();
                                    });
                              });

                          FCortexResponse Response;
                          Response.Id = FGuid::NewGuid().ToString();
                          Response.Text = FullText;
                          Response.Stats = TEXT("local-node-stream");

                          AsyncTask(ENamedThreads::GameThread,
                                    [Dispatch, Resolve, Response]() {
                                      Dispatch(CortexSlice::Actions::
                                                   CortexStreamComplete(
                                                       Response.Text));
                                      Dispatch(CortexSlice::Actions::
                                                   CortexCompleteFulfilled(
                                                       Response));
                                      Resolve(Response);
                                    });
                        }();
                });
        });
  };
}

/**
 * Remote cortex thunks (mirrors TS cortexSlice.ts)
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */

inline ThunkAction<TArray<FCortexModelInfo>, FStoreState>
listCortexModelsThunk() {
  return [](std::function<AnyAction(const AnyAction &)> Dispatch,
            std::function<FStoreState()> GetState)
             -> func::AsyncResult<TArray<FCortexModelInfo>> {
    const auto ApiKeyError = Errors::requireApiKeyGuidance(
        SDKConfig::GetApiUrl(), SDKConfig::GetApiKey());
    return ApiKeyError.hasValue
        ? detail::RejectAsync<TArray<FCortexModelInfo>>(ApiKeyError.value)
        : APISlice::Endpoints::getCortexModels()(Dispatch, GetState);
  };
}

inline ThunkAction<FCortexStatus, FStoreState>
initRemoteCortexThunk(const FString &Model = TEXT("api-integrated"),
                      const FString &AuthKey = TEXT("")) {
  return [Model, AuthKey](std::function<AnyAction(const AnyAction &)> Dispatch,
                          std::function<FStoreState()> GetState)
             -> func::AsyncResult<FCortexStatus> {
    const auto ApiKeyError = Errors::requireApiKeyGuidance(
        SDKConfig::GetApiUrl(), SDKConfig::GetApiKey());
    return ApiKeyError.hasValue
        ? detail::RejectAsync<FCortexStatus>(ApiKeyError.value)
        : (Dispatch(CortexSlice::Actions::CortexInitPending(Model)),
           func::AsyncChain::then<FCortexInitResponse, FCortexStatus>(
               APISlice::Endpoints::postCortexInit(
                   TypeFactory::CortexInitRequest(Model, AuthKey))(Dispatch,
                                                                   GetState),
               [Model, Dispatch](const FCortexInitResponse &Response) {
                 FCortexStatus Status;
                 Status.Id = Response.CortexId;
                 Status.Model = Model;
                 Status.bReady = Response.State.Equals(
                     TEXT("ready"), ESearchCase::IgnoreCase);
                 Status.Engine = ECortexEngine::Remote;
                 Status.DownloadProgress = Status.bReady ? 1.0f : 0.0f;
                 Status.Error = Status.bReady ? TEXT("") : Response.State;
                 return Status.bReady
                     ? (Dispatch(CortexSlice::Actions::CortexInitFulfilled(
                            Status)),
                        detail::ResolveAsync(Status))
                     : (Dispatch(CortexSlice::Actions::CortexInitRejected(
                            Status.Error)),
                        detail::RejectAsync<FCortexStatus>(
                            Status.Error.IsEmpty()
                                ? TEXT("Remote cortex init failed")
                                : Status.Error));
               })
               .catch_([Dispatch](std::string Error) {
                 Dispatch(CortexSlice::Actions::CortexInitRejected(
                     FString(UTF8_TO_TCHAR(Error.c_str()))));
               }));
  };
}

inline ThunkAction<FCortexResponse, FStoreState>
completeRemoteThunk(const FString &CortexId, const FString &Prompt) {
  return completeRemoteThunk(CortexId, Prompt, FCortexConfig());
}

inline ThunkAction<FCortexResponse, FStoreState>
completeRemoteThunk(const FString &CortexId, const FString &Prompt,
                    const FCortexConfig &Config) {
  return [CortexId, Prompt,
          Config](std::function<AnyAction(const AnyAction &)> Dispatch,
                  std::function<FStoreState()> GetState)
             -> func::AsyncResult<FCortexResponse> {
    const auto ApiKeyError = Errors::requireApiKeyGuidance(
        SDKConfig::GetApiUrl(), SDKConfig::GetApiKey());
    return ApiKeyError.hasValue
        ? detail::RejectAsync<FCortexResponse>(ApiKeyError.value)
        : (Dispatch(CortexSlice::Actions::CortexCompletePending(Prompt)),
           func::AsyncChain::then<FCortexResponse, FCortexResponse>(
               APISlice::Endpoints::postCortexComplete(
                   CortexId, TypeFactory::CortexCompleteRequest(
                                 Prompt, Config.MaxTokens, Config.Temperature,
                                 Config.Stop, Config.JsonSchemaJson))(Dispatch,
                                                                      GetState),
               [Dispatch](const FCortexResponse &Response) {
                 Dispatch(
                     CortexSlice::Actions::CortexCompleteFulfilled(Response));
                 return detail::ResolveAsync(Response);
               })
               .catch_([Dispatch](std::string Error) {
                 Dispatch(CortexSlice::Actions::CortexCompleteRejected(
                     FString(UTF8_TO_TCHAR(Error.c_str()))));
               }));
  };
}

} // namespace rtk
