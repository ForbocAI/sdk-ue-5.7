#include "CLI/CliHandlers.h"
#include "CLI/CliOperations.h"
#include "RuntimeStore.h"

namespace CLIOps {
namespace Handlers {

HandlerResult HandleGhost(rtk::EnhancedStore<FStoreState> &Store,
                         const FString &CommandKey,
                         const TArray<FString> &Args) {
  using func::just;
  using func::nothing;

  return CommandKey == TEXT("ghost_run")
             ? [&]() -> HandlerResult {
                 FString Suite =
                     Args.Num() > 0 ? Args[0] : TEXT("default");
                 int32 Duration =
                     Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 300;
                 FGhostRunResponse Resp =
                     Ops::GhostRun(Store, Suite, Duration);
                 UE_LOG(LogTemp, Display,
                        TEXT("Ghost session started: %s"), *Resp.SessionId);
                 return just(Result::Success("Ghost run started"));
               }()
         : CommandKey == TEXT("ghost_status")
             ? (Args.Num() < 1
                    ? just(Result::Failure(
                          "Usage: ghost_status <sessionId>"))
                    : [&]() -> HandlerResult {
                        FGhostStatusResponse Resp =
                            Ops::GhostStatus(Store, Args[0]);
                        UE_LOG(LogTemp, Display, TEXT("Ghost status: %s"),
                               *Resp.GhostStatus);
                        return just(
                            Result::Success("Ghost status retrieved"));
                      }())
         : CommandKey == TEXT("ghost_results")
             ? (Args.Num() < 1
                    ? just(Result::Failure(
                          "Usage: ghost_results <sessionId>"))
                    : [&]() -> HandlerResult {
                        Ops::GhostResults(Store, Args[0]);
                        UE_LOG(LogTemp, Display,
                               TEXT("Ghost results retrieved"));
                        return just(Result::Success(
                            "Ghost results retrieved"));
                      }())
         : CommandKey == TEXT("ghost_stop")
             ? (Args.Num() < 1
                    ? just(Result::Failure(
                          "Usage: ghost_stop <sessionId>"))
                    : [&]() -> HandlerResult {
                        FGhostStopResponse Resp =
                            Ops::GhostStop(Store, Args[0]);
                        UE_LOG(LogTemp, Display,
                               TEXT("Ghost stopped: %s"), *Resp.StopStatus);
                        return just(Result::Success("Ghost stopped"));
                      }())
         : CommandKey == TEXT("ghost_history")
             ? [&]() -> HandlerResult {
                 int32 Limit =
                     Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 10;
                 TArray<FGhostHistoryEntry> History =
                     Ops::GhostHistory(Store, Limit);
                 UE_LOG(LogTemp, Display,
                        TEXT("Ghost history: %d entries"), History.Num());
                 return just(
                     Result::Success("Ghost history retrieved"));
               }()
             : nothing<Result>();
}

} // namespace Handlers
} // namespace CLIOps
