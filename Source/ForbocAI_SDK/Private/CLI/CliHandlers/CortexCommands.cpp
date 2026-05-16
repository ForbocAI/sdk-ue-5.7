#include "CLI/CliHandlers.h"
#include "CLI/CliOperations.h"
#include "Cortex/CortexTypes.h"
#include "RuntimeStore.h"

namespace CLIOps {
namespace Handlers {

namespace {

FString DescribeCortexStatus(const FCortexStatus &Status) {
  return Status.bReady             ? TEXT("ready")
         : !Status.Error.IsEmpty() ? Status.Error
                                   : TEXT("initializing");
}

} // namespace

HandlerResult HandleCortex(rtk::EnhancedStore<FStoreState> &Store,
                          const FString &CommandKey,
                          const TArray<FString> &Args) {
  using func::just;
  using func::nothing;

  return CommandKey == TEXT("cortex_init")
             ? (Args.Num() < 1
                    ? just(Result::Failure("Usage: cortex_init <model>"))
                    : [&]() -> HandlerResult {
                        FCortexStatus Status =
                            Ops::InitCortex(Store, Args[0]);
                        UE_LOG(LogTemp, Display,
                               TEXT("Cortex initialized: %s"),
                               *DescribeCortexStatus(Status));
                        return just(
                            Result::Success("Cortex initialized"));
                      }())
         : CommandKey == TEXT("cortex_init_remote")
             ? (Args.Num() < 1
                    ? just(Result::Failure(
                          "Usage: cortex_init_remote <model> [authKey]"))
                    : [&]() -> HandlerResult {
                        FString AuthKey =
                            Args.Num() > 1 ? Args[1] : TEXT("");
                        FCortexStatus Status = Ops::InitRemoteCortex(
                            Store, Args[0], AuthKey);
                        UE_LOG(LogTemp, Display,
                               TEXT("Remote cortex initialized: %s"),
                               *DescribeCortexStatus(Status));
                        return just(Result::Success(
                            "Remote cortex initialized"));
                      }())
         : CommandKey == TEXT("cortex_models")
             ? [&]() -> HandlerResult {
                 TArray<FCortexModelInfo> Models =
                     Ops::ListCortexModels(Store);
                 UE_LOG(LogTemp, Display, TEXT("Found %d models"),
                        Models.Num());
                 return just(Result::Success("Models listed"));
               }()
         : CommandKey == TEXT("cortex_complete")
             ? (Args.Num() < 2
                    ? just(Result::Failure(
                          "Usage: cortex_complete <cortexId> <prompt>"))
                    : [&]() -> HandlerResult {
                        FCortexResponse Resp =
                            Ops::CompleteRemoteCortex(Store, Args[0],
                                                     Args[1]);
                        UE_LOG(LogTemp, Display, TEXT("Completion: %s"),
                               *Resp.Text);
                        return just(
                            Result::Success("Completion done"));
                      }())
             : nothing<Result>();
}

} // namespace Handlers
} // namespace CLIOps
