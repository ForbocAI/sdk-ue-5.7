#include "CLI/CliHandlers.h"
#include "CLI/CliOperations.h"
#include "Dom/JsonObject.h"
#include "RuntimeStore.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace CLIOps {
namespace Handlers {

HandlerResult HandleMemory(rtk::EnhancedStore<FStoreState> &Store,
                          const FString &CommandKey,
                          const TArray<FString> &Args) {
  using func::just;
  using func::nothing;

  return CommandKey == TEXT("memory_list")
             ? (Args.Num() < 1
                    ? just(Result::Failure(
                          "Usage: memory_list <npcId>"))
                    : [&]() -> HandlerResult {
                        TArray<FMemoryItem> Items =
                            Ops::MemoryList(Store, Args[0]);
                        UE_LOG(LogTemp, Display,
                               TEXT("Found %d memories"), Items.Num());
                        return just(
                            Result::Success("Memories listed"));
                      }())
         : CommandKey == TEXT("memory_recall")
             ? (Args.Num() < 2
                    ? just(Result::Failure(
                          "Usage: memory_recall <npcId> <query>"))
                    : [&]() -> HandlerResult {
                        TArray<FMemoryItem> Items =
                            Ops::MemoryRecall(Store, Args[0], Args[1]);
                        UE_LOG(LogTemp, Display,
                               TEXT("Recalled %d memories"),
                               Items.Num());
                        return just(
                            Result::Success("Memories recalled"));
                      }())
         : CommandKey == TEXT("memory_store")
             ? (Args.Num() < 2
                    ? just(Result::Failure(
                          "Usage: memory_store <npcId> <observation>"))
                    : (Ops::MemoryStore(Store, Args[0], Args[1]),
                       just(Result::Success("Memory stored"))))
         : CommandKey == TEXT("memory_clear")
             ? (Args.Num() < 1
                    ? just(Result::Failure(
                          "Usage: memory_clear <npcId>"))
                    : (Ops::MemoryClear(Store, Args[0]),
                       just(Result::Success("Memory cleared"))))
         : CommandKey == TEXT("memory_export")
             ? (Args.Num() < 1
                    ? just(Result::Failure(
                          "Usage: memory_export <npcId>"))
                    : [&]() -> HandlerResult {
                        TArray<FMemoryItem> Items =
                            Ops::MemoryList(Store, Args[0]);
                        TSharedRef<FJsonObject> Root =
                            MakeShared<FJsonObject>();
                        TArray<TSharedPtr<FJsonValue>> JsonItems;
                        struct BuildItems {
                          static void apply(
                              const TArray<FMemoryItem> &Src,
                              TArray<TSharedPtr<FJsonValue>> &Out,
                              int32 Idx) {
                            Idx >= Src.Num()
                                ? void()
                                : ([&]() {
                                     TSharedPtr<FJsonObject> Obj =
                                         MakeShared<FJsonObject>();
                                     Obj->SetStringField(TEXT("text"),
                                                         Src[Idx].Text);
                                     Obj->SetStringField(TEXT("type"),
                                                         Src[Idx].Type);
                                     Obj->SetNumberField(
                                         TEXT("importance"),
                                         Src[Idx].Importance);
                                     Out.Add(
                                         MakeShared<FJsonValueObject>(
                                             Obj));
                                   }(),
                                   apply(Src, Out, Idx + 1), void());
                          }
                        };
                        BuildItems::apply(Items, JsonItems, 0);
                        Root->SetArrayField(TEXT("memories"),
                                            JsonItems);
                        FString JsonString;
                        TSharedRef<TJsonWriter<>> Writer =
                            TJsonWriterFactory<>::Create(&JsonString);
                        FJsonSerializer::Serialize(Root, Writer);
                        UE_LOG(LogTemp, Display, TEXT("%s"),
                               *JsonString);
                        return just(
                            Result::Success("Memories exported"));
                      }())
             : nothing<Result>();
}

} // namespace Handlers
} // namespace CLIOps
