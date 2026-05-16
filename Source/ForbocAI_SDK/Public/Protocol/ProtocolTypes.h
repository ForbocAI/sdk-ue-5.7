#pragma once

// clang-format off
#include "CoreMinimal.h"
#include "Cortex/CortexTypes.h"
#include "Memory/MemoryTypes.h"
#include "NPC/NPCBaseTypes.h"
#include "ProtocolTypes.generated.h"
// clang-format on

UENUM(BlueprintType)
enum class EDirectiveStatus : uint8 { Running, Completed, Failed };

USTRUCT(BlueprintType)
struct FDirectiveRun {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  FString Id;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  FString NpcId;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  FString Observation;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  EDirectiveStatus Status;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  int64 StartedAt;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  int64 CompletedAt;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  FString Error;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  FString MemoryRecallQuery;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  int32 MemoryRecallLimit;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  float MemoryRecallThreshold;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  FString ContextPrompt;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  FCortexConfig ContextConstraints;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  bool bVerdictValid;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  FString VerdictDialogue;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  FString VerdictActionType;

  FDirectiveRun()
      : Status(EDirectiveStatus::Running), StartedAt(0), CompletedAt(0),
        MemoryRecallLimit(0), MemoryRecallThreshold(0.0f),
        bVerdictValid(false) {}
};

USTRUCT(BlueprintType)
struct FMemoryRecallInstruction {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  FString Query;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  int32 Limit;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  float Threshold;

  FMemoryRecallInstruction() : Limit(5), Threshold(0.7f) {}
};

USTRUCT(BlueprintType)
struct FMemoryStoreInstruction {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  FString Text;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  FString Type;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  float Importance;

  FMemoryStoreInstruction() : Importance(0.5f) {}
};

USTRUCT(BlueprintType)
struct FRecalledMemory {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  FString Text;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  FString Type;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  float Importance;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  float Similarity;

  FRecalledMemory() : Importance(0.5f), Similarity(0.0f) {}
};

UENUM(BlueprintType)
enum class ENPCInstructionType : uint8 {
  IdentifyActor,
  QueryVector,
  Decision,
  Reasoning,
  ExecuteInference,
  Finalize
};

USTRUCT(BlueprintType)
struct FNPCInstruction {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  ENPCInstructionType Type;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  FString Query;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  int32 Limit;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  float Threshold;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  FString Prompt;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  FCortexConfig Constraints;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  bool bValid;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  FString Signature;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  TArray<FMemoryStoreInstruction> MemoryStore;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  FAgentState StateTransform;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  FAgentAction Action;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  bool bHasAction;

  UPROPERTY(BlueprintReadOnly, Category = "Protocol")
  FString Dialogue;

  FNPCInstruction()
      : Type(ENPCInstructionType::Finalize), Limit(0), Threshold(0.7f),
        bValid(true), bHasAction(false) {}
};

namespace TypeFactory {

inline FMemoryRecallInstruction
MemoryRecallInstruction(const FString &Query, int32 Limit = 5,
                        float Threshold = 0.7f) {
  FMemoryRecallInstruction Instruction;
  Instruction.Query = Query;
  Instruction.Limit = Limit;
  Instruction.Threshold = Threshold;
  return Instruction;
}

inline FMemoryStoreInstruction
MemoryStoreInstruction(const FString &Text,
                       const FString &Type = TEXT("observation"),
                       float Importance = 0.5f) {
  FMemoryStoreInstruction Instruction;
  Instruction.Text = Text;
  Instruction.Type = Type;
  Instruction.Importance = Importance;
  return Instruction;
}

} // namespace TypeFactory
