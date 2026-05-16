#pragma once

// clang-format off
#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Types.h"
#include "NPCTypes.generated.h"
// clang-format on

USTRUCT(BlueprintType)
struct FNPCHistoryEntry {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FString Role;

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FString Content;
};

USTRUCT(BlueprintType)
struct FNPCStateLogEntry {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  int64 Timestamp;

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FAgentState Delta;

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FAgentState State;

  FNPCStateLogEntry() : Timestamp(0) {}
};

USTRUCT(BlueprintType)
struct FNPCInternalState {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FString Id;

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FString Persona;

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FAgentState State;

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  TArray<FNPCHistoryEntry> History;

  FAgentAction LastAction;
  bool bHasLastAction;

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  bool bIsBlocked;

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FString BlockReason;

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  TArray<FNPCStateLogEntry> StateLog;

  FNPCInternalState() : bHasLastAction(false), bIsBlocked(false) {}
};

namespace NPCSlice {

using namespace rtk;
using namespace func;
using ::FNPCHistoryEntry;
using ::FNPCInternalState;
using ::FNPCStateLogEntry;

inline FString NPCIdSelector(const FNPCInternalState &NPC) { return NPC.Id; }

inline EntityAdapterOps<FNPCInternalState> GetNPCAdapter() {
  return createEntityAdapter<FNPCInternalState>(&NPCIdSelector);
}

struct FNPCSliceState {
  EntityState<FNPCInternalState> Entities;
  FString ActiveNpcId;

  FNPCSliceState() : Entities(GetNPCAdapter().getInitialState()) {}
};

/**
 * Merges a partial state delta into the current NPC state JSON.
 * User Story: As NPC state updates, I need deltas merged into the current
 * state so protocol turns can evolve state without replacing everything.
 */
inline FAgentState MergeStateDelta(const FAgentState &Current,
                                   const FAgentState &Delta) {
  return (Delta.JsonData.IsEmpty() || Delta.JsonData == TEXT("{}"))
      ? Current
      : (Current.JsonData.IsEmpty() || Current.JsonData == TEXT("{}"))
      ? Delta
      : [&]() -> FAgentState {
          TSharedPtr<FJsonObject> CurrentJson = MakeShared<FJsonObject>();
          TSharedPtr<FJsonObject> DeltaJson = MakeShared<FJsonObject>();
          return (!FJsonSerializer::Deserialize(
                      TJsonReaderFactory<>::Create(Current.JsonData),
                      CurrentJson) ||
                  !CurrentJson.IsValid())
              ? Delta
              : (!FJsonSerializer::Deserialize(
                      TJsonReaderFactory<>::Create(Delta.JsonData),
                      DeltaJson) ||
                  !DeltaJson.IsValid())
              ? Delta
              : [&]() -> FAgentState {
                  TArray<FString> Keys;
                  DeltaJson->Values.GetKeys(Keys);
                  struct MergeFields {
                    static void apply(
                        const TSharedPtr<FJsonObject> &Src,
                        const TSharedPtr<FJsonObject> &Dst,
                        const TArray<FString> &K,
                        int32 Idx) {
                      Idx >= K.Num()
                          ? void()
                          : (Dst->SetField(K[Idx], Src->TryGetField(K[Idx])),
                             apply(Src, Dst, K, Idx + 1), void());
                    }
                  };
                  MergeFields::apply(DeltaJson, CurrentJson, Keys, 0);
                  FString MergedJson;
                  FJsonSerializer::Serialize(
                      CurrentJson.ToSharedRef(),
                      TJsonWriterFactory<>::Create(&MergedJson));
                  return TypeFactory::AgentState(MergedJson);
                }();
        }();
}

/**
 * Builds a timestamped state-log entry from delta and resulting state.
 * User Story: As NPC audit trails, I need state log entries created so state
 * transitions can be inspected after updates occur.
 */
inline FNPCStateLogEntry MakeStateLogEntry(const FAgentState &Delta,
                                           const FAgentState &State) {
  FNPCStateLogEntry Entry;
  Entry.Timestamp = FDateTime::UtcNow().ToUnixTimestamp();
  Entry.Delta = Delta;
  Entry.State = State;
  return Entry;
}

} // namespace NPCSlice
