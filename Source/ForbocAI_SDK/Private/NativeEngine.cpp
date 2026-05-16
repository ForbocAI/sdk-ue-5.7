#include "NativeEngine.h"
#include "LlamaFacade.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include <memory>

#if WITH_FORBOC_SQLITE_VEC
extern "C" {
#include "sqlite3.h"
#include "sqlite-vec.h"
}
#endif

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

namespace {

constexpr int32 EmbeddingDimensions = 384;

struct FBinaryDownloadState {
  FString Destination;
  std::function<void(FString)> Resolve;
  std::function<void(std::string)> Reject;
};

const char *Utf8Bytes(const UTF8CHAR *Chars) {
  return reinterpret_cast<const char *>(Chars);
}

uint64 StableHashRecursive(const FString &Value, int32 Index, uint64 Hash) {
  return Index == Value.Len()
             ? Hash
             : StableHashRecursive(
                   Value, Index + 1,
                   (Hash ^ static_cast<uint64>(FChar::ToLower(Value[Index]))) *
                       1099511628211ull);
}

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

FString BuildJsonVectorRecursive(const TArray<float> &Vector, int32 Index,
                                 FString JsonVec) {
  return Index == Vector.Num()
             ? JsonVec + TEXT("]")
             : BuildJsonVectorRecursive(
                   Vector, Index + 1,
                   JsonVec + FString::SanitizeFloat(Vector[Index]) +
                       (Index + 1 < Vector.Num() ? TEXT(",") : TEXT("")));
}

FString BuildJsonVector(const TArray<float> &Vector) {
  return BuildJsonVectorRecursive(Vector, 0, TEXT("["));
}

#if WITH_FORBOC_SQLITE_VEC
FMemoryItem ReadMemoryItem(sqlite3_stmt *Stmt) {
  FMemoryItem Item;
  const unsigned char *IdText = sqlite3_column_text(Stmt, 0);
  const unsigned char *Txt = sqlite3_column_text(Stmt, 1);
  const unsigned char *TypeTxt = sqlite3_column_text(Stmt, 2);
  Item.Id = IdText ? UTF8_TO_TCHAR(reinterpret_cast<const char *>(IdText))
                   : TEXT("");
  Item.Text = Txt ? UTF8_TO_TCHAR(reinterpret_cast<const char *>(Txt))
                  : TEXT("");
  Item.Type = TypeTxt ? UTF8_TO_TCHAR(reinterpret_cast<const char *>(TypeTxt))
                      : TEXT("observation");
  Item.Importance = static_cast<float>(sqlite3_column_double(Stmt, 3));
  Item.Timestamp = static_cast<int64>(sqlite3_column_int64(Stmt, 4));
  Item.Similarity =
      static_cast<float>(1.0 - sqlite3_column_double(Stmt, 5));
  return Item;
}

void CollectSearchRowsRecursive(sqlite3_stmt *Stmt, TArray<FMemoryItem> &Results) {
  const int StepResult = sqlite3_step(Stmt);
  StepResult == SQLITE_ROW
      ? (Results.Add(ReadMemoryItem(Stmt)),
         CollectSearchRowsRecursive(Stmt, Results))
      : void();
}
#endif

bool IsRedirectCode(const int32 Code) {
  return Code == 301 || Code == 302 || Code == 303 || Code == 307 ||
         Code == 308;
}

void ContinueBinaryDownload(const FString &RequestUrl,
                            const TSharedPtr<FBinaryDownloadState> &State);

void ResolveBinaryDownload(const TSharedPtr<FBinaryDownloadState> &State,
                           const TArray<uint8> &Payload) {
  FFileHelper::SaveArrayToFile(Payload, *State->Destination)
      ? State->Resolve(State->Destination)
      : State->Reject("Failed to save downloaded file to disk");
}

void HandleBinaryDownloadResponse(FHttpResponsePtr Response,
                                  const bool bWasSuccessful,
                                  const TSharedPtr<FBinaryDownloadState> &State) {
  (!bWasSuccessful || !Response.IsValid())
      ? State->Reject("Network failure downloading binary")
      : IsRedirectCode(Response->GetResponseCode()) &&
                !Response->GetHeader(TEXT("Location")).IsEmpty()
            ? ContinueBinaryDownload(Response->GetHeader(TEXT("Location")),
                                     State)
            : (Response->GetResponseCode() < 200 ||
               Response->GetResponseCode() >= 300)
                  ? State->Reject(std::string("HTTP error ") +
                                  std::to_string(Response->GetResponseCode()))
                  : ResolveBinaryDownload(State, Response->GetContent());
}

void ContinueBinaryDownload(const FString &RequestUrl,
                            const TSharedPtr<FBinaryDownloadState> &State) {
  TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
      FHttpModule::Get().CreateRequest();
  Request->SetURL(RequestUrl);
  Request->SetVerb(TEXT("GET"));
  Request->OnProcessRequestComplete().BindLambda(
      [State](FHttpRequestPtr RequestPtr, FHttpResponsePtr Response,
              bool bWasSuccessful) {
        HandleBinaryDownloadResponse(Response, bWasSuccessful, State);
      });
  Request->ProcessRequest();
}

/**
 * Computes a case-insensitive stable hash used for generated memory ids.
 * User Story: As local-memory persistence, I need stable hashes so derived
 * memory ids stay deterministic across repeated saves.
 */
uint64 StableHashString(const FString &Value) {
  return StableHashRecursive(Value, 0, 1469598103934665603ull);
}

/**
 * Derives a deterministic memory id from the item's stable content fields.
 * User Story: As local-memory persistence, I need deterministic ids so the
 * same memory content can be upserted without creating duplicates.
 */
FString MakeStableMemoryId(const FMemoryItem &Item) {
  const FString Seed =
      FString::Printf(TEXT("%s|%s|%lld"), *Item.Type, *Item.Text,
                      static_cast<long long>(Item.Timestamp));
  return FString::Printf(TEXT("mem_%016llx"), static_cast<unsigned long long>(
                                                  StableHashString(Seed)));
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

/**
 * Normalizes a memory item before persistence and ensures it has an id.
 * User Story: As local-memory writes, I need items normalized before storage so
 * sqlite rows always have stable ids and clean similarity defaults.
 */
FMemoryItem PrepareStoredItem(const FMemoryItem &Item) {
  FMemoryItem StoredItem = Item;
  StoredItem.Id =
      StoredItem.Id.IsEmpty() ? MakeStableMemoryId(StoredItem) : StoredItem.Id;
  StoredItem.Similarity = 0.0f;
  return StoredItem;
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

namespace File {

/**
 * Downloads a binary asset to disk, following redirect responses as needed.
 * User Story: As local-runtime setup, I need binary assets downloaded safely so
 * models and native artifacts can be prepared on demand. Redirect responses are
 * followed before the payload is persisted.
 */
func::AsyncResult<FString> DownloadBinary(const FString &Url,
                                          const FString &DestPath) {
  return func::createAsyncResult<FString>(
      [Url, DestPath](std::function<void(FString)> Resolve,
                      std::function<void(std::string)> Reject) {
        TSharedPtr<FBinaryDownloadState> State =
            MakeShared<FBinaryDownloadState>();
        State->Destination = DestPath;
        State->Resolve = Resolve;
        State->Reject = Reject;
        ContinueBinaryDownload(Url, State);
      });
}

} // namespace File

namespace Sqlite {

/**
 * Opens the sqlite-vec database handle for local memory storage.
 * User Story: As local-memory initialization, I need a sqlite-vec handle so
 * vector-backed memory rows can be persisted and queried locally. Relative
 * paths are normalized, traversal segments are rejected, and the vector table
 * is created before the handle is returned.
 */
DB Open(const FString &Path) {
  const FString NormalizedPath = Path.IsEmpty()
                                     ? TEXT(":memory:")
                                     : FPaths::ConvertRelativePathToFull(Path);

  return (NormalizedPath != TEXT(":memory:") &&
          NormalizedPath.Contains(TEXT("..")))
             ? [&]() -> DB {
                 UE_LOG(LogTemp, Error,
                        TEXT("ForbocAI: Rejected database path containing "
                             "'..': %s"),
                        *NormalizedPath);
                 return static_cast<DB>(nullptr);
               }()
             :
#if WITH_FORBOC_SQLITE_VEC
             [&]() -> DB {
               sqlite3 *Db = nullptr;
               const int Rc =
                   sqlite3_open(TCHAR_TO_UTF8(*NormalizedPath), &Db);
               return (Rc != SQLITE_OK || !Db)
                          ? (Db ? (sqlite3_close(Db), static_cast<DB>(nullptr))
                                : static_cast<DB>(nullptr))
                          : [&]() -> DB {
                              /* Register sqlite-vec extension before creating
                               * the vec0 virtual table. With SQLITE_CORE=1 the
                               * extension uses the host sqlite3 API directly. */
                              sqlite3_vec_init(Db, nullptr, nullptr);

                              const char *CreateSql =
                                  "CREATE VIRTUAL TABLE IF NOT EXISTS memories "
                                  "USING "
                                  "vec0(embedding float[384], "
                                  "+id text, +text text, +type text, "
                                  "+importance float, +timestamp integer);";
                              sqlite3_exec(Db, CreateSql, nullptr, nullptr,
                                           nullptr);
                              return reinterpret_cast<DB>(Db);
                            }();
             }()
#else
             [&]() -> DB {
               UE_LOG(LogTemp, Error,
                      TEXT("ForbocAI: Sqlite::Open requires "
                           "WITH_FORBOC_SQLITE_VEC=1. Native libs not "
                           "available."));
               return static_cast<DB>(nullptr);
             }()
#endif
      ;
}

/**
 * Closes a sqlite-vec database handle created by Open.
 * User Story: As local-memory shutdown, I need opened sqlite handles closed so
 * vector storage does not leak native resources.
 */
void Close(DB Database) {
  !Database ? void()
            :
#if WITH_FORBOC_SQLITE_VEC
            (sqlite3_close(reinterpret_cast<sqlite3 *>(Database)), void())
#else
            ((void)Database, void())
#endif
      ;
}

/**
 * Deletes all rows from the in-memory sqlite-vec backing table.
 * User Story: As local-memory reset flows, I need the vector table cleared so
 * tests and fresh sessions can start from an empty store.
 */
void Clear(DB Database) {
#if WITH_FORBOC_SQLITE_VEC
  sqlite3 *Handle = reinterpret_cast<sqlite3 *>(Database);
  Handle
      ? (sqlite3_exec(Handle, "DELETE FROM memories;", nullptr, nullptr,
                      nullptr),
         void())
      : void();
#else
  (void)Database;
#endif
}

/**
 * Clears a database path when higher layers request a full reset.
 * User Story: As local-memory reset flows, I need path-based clearing guarded
 * so higher layers can request resets without unsafe file operations. Under
 * sqlite-vec this only validates the path and leaves file deletion to higher
 * layers.
 */
void ClearPath(const FString &Path) {
  const FString NormalizedPath = Path.IsEmpty()
                                     ? TEXT(":memory:")
                                     : FPaths::ConvertRelativePathToFull(Path);

  (NormalizedPath != TEXT(":memory:") && NormalizedPath.Contains(TEXT("..")))
      ? [&]() {
          UE_LOG(LogTemp, Error,
                 TEXT("ForbocAI: Rejected database path containing '..': %s"),
                 *NormalizedPath);
        }()
      :
#if WITH_FORBOC_SQLITE_VEC
      (static_cast<void>(NormalizedPath), void())
#else
      ((void)NormalizedPath, void())
#endif
      ;
}

/**
 * Executes a vector similarity search and materializes matching memory rows.
 * User Story: As local-memory recall, I need nearest-neighbor rows turned into
 * memory items so search results can flow back into higher-level thunks.
 */
TArray<FMemoryItem> SearchRows(DB Database, const TArray<float> &Vector,
                               int32 TopK) {
  TArray<FMemoryItem> Results;
#if WITH_FORBOC_SQLITE_VEC
  sqlite3 *Handle = reinterpret_cast<sqlite3 *>(Database);
  return !Handle
             ? Results
             : [&]() -> TArray<FMemoryItem> {
                 const int32 Limit = TopK > 0 ? TopK : 10;
                 const char *Sql =
                     "SELECT id, text, type, importance, timestamp, distance "
                     "FROM memories "
                     "WHERE embedding MATCH ? "
                     "ORDER BY distance "
                     "LIMIT ?;";

                 sqlite3_stmt *Stmt = nullptr;
                 return sqlite3_prepare_v2(Handle, Sql, -1, &Stmt, nullptr) !=
                                SQLITE_OK
                            ? Results
                            : [&]() -> TArray<FMemoryItem> {
                                const FString JsonVec =
                                    BuildJsonVector(Vector);

                                sqlite3_bind_text(Stmt, 1,
                                                  TCHAR_TO_UTF8(*JsonVec), -1,
                                                  SQLITE_TRANSIENT);
                                sqlite3_bind_int(Stmt, 2, Limit);
                                CollectSearchRowsRecursive(Stmt, Results);

                                sqlite3_finalize(Stmt);

                                return Results;
                              }();
               }();
#else
  (void)Database;
  (void)Vector;
  (void)TopK;
  return Results;
#endif
}

/**
 * Upserts a memory item using its embedded vector payload.
 * User Story: As local-memory writes, I need an upsert path that reuses the
 * item's embedding so stored rows stay aligned with computed vectors when no
 * explicit vector override is provided.
 */
bool Upsert(DB Database, const FMemoryItem &Item) {
#if WITH_FORBOC_SQLITE_VEC
  return Upsert(Database, Item, Item.Embedding);
#else
  (void)Database;
  (void)Item;
  return false;
#endif
}

/**
 * Searches the vector store for the nearest memory rows.
 * User Story: As local-memory recall, I need a public search helper so higher
 * layers can run similarity lookup without touching sqlite directly.
 */
TArray<FMemoryItem> Search(DB Database, const TArray<float> &Vector,
                           int32 TopK) {
  return SearchRows(Database, Vector, TopK);
}

/**
 * Upserts a memory item with an explicitly supplied embedding vector.
 * User Story: As local-memory writes, I need an explicit-vector upsert so
 * callers can persist precomputed embeddings without recomputing them.
 */
bool Upsert(DB Database, const FMemoryItem &Item, const TArray<float> &Vector) {
#if WITH_FORBOC_SQLITE_VEC
  sqlite3 *Handle = reinterpret_cast<sqlite3 *>(Database);
  return !Handle
             ? false
             : Vector.Num() == 0
                   ? false
                   : [&]() -> bool {
                       const char *Sql =
                           "INSERT OR REPLACE INTO memories "
                           "(id, text, type, importance, timestamp, embedding) "
                           "VALUES (?, ?, ?, ?, ?, ?);";

                       sqlite3_stmt *Stmt = nullptr;
                       return sqlite3_prepare_v2(Handle, Sql, -1, &Stmt,
                                                nullptr) != SQLITE_OK
                                  ? false
                                  : [&]() -> bool {
                                      const FMemoryItem StoredItem =
                                          PrepareStoredItem(Item);
                                      const FString JsonVec =
                                          BuildJsonVector(Vector);

                                      sqlite3_bind_text(
                                          Stmt, 1,
                                          TCHAR_TO_UTF8(*StoredItem.Id), -1,
                                          SQLITE_TRANSIENT);
                                      sqlite3_bind_text(
                                          Stmt, 2,
                                          TCHAR_TO_UTF8(*StoredItem.Text), -1,
                                          SQLITE_TRANSIENT);
                                      sqlite3_bind_text(
                                          Stmt, 3,
                                          TCHAR_TO_UTF8(*StoredItem.Type), -1,
                                          SQLITE_TRANSIENT);
                                      sqlite3_bind_double(
                                          Stmt, 4,
                                          static_cast<double>(
                                              StoredItem.Importance));
                                      sqlite3_bind_int64(
                                          Stmt, 5,
                                          static_cast<sqlite3_int64>(
                                              StoredItem.Timestamp));
                                      sqlite3_bind_text(
                                          Stmt, 6,
                                          TCHAR_TO_UTF8(*JsonVec), -1,
                                          SQLITE_TRANSIENT);

                                      const bool bOk =
                                          (sqlite3_step(Stmt) == SQLITE_DONE);
                                      sqlite3_finalize(Stmt);
                                      return bOk;
                                    }();
                     }();
#else
  (void)Database;
  (void)Item;
  (void)Vector;
  return false;
#endif
}

} // namespace Sqlite
} // namespace Native
