#include "NativeEngine.h"
#include "LlamaFacade.h"
#include <memory>

namespace {

int32 EarliestStopRecursive(const FString &Value,
                            const TArray<FString> &StopTokens, int32 Index,
                            int32 EarliestStop) {
  return Index == StopTokens.Num()
             ? EarliestStop
             : EarliestStopRecursive(
                   Value, StopTokens, Index + 1,
                   (!StopTokens[Index].IsEmpty()
                            ? Value.Find(StopTokens[Index])
                            : INDEX_NONE) != INDEX_NONE &&
                           (EarliestStop == INDEX_NONE ||
                            Value.Find(StopTokens[Index]) < EarliestStop)
                       ? Value.Find(StopTokens[Index])
                       : EarliestStop);
}

bool ContainsStopTokenRecursive(const FString &Accumulated,
                                const TArray<FString> &StopTokens,
                                int32 Index) {
  return Index == StopTokens.Num()
             ? false
             : (!StopTokens[Index].IsEmpty() &&
                Accumulated.Contains(StopTokens[Index]))
                   ? true
                   : ContainsStopTokenRecursive(Accumulated, StopTokens,
                                                Index + 1);
}

/**
 * Truncates generated text at the earliest configured stop token.
 * User Story: As inference consumers, I need generated text cut at stop tokens
 * so local completions honor the same boundaries as configured runtimes.
 */
FString ApplyStopTokens(const FString &Value,
                        const TArray<FString> &StopTokens) {
  const int32 EarliestStop =
      EarliestStopRecursive(Value, StopTokens, 0, INDEX_NONE);
  return EarliestStop == INDEX_NONE ? Value : Value.Left(EarliestStop);
}

constexpr int32 EmbeddingDimensions = 384;
const char *Utf8Bytes(const UTF8CHAR *Chars) {
  return reinterpret_cast<const char *>(Chars);
}

} // namespace

namespace Native {
namespace Llama {

/**
 * Loads the primary inference model and returns an opaque native context.
 * User Story: As local-cortex setup, I need the primary model loaded into a
 * native context so inference requests can execute locally.
 */
Context LoadModel(const FString &Path) {
#if WITH_FORBOC_NATIVE
  auto Utf8Path = StringCast<UTF8CHAR>(*Path);
  return reinterpret_cast<Context>(
      LlamaFacade::LoadInferenceModel(Utf8Bytes(Utf8Path.Get())));
#else
  UE_LOG(LogTemp, Error, TEXT("ForbocAI: LoadModel requires WITH_FORBOC_NATIVE=1. Native libs not available."));
  return nullptr;
#endif
}

/**
 * Loads the embedding model and returns an opaque native context.
 * User Story: As local-vector setup, I need the embedding model loaded into a
 * native context so text can be converted into vectors locally.
 */
Context LoadEmbeddingModel(const FString &Path) {
#if WITH_FORBOC_NATIVE
  auto Utf8Path = StringCast<UTF8CHAR>(*Path);
  return reinterpret_cast<Context>(
      LlamaFacade::LoadEmbeddingModel(Utf8Bytes(Utf8Path.Get())));
#else
  (void)Path;
  return nullptr;
#endif
}

/**
 * Frees a previously loaded native model context.
 * User Story: As native-runtime teardown, I need loaded model contexts freed so
 * local inference and embedding do not leak native resources.
 */
void FreeModel(Context Ctx) {
  !Ctx ? void()
       :
#if WITH_FORBOC_NATIVE
       (LlamaFacade::FreeContext(
            reinterpret_cast<struct llama_facade_context *>(Ctx)),
        void())
#else
       void()
#endif
      ;
}

/**
 * Generates a fixed-width embedding vector for the supplied text.
 * User Story: As local-vector workflows, I need text embedded into a fixed
 * vector shape so semantic search can use node-backed memory locally.
 */
TArray<float> Embed(Context Ctx, const FString &Text) {
#if WITH_FORBOC_NATIVE
  return Ctx
             ? [&]() -> TArray<float> {
                 TArray<float> Out;
                 Out.SetNum(EmbeddingDimensions);
                 auto Utf8 = StringCast<UTF8CHAR>(*Text);
                 return LlamaFacade::Embed(
                            reinterpret_cast<struct llama_facade_context *>(Ctx),
                            Utf8Bytes(Utf8.Get()), Out.GetData(),
                            EmbeddingDimensions)
                            ? Out
                            : [&]() -> TArray<float> {
                                UE_LOG(LogTemp, Error,
                                       TEXT("ForbocAI: Embed failed — native "
                                            "embedding model required."));
                                return TArray<float>();
                              }();
               }()
             : [&]() -> TArray<float> {
                 UE_LOG(LogTemp, Error,
                        TEXT("ForbocAI: Embed failed — native embedding model "
                             "required."));
                 return TArray<float>();
               }();
#else
  UE_LOG(LogTemp, Error, TEXT("ForbocAI: Embed failed — native embedding model required."));
  return TArray<float>();
#endif
}

/**
 * Runs plain text inference with a simple max-token limit.
 * User Story: As local-cortex workflows, I need a simple inference path so
 * prompts can be completed even without advanced config options.
 */
FString Infer(Context Ctx, const FString &Prompt, int32 MaxTokens) {
  return !Ctx
             ? TEXT("Error: Model not loaded")
             :
#if WITH_FORBOC_NATIVE
             [&]() -> FString {
               auto Utf8Prompt = StringCast<UTF8CHAR>(*Prompt);
               char *Result = LlamaFacade::Infer(
                   reinterpret_cast<struct llama_facade_context *>(Ctx),
                   Utf8Bytes(Utf8Prompt.Get()), MaxTokens, 0.8f);
               return Result ? [&]() -> FString {
                 FString Out(UTF8_TO_TCHAR(Result));
                 free(Result);
                 return Out;
               }()
                              : TEXT("Error: Inference failed");
             }()
#else
             [&]() -> FString {
               UE_LOG(LogTemp, Error,
                      TEXT("ForbocAI: Infer requires WITH_FORBOC_NATIVE=1. "
                           "Native libs not available."));
               return FString(TEXT("Error: Native inference not available"));
             }()
#endif
      ;
}

/**
 * Runs configured inference, including optional grammar and stop handling.
 * User Story: As local-cortex workflows, I need config-aware inference so
 * grammar, temperature, and stop tokens are honored locally. Supplying a GBNF
 * grammar routes through grammar-constrained inference before stop-token
 * truncation is applied.
 */
FString Infer(Context Ctx, const FString &Prompt, const FCortexConfig &Config) {
  return !Config.GbnfGrammar.IsEmpty()
             ?
#if WITH_FORBOC_NATIVE
             (!Ctx ? TEXT("Error: Model not loaded")
                   : [&]() -> FString {
                       auto Utf8Prompt = StringCast<UTF8CHAR>(*Prompt);
                       auto Utf8Grammar =
                           StringCast<UTF8CHAR>(*Config.GbnfGrammar);
                       char *Result = LlamaFacade::InferWithGrammar(
                           reinterpret_cast<struct llama_facade_context *>(Ctx),
                           Utf8Bytes(Utf8Prompt.Get()), Config.MaxTokens,
                           Config.Temperature > 0.0f ? Config.Temperature
                                                     : 0.8f,
                           Utf8Bytes(Utf8Grammar.Get()));
                       return Result ? [&]() -> FString {
                         FString Out(UTF8_TO_TCHAR(Result));
                         free(Result);
                         return ApplyStopTokens(Out, Config.Stop);
                       }()
                                     : TEXT("Error: Grammar-constrained "
                                            "inference failed");
                     }())
#else
             [&]() -> FString {
               UE_LOG(LogTemp, Error,
                      TEXT("ForbocAI: Grammar-constrained inference requires "
                           "WITH_FORBOC_NATIVE=1."));
               return FString(TEXT("Error: Native inference not available"));
             }()
#endif
             : ApplyStopTokens(Infer(Ctx, Prompt, Config.MaxTokens),
                               Config.Stop);
}

/**
 * Streams inference tokens to the caller while respecting stop tokens.
 * User Story: As streaming inference consumers, I need token callbacks and
 * stop-token handling so local streaming mirrors runtime expectations. Token
 * forwarding stops as soon as the accumulated output contains a stop sequence.
 */
FString InferStream(Context Ctx, const FString &Prompt,
                    const FCortexConfig &Config,
                    const TokenCallback &OnToken) {
  return !Ctx
             ? TEXT("Error: Model not loaded")
             :
#if WITH_FORBOC_NATIVE
             [&]() -> FString {
               struct StreamState {
                 FString Accumulated;
                 TokenCallback Callback;
                 TArray<FString> StopTokens;
                 bool bStopped;
               };
               StreamState State;
               State.Callback = OnToken;
               State.StopTokens = Config.Stop;
               State.bStopped = false;

               auto Utf8Prompt = StringCast<UTF8CHAR>(*Prompt);
               LlamaFacade::InferStream(
                   reinterpret_cast<struct llama_facade_context *>(Ctx),
                   Utf8Bytes(Utf8Prompt.Get()), Config.MaxTokens, 0.8f,
                   [](const char *TokenUtf8, int Len, void *UserData) {
                     StreamState *S = static_cast<StreamState *>(UserData);
                     !S->bStopped
                         ? [&]() {
                             FString Token(Len, UTF8_TO_TCHAR(TokenUtf8));
                             S->Accumulated += Token;
                             S->bStopped = ContainsStopTokenRecursive(
                                 S->Accumulated, S->StopTokens, 0);
                             !S->bStopped ? (S->Callback(Token), void())
                                          : void();
                           }()
                         : void();
                   },
                   &State);

               return ApplyStopTokens(State.Accumulated, Config.Stop);
             }()
#else
             [&]() -> FString {
               UE_LOG(LogTemp, Error,
                      TEXT("ForbocAI: InferStream requires "
                           "WITH_FORBOC_NATIVE=1. Native libs not available."));
               return FString(TEXT("Error: Native inference not available"));
             }()
#endif
      ;
}

} // namespace Llama
} // namespace Native