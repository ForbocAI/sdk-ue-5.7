// User Story: As a developer, I need this module to function.
#include "CLI/CliHandlers.h"
#include "CLI/CliOperations.h"
#include "RuntimeStore.h"

namespace CLIOps {
namespace Handlers {

HandlerResult HandleNpc(rtk::EnhancedStore<FStoreState> &Store,
                       const FString &CommandKey,
                       const TArray<FString> &Args) {
  using func::just;
  using func::nothing;

  return CommandKey == TEXT("npc_create")
             ? [&]() -> HandlerResult {
                 // Vessel contract: pass empty when no persona is supplied so
                 // the API renders all slots as <unset>.
                 FString Persona =
                     Args.Num() > 0 ? Args[0] : TEXT("");
                 FNPCInternalState Npc = Ops::CreateNpc(Store, Persona);
                 UE_LOG(LogTemp, Display, TEXT("Created NPC: %s"),
                        *Npc.Id);
                 return just(Result::Success("NPC created"));
               }()
         : CommandKey == TEXT("npc_list")
             ? [&]() -> HandlerResult {
                 TArray<FNPCInternalState> Npcs = Ops::ListNpcs(Store);
                 UE_LOG(LogTemp, Display, TEXT("Found %d NPCs"),
                        Npcs.Num());
                 struct LogNpcs {
                   static void apply(const TArray<FNPCInternalState> &N,
                                     int32 Idx) {
                     Idx >= N.Num()
                         ? void()
                         : ([&]() {
                              UE_LOG(LogTemp, Display, TEXT("- %s (%s)"),
                                     *N[Idx].Id, *N[Idx].Persona);
                            }(),
                            apply(N, Idx + 1), void());
                   }
                 };
                 LogNpcs::apply(Npcs, 0);
                 return just(Result::Success("NPCs listed"));
               }()
         : CommandKey == TEXT("npc_process")
             ? (Args.Num() < 2
                    ? just(Result::Failure(
                          "Usage: npc_process <npcId> <text>"))
                    : [&]() -> HandlerResult {
                        FAgentResponse Resp =
                            Ops::ProcessNpc(Store, Args[0], Args[1]);
                        UE_LOG(LogTemp, Display, TEXT("Verdict: %s"),
                               *Resp.Dialogue);
                        return just(
                            Result::Success("Protocol complete"));
                      }())
         : CommandKey == TEXT("npc_active")
             ? [&]() -> HandlerResult {
                 func::Maybe<FNPCInternalState> Active =
                     Ops::GetActiveNpc(Store);
                 Active.hasValue
                     ? [&]() {
                         UE_LOG(LogTemp, Display,
                                TEXT("Active NPC: %s (%s)"),
                                *Active.value.Id,
                                *Active.value.Persona);
                       }()
                     : [&]() {
                         UE_LOG(LogTemp, Display,
                                TEXT("No active NPC"));
                       }();
                 return just(
                     Result::Success("Active NPC queried"));
               }()
         : CommandKey == TEXT("npc_state")
             ? [&]() -> HandlerResult {
                 func::Maybe<FNPCInternalState> Active =
                     Ops::GetActiveNpc(Store);
                 return !Active.hasValue
                     ? [&]() -> HandlerResult {
                         UE_LOG(LogTemp, Display,
                                TEXT("No active NPC"));
                         return just(
                             Result::Success("No active NPC"));
                       }()
                     : [&]() -> HandlerResult {
                         UE_LOG(LogTemp, Display,
                                TEXT("NPC ID:      %s"),
                                *Active.value.Id);
                         UE_LOG(LogTemp, Display,
                                TEXT("Persona:     %s"),
                                *Active.value.Persona);
                         return just(
                             Result::Success("NPC state printed"));
                       }();
               }()
         : CommandKey == TEXT("npc_update")
             ? (Args.Num() < 2
                    ? just(Result::Failure(
                          "Usage: npc_update <npcId> "
                          "[-Mood=<value>] [-Inventory=<item,item>]"))
                    : [&]() -> HandlerResult {
                        FAgentState Delta;
                        Delta.JsonData =
                            Args.Num() > 2
                                ? FString::Printf(
                                      TEXT("{\"Mood\":\"%s\","
                                           "\"Inventory\":\"%s\"}"),
                                      *Args[1], *Args[2])
                                : FString::Printf(
                                      TEXT("{\"Mood\":\"%s\"}"),
                                      *Args[1]);
                        Ops::UpdateNpc(Store, Args[0], Delta);
                        UE_LOG(LogTemp, Display,
                               TEXT("NPC %s updated"), *Args[0]);
                        return just(
                            Result::Success("NPC updated"));
                      }())
         : CommandKey == TEXT("npc_import")
             ? (Args.Num() < 1
                    ? just(Result::Failure(
                          "Usage: npc_import <txId>"))
                    : [&]() -> HandlerResult {
                        FImportedNpc Npc =
                            Ops::ImportNpcFromSoul(Store, Args[0]);
                        UE_LOG(LogTemp, Display,
                               TEXT("NPC imported from soul: %s"),
                               *Npc.NpcId);
                        return just(Result::Success(
                            "NPC imported from soul"));
                      }())
         : CommandKey == TEXT("npc_chat")
             ? (Args.Num() < 2
                    ? just(Result::Failure(
                          "Usage: npc_chat <npcId> <message>"))
                    : [&]() -> HandlerResult {
                        UE_LOG(LogTemp, Display, TEXT("> You: %s"),
                               *Args[1]);
                        FAgentResponse Resp =
                            Ops::ProcessNpc(Store, Args[0], Args[1]);
                        UE_LOG(LogTemp, Display, TEXT("> NPC: %s"),
                               *Resp.Dialogue);
                        !Resp.Action.Type.IsEmpty()
                            ? [&]() {
                                UE_LOG(LogTemp, Display,
                                       TEXT("> Action: %s"),
                                       *Resp.Action.Type);
                              }()
                            : (void)0;
                        return just(
                            Result::Success("Chat turn complete"));
                      }())
             : nothing<Result>();
}

} // namespace Handlers
} // namespace CLIOps
