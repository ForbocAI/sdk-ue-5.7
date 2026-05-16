#include "CLI/CliHandlers.h"
#include "CLI/CliOperations.h"
#include "RuntimeConfig.h"
#include "RuntimeStore.h"

namespace CLIOps {
namespace Handlers {

HandlerResult HandleSystem(rtk::EnhancedStore<FStoreState> &Store,
                          const FString &CommandKey,
                          const TArray<FString> &Args) {
  (void)Args;
  using func::just;
  using func::nothing;

  return CommandKey == TEXT("version")
             ? [&]() -> HandlerResult {
                 UE_LOG(LogTemp, Display, TEXT("ForbocAI SDK v%s (UE5)"),
                        *SDKConfig::GetSdkVersion());
                 return just(Result::Success("Version printed"));
               }()
         : CommandKey == TEXT("status")
             ? [&]() -> HandlerResult {
                 FApiStatusResponse Status = Ops::CheckApiStatus(Store);
                 UE_LOG(LogTemp, Display, TEXT("API: %s"), *Status.Status);
                 return just(Result::Success("Status checked"));
               }()
         : (CommandKey == TEXT("doctor") ||
            CommandKey == TEXT("system_status"))
             ? [&]() -> HandlerResult {
                 UE_LOG(LogTemp, Display, TEXT("ForbocAI SDK v%s (UE5)"),
                        *SDKConfig::GetSdkVersion());
                 UE_LOG(LogTemp, Display, TEXT("API URL: %s"),
                        *SDKConfig::GetApiUrl());
                 UE_LOG(LogTemp, Display, TEXT("API Key: %s"),
                        SDKConfig::GetApiKey().IsEmpty()
                            ? TEXT("(not set)")
                            : TEXT("********"));
                 FApiStatusResponse Status = Ops::CheckApiStatus(Store);
                 UE_LOG(LogTemp, Display, TEXT("API Status: %s (v%s)"),
                        *Status.Status, *Status.Version);
                 return just(Result::Success("Doctor check completed"));
               }()
             : nothing<Result>();
}

} // namespace Handlers
} // namespace CLIOps
