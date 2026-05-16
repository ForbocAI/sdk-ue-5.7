#include "CLI/CliHandlers.h"
#include "CLI/CliOperations.h"
#include "RuntimeStore.h"

namespace CLIOps {
namespace Handlers {

HandlerResult HandleConfig(rtk::EnhancedStore<FStoreState> &Store,
                          const FString &CommandKey,
                          const TArray<FString> &Args) {
  (void)Store;
  using func::just;
  using func::nothing;

  return CommandKey == TEXT("config_set")
             ? (Args.Num() < 2
                    ? just(Result::Failure("Usage: config_set <key> <value>"))
                    : (Ops::ConfigSet(Args[0], Args[1]),
                       just(Result::Success("Config updated"))))
         : CommandKey == TEXT("config_get")
             ? (Args.Num() < 1
                    ? just(Result::Failure("Usage: config_get <key>"))
                    : [&]() -> HandlerResult {
                        FString Value = Ops::ConfigGet(Args[0]);
                        UE_LOG(LogTemp, Display, TEXT("%s = %s"), *Args[0],
                               *Value);
                        return just(Result::Success("Config retrieved"));
                      }())
         : CommandKey == TEXT("config_list")
             ? [&]() -> HandlerResult {
                 static const TArray<FString> ConfigKeys = {
                     TEXT("version"), TEXT("apiUrl"), TEXT("apiKey"),
                     TEXT("modelPath"), TEXT("databasePath"),
                     TEXT("vectorDimension"), TEXT("maxRecallResults")};
                 struct LogKeys {
                   static void apply(const TArray<FString> &Keys, int32 Idx) {
                     Idx >= Keys.Num()
                         ? void()
                         : ([&]() {
                              const FString Val = Ops::ConfigGet(Keys[Idx]);
                              const bool bMask =
                                  Keys[Idx].Contains(TEXT("key"),
                                                     ESearchCase::IgnoreCase) ||
                                  Keys[Idx].Contains(
                                      TEXT("Key"),
                                      ESearchCase::CaseSensitive);
                              UE_LOG(LogTemp, Display, TEXT("  %s = %s"),
                                     *Keys[Idx],
                                     bMask && !Val.IsEmpty() ? TEXT("********")
                                                             : *Val);
                            }(),
                            apply(Keys, Idx + 1), void());
                   }
                 };
                 LogKeys::apply(ConfigKeys, 0);
                 return just(Result::Success("Config listed"));
               }()
             : nothing<Result>();
}

} // namespace Handlers
} // namespace CLIOps
