/**
 * LlamaFacade — Wraps llama.cpp C API for inference and embeddings
 * When WITH_FORBOC_NATIVE=1: Requires real libllama.a and llama.cpp include
 * folder. API targets llama.cpp master (b5220+). Batch/sampler API may need
 * adjustment for your llama.cpp version.
 * When WITH_FORBOC_NATIVE=0: Stub implementation returns nullptr/false.
 * User Story: As a maintainer, I need this note so the surrounding API intent stays clear during maintenance and integration.
 */

#include "LlamaFacade.h"
#include "Core/functional_core.hpp"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#if WITH_FORBOC_NATIVE

#include "llama.h"

struct llama_facade_context {
  llama_model *Model;
  llama_context *Ctx;
  bool IsEmbedding;
};

/**
 * Recursive helper: accumulates sum-of-squares across a float array.
 */
static float SumSquaresRecursive(const float *Data, int N, int Index, float Acc) {
  return Index >= N ? Acc : SumSquaresRecursive(Data, N, Index + 1, Acc + Data[Index] * Data[Index]);
}

/**
 * Recursive helper: scales each element by InvNorm.
 */
static void ScaleRecursive(float *Data, int N, int Index, float InvNorm) {
  Index < N
      ? (Data[Index] *= InvNorm, ScaleRecursive(Data, N, Index + 1, InvNorm))
      : void();
}

static void NormalizeVector(float *Data, int N) {
  const float SumSq = SumSquaresRecursive(Data, N, 0, 0.0f);
  SumSq > 1e-10f
      ? ScaleRecursive(Data, N, 0, 1.0f / std::sqrt(SumSq))
      : void();
}

static void ClearBatch(llama_batch &Batch) { Batch.n_tokens = 0; }

static void AddBatchToken(llama_batch &Batch, llama_token Token, llama_pos Pos,
                          bool bLogits) {
  const int32_t Index = Batch.n_tokens++;
  Batch.token[Index] = Token;
  Batch.pos[Index] = Pos;
  Batch.n_seq_id[Index] = 1;
  Batch.seq_id[Index][0] = 0;
  Batch.logits[Index] = bLogits ? 1 : 0;
}

/**
 * Recursive helper: fills batch with tokens from the prompt token array.
 */
static int32_t FillBatchRecursive(llama_batch &Batch,
                                  const std::vector<llama_token> &Tokens,
                                  size_t Index, int32_t Pos) {
  return Index >= Tokens.size()
             ? Pos
             : (AddBatchToken(Batch, Tokens[Index], Pos, true),
                FillBatchRecursive(Batch, Tokens, Index + 1, Pos + 1));
}

/**
 * Recursive helper: runs the inference generation loop, appending to Output.
 */
static int GenerateRecursive(llama_facade_context *Ctx,
                             const llama_vocab *Vocab,
                             llama_sampler *Smpl,
                             llama_batch &Batch,
                             std::string &Output,
                             int MaxTokens, int Generated, int32_t Pos) {
  return Generated >= MaxTokens
             ? Generated
             : [&]() -> int {
                 const llama_token Next =
                     llama_sampler_sample(Smpl, Ctx->Ctx, -1);
                 return Next == llama_vocab_eos(Vocab)
                            ? Generated
                            : [&]() -> int {
                                char Buf[256];
                                const int Len = llama_token_to_piece(
                                    Vocab, Next, Buf, sizeof(Buf), 0, false);
                                Len > 0 ? (Output.append(
                                               Buf, static_cast<size_t>(Len)),
                                           void())
                                        : void();
                                ClearBatch(Batch);
                                AddBatchToken(Batch, Next, Pos, true);
                                return llama_decode(Ctx->Ctx, Batch) != 0
                                           ? Generated + 1
                                           : GenerateRecursive(
                                                 Ctx, Vocab, Smpl, Batch,
                                                 Output, MaxTokens,
                                                 Generated + 1, Pos + 1);
                              }();
               }();
}

/**
 * Recursive helper: runs the streaming generation loop, invoking callbacks.
 */
static int StreamRecursive(llama_facade_context *Ctx,
                           const llama_vocab *Vocab,
                           llama_sampler *Smpl,
                           llama_batch &Batch,
                           LlamaFacade::TokenCallback OnToken, void *UserData,
                           int MaxTokens, int Generated, int32_t Pos) {
  return Generated >= MaxTokens
             ? Generated
             : [&]() -> int {
                 const llama_token Next =
                     llama_sampler_sample(Smpl, Ctx->Ctx, -1);
                 return Next == llama_vocab_eos(Vocab)
                            ? Generated
                            : [&]() -> int {
                                char Buf[256];
                                const int Len = llama_token_to_piece(
                                    Vocab, Next, Buf, sizeof(Buf), 0, false);
                                Len > 0 ? (OnToken(Buf, Len, UserData), void())
                                        : void();
                                ClearBatch(Batch);
                                AddBatchToken(Batch, Next, Pos, true);
                                return llama_decode(Ctx->Ctx, Batch) != 0
                                           ? Generated + 1
                                           : StreamRecursive(
                                                 Ctx, Vocab, Smpl, Batch,
                                                 OnToken, UserData, MaxTokens,
                                                 Generated + 1, Pos + 1);
                              }();
               }();
}

/**
 * Recursive helper: copies embeddings from source to destination.
 */
static void CopyEmbeddingsRecursive(float *Out, const float *Emb, int Count,
                                     int Index) {
  Index < Count ? (Out[Index] = Emb[Index],
                   CopyEmbeddingsRecursive(Out, Emb, Count, Index + 1))
                : void();
}

/**
 * Recursive helper: zero-fills remaining embedding dimensions.
 */
static void ZeroFillRecursive(float *Out, int Dims, int Index) {
  Index < Dims
      ? (Out[Index] = 0.0f, ZeroFillRecursive(Out, Dims, Index + 1))
      : void();
}

/**
 * Ensures llama backend is initialized exactly once.
 */
static void EnsureBackendInit(bool &Initialized) {
  !Initialized ? (llama_backend_init(), Initialized = true, void()) : void();
}

namespace LlamaFacade {

static bool BackendInitialized = false;

llama_facade_context *LoadInferenceModel(const char *PathUtf8) {
  return (!PathUtf8 || !*PathUtf8)
             ? nullptr
             : [&]() -> llama_facade_context * {
                 EnsureBackendInit(BackendInitialized);

                 llama_model_params MParams = llama_model_default_params();
                 MParams.n_gpu_layers = -1;

                 llama_model *Model =
                     llama_model_load_from_file(PathUtf8, MParams);
                 return !Model
                            ? nullptr
                            : [&]() -> llama_facade_context * {
                                llama_context_params CParams =
                                    llama_context_default_params();
                                CParams.n_ctx = 2048;
                                CParams.n_batch = 512;
                                CParams.embeddings = false;

                                llama_context *Ctx =
                                    llama_init_from_model(Model, CParams);
                                return !Ctx
                                           ? (llama_model_free(Model), nullptr)
                                           : [&]() -> llama_facade_context * {
                                               std::unique_ptr<
                                                   llama_facade_context>
                                                   F(new llama_facade_context{
                                                       Model, Ctx, false});
                                               return F.release(); // Ownership transferred to caller; freed via FreeContext
                                             }();
                              }();
               }();
}

llama_facade_context *LoadEmbeddingModel(const char *PathUtf8) {
  return (!PathUtf8 || !*PathUtf8)
             ? nullptr
             : [&]() -> llama_facade_context * {
                 EnsureBackendInit(BackendInitialized);

                 llama_model_params MParams = llama_model_default_params();
                 MParams.n_gpu_layers = -1;

                 llama_model *Model =
                     llama_model_load_from_file(PathUtf8, MParams);
                 return !Model
                            ? nullptr
                            : [&]() -> llama_facade_context * {
                                llama_context_params CParams =
                                    llama_context_default_params();
                                CParams.n_ctx = 512;
                                CParams.n_batch = 512;
                                CParams.embeddings = true;
                                CParams.pooling_type =
                                    LLAMA_POOLING_TYPE_MEAN;

                                llama_context *Ctx =
                                    llama_init_from_model(Model, CParams);
                                return !Ctx
                                           ? (llama_model_free(Model), nullptr)
                                           : [&]() -> llama_facade_context * {
                                               std::unique_ptr<
                                                   llama_facade_context>
                                                   F(new llama_facade_context{
                                                       Model, Ctx, true});
                                               return F.release(); // Ownership transferred to caller; freed via FreeContext
                                             }();
                              }();
               }();
}

void FreeContext(llama_facade_context *Ctx) {
  Ctx ? (llama_free(Ctx->Ctx),
         llama_model_free(Ctx->Model),
         std::unique_ptr<llama_facade_context>(Ctx).reset(),
         void())
      : void();
  // unique_ptr destructor calls delete
}

char *Infer(llama_facade_context *Ctx, const char *PromptUtf8, int MaxTokens,
            float Temperature) {
  return (!Ctx || !PromptUtf8 || Ctx->IsEmbedding)
             ? nullptr
             : [&]() -> char * {
                 const llama_vocab *Vocab =
                     llama_model_get_vocab(Ctx->Model);
                 const int32_t PromptLen = static_cast<int32_t>(strlen(PromptUtf8));
                 const int32_t EstTokens = PromptLen + 2;

                 std::vector<llama_token> Tokens(EstTokens);
                 int32_t N = llama_tokenize(
                     Vocab, PromptUtf8, PromptLen, Tokens.data(),
                     static_cast<int32_t>(Tokens.size()), true, false);
                 N < 0 ? (Tokens.resize(static_cast<size_t>(-N)),
                          N = llama_tokenize(Vocab, PromptUtf8, PromptLen,
                                             Tokens.data(),
                                             static_cast<int32_t>(Tokens.size()),
                                             true, false),
                          void())
                       : void();
                 return N <= 0
                            ? nullptr
                            : [&]() -> char * {
                                Tokens.resize(static_cast<size_t>(N));

                                llama_sampler_chain_params SParams =
                                    llama_sampler_chain_default_params();
                                llama_sampler *Smpl =
                                    llama_sampler_chain_init(SParams);
                                llama_sampler_chain_add(
                                    Smpl, llama_sampler_init_top_k(40));
                                llama_sampler_chain_add(
                                    Smpl, llama_sampler_init_top_p(0.9f, 1));
                                llama_sampler_chain_add(
                                    Smpl,
                                    llama_sampler_init_temp(Temperature));
                                llama_sampler_chain_add(
                                    Smpl, llama_sampler_init_dist(
                                              LLAMA_DEFAULT_SEED));

                                llama_batch Batch =
                                    llama_batch_init(512, 0, 1);

                                const int32_t Pos =
                                    FillBatchRecursive(Batch, Tokens, 0, 0);
                                return llama_decode(Ctx->Ctx, Batch) != 0
                                           ? (llama_batch_free(Batch),
                                              llama_sampler_free(Smpl),
                                              static_cast<char *>(nullptr))
                                           : [&]() -> char * {
                                               std::string Output;
                                               GenerateRecursive(
                                                   Ctx, Vocab, Smpl, Batch,
                                                   Output, MaxTokens, 0, Pos);

                                               llama_batch_free(Batch);
                                               llama_sampler_free(Smpl);

                                               char *Result =
                                                   static_cast<char *>(malloc(
                                                       Output.size() + 1));
                                               return Result
                                                          ? (memcpy(Result,
                                                                    Output
                                                                        .c_str(),
                                                                    Output.size() +
                                                                        1),
                                                             Result)
                                                          : nullptr;
                                             }();
                              }();
               }();
}

int InferStream(llama_facade_context *Ctx, const char *PromptUtf8, int MaxTokens,
                float Temperature, TokenCallback OnToken, void *UserData) {
  return (!Ctx || !PromptUtf8 || Ctx->IsEmbedding || !OnToken)
             ? 0
             : [&]() -> int {
                 const llama_vocab *Vocab =
                     llama_model_get_vocab(Ctx->Model);
                 const int32_t PromptLen = static_cast<int32_t>(strlen(PromptUtf8));
                 const int32_t EstTokens = PromptLen + 2;

                 std::vector<llama_token> Tokens(EstTokens);
                 int32_t N = llama_tokenize(
                     Vocab, PromptUtf8, PromptLen, Tokens.data(),
                     static_cast<int32_t>(Tokens.size()), true, false);
                 N < 0 ? (Tokens.resize(static_cast<size_t>(-N)),
                          N = llama_tokenize(Vocab, PromptUtf8, PromptLen,
                                             Tokens.data(),
                                             static_cast<int32_t>(Tokens.size()),
                                             true, false),
                          void())
                       : void();
                 return N <= 0
                            ? 0
                            : [&]() -> int {
                                Tokens.resize(static_cast<size_t>(N));

                                llama_sampler_chain_params SParams =
                                    llama_sampler_chain_default_params();
                                llama_sampler *Smpl =
                                    llama_sampler_chain_init(SParams);
                                llama_sampler_chain_add(
                                    Smpl, llama_sampler_init_top_k(40));
                                llama_sampler_chain_add(
                                    Smpl, llama_sampler_init_top_p(0.9f, 1));
                                llama_sampler_chain_add(
                                    Smpl,
                                    llama_sampler_init_temp(Temperature));
                                llama_sampler_chain_add(
                                    Smpl, llama_sampler_init_dist(
                                              LLAMA_DEFAULT_SEED));

                                llama_batch Batch =
                                    llama_batch_init(512, 0, 1);

                                const int32_t Pos =
                                    FillBatchRecursive(Batch, Tokens, 0, 0);
                                return llama_decode(Ctx->Ctx, Batch) != 0
                                           ? (llama_batch_free(Batch),
                                              llama_sampler_free(Smpl), 0)
                                           : [&]() -> int {
                                               const int Generated =
                                                   StreamRecursive(
                                                       Ctx, Vocab, Smpl, Batch,
                                                       OnToken, UserData,
                                                       MaxTokens, 0, Pos);

                                               llama_batch_free(Batch);
                                               llama_sampler_free(Smpl);
                                               return Generated;
                                             }();
                              }();
               }();
}

char *InferWithGrammar(llama_facade_context *Ctx, const char *PromptUtf8,
                       int MaxTokens, float Temperature,
                       const char *GrammarUtf8) {
  return (!Ctx || !PromptUtf8 || Ctx->IsEmbedding)
             ? nullptr
             /**
              * If no grammar provided, fall back to regular Infer
              * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
              */
             : (!GrammarUtf8 || !*GrammarUtf8)
                   ? Infer(Ctx, PromptUtf8, MaxTokens, Temperature)
                   : [&]() -> char * {
                       const llama_vocab *Vocab =
                           llama_model_get_vocab(Ctx->Model);
                       const int32_t PromptLen = static_cast<int32_t>(strlen(PromptUtf8));
                       const int32_t EstTokens = PromptLen + 2;

                       std::vector<llama_token> Tokens(EstTokens);
                       int32_t N = llama_tokenize(
                           Vocab, PromptUtf8, PromptLen, Tokens.data(),
                           static_cast<int32_t>(Tokens.size()), true, false);
                       N < 0 ? (Tokens.resize(static_cast<size_t>(-N)),
                                N = llama_tokenize(Vocab, PromptUtf8, PromptLen,
                                                   Tokens.data(),
                                                   static_cast<int32_t>(Tokens.size()),
                                                   true, false),
                                void())
                             : void();
                       return N <= 0
                                  ? nullptr
                                  : [&]() -> char * {
                                      Tokens.resize(static_cast<size_t>(N));

                                      llama_sampler_chain_params SParams =
                                          llama_sampler_chain_default_params();
                                      llama_sampler *Smpl =
                                          llama_sampler_chain_init(SParams);
                                      llama_sampler_chain_add(
                                          Smpl,
                                          llama_sampler_init_top_k(40));
                                      llama_sampler_chain_add(
                                          Smpl,
                                          llama_sampler_init_top_p(0.9f, 1));
                                      llama_sampler_chain_add(
                                          Smpl,
                                          llama_sampler_init_temp(Temperature));
                                      /**
                                       * G11: Add GBNF grammar sampler for constrained output
                                       * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
                                       */
                                      llama_sampler_chain_add(
                                          Smpl,
                                          llama_sampler_init_grammar(
                                              Vocab, GrammarUtf8, "root"));
                                      llama_sampler_chain_add(
                                          Smpl, llama_sampler_init_dist(
                                                    LLAMA_DEFAULT_SEED));

                                      llama_batch Batch =
                                          llama_batch_init(512, 0, 1);

                                      const int32_t Pos =
                                          FillBatchRecursive(
                                              Batch, Tokens, 0, 0);
                                      return llama_decode(Ctx->Ctx, Batch) != 0
                                                 ? (llama_batch_free(Batch),
                                                    llama_sampler_free(Smpl),
                                                    static_cast<char *>(
                                                        nullptr))
                                                 : [&]() -> char * {
                                                     std::string Output;
                                                     GenerateRecursive(
                                                         Ctx, Vocab, Smpl,
                                                         Batch, Output,
                                                         MaxTokens, 0, Pos);

                                                     llama_batch_free(Batch);
                                                     llama_sampler_free(Smpl);

                                                     char *Result =
                                                         static_cast<char *>(
                                                             malloc(
                                                                 Output.size() +
                                                                 1));
                                                     return Result
                                                                ? (memcpy(
                                                                       Result,
                                                                       Output
                                                                           .c_str(),
                                                                       Output.size() +
                                                                           1),
                                                                   Result)
                                                                : nullptr;
                                                   }();
                                    }();
                     }();
}

bool Embed(llama_facade_context *Ctx, const char *TextUtf8, float *Out,
           int Dims) {
  return (!Ctx || !TextUtf8 || !Out || Dims < 1 || !Ctx->IsEmbedding)
             ? false
             : [&]() -> bool {
                 try {
                 const llama_vocab *Vocab =
                     llama_model_get_vocab(Ctx->Model);
                 const int32_t TextLen = static_cast<int32_t>(strlen(TextUtf8));
                 const int32_t EstTokens = TextLen + 2;
                 std::vector<llama_token> Tokens(EstTokens);
                 int32_t N = llama_tokenize(
                     Vocab, TextUtf8, TextLen, Tokens.data(),
                     static_cast<int32_t>(Tokens.size()), true, false);
                 N < 0 ? (Tokens.resize(static_cast<size_t>(-N)),
                          N = llama_tokenize(Vocab, TextUtf8, TextLen,
                                             Tokens.data(),
                                             static_cast<int32_t>(Tokens.size()),
                                             true, false),
                          void())
                       : void();
                 return N <= 0
                            ? false
                            : [&]() -> bool {
                                Tokens.resize(static_cast<size_t>(N));

                                llama_batch Batch =
                                    llama_batch_init(512, 0, 1);
                                FillBatchRecursive(Batch, Tokens, 0, 0);

                                llama_memory_clear(
                                    llama_get_memory(Ctx->Ctx), true);
                                return llama_decode(Ctx->Ctx, Batch) != 0
                                           ? (llama_batch_free(Batch), false)
                                           : [&]() -> bool {
                                               const int32_t NEmb =
                                                   llama_model_n_embd_out(
                                                       Ctx->Model);
                                               const float *Emb =
                                                   llama_get_embeddings_seq(
                                                       Ctx->Ctx, 0);
                                               return !Emb
                                                          ? (llama_batch_free(
                                                                 Batch),
                                                             false)
                                                          : [&]() -> bool {
                                                              const int Copy =
                                                                  Dims < NEmb
                                                                      ? Dims
                                                                      : NEmb;
                                                              CopyEmbeddingsRecursive(
                                                                  Out, Emb,
                                                                  Copy, 0);
                                                              ZeroFillRecursive(
                                                                  Out, Dims,
                                                                  Copy);
                                                              NormalizeVector(
                                                                  Out, Dims);

                                                              llama_batch_free(
                                                                  Batch);
                                                              return true;
                                                            }();
                                             }();
                              }();
                 } catch (const std::exception &) {
                   return false;
                 } catch (...) {
                   return false;
                 }
               }();
}

} // namespace LlamaFacade

#else

namespace LlamaFacade {

llama_facade_context *LoadInferenceModel(const char *) { return nullptr; }
llama_facade_context *LoadEmbeddingModel(const char *) { return nullptr; }
void FreeContext(llama_facade_context *) {}
char *Infer(llama_facade_context *, const char *, int, float) { return nullptr; }
char *InferWithGrammar(llama_facade_context *, const char *, int, float, const char *) { return nullptr; }
int InferStream(llama_facade_context *, const char *, int, float, TokenCallback, void *) { return 0; }
bool Embed(llama_facade_context *, const char *, float *, int) { return false; }

} // namespace LlamaFacade

#endif
