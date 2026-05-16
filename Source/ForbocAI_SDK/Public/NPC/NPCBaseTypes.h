#pragma once

#include "CoreMinimal.h"
#include "Memory/MemoryTypes.h"
#include "NPCBaseTypes.generated.h"

/**
 * NPC State — Immutable JSON data snapshot.
 * User Story: As NPC state transport, I need a simple immutable wrapper so
 * arbitrary serialized state can move through SDK flows unchanged.
 *
 * NOTE: Struct names retain the FAgent* prefix for Blueprint compatibility.
 * The file was renamed from AgentTypes.h as part of the Agent→NPC terminology
 * migration. Include "NPC/NPCBaseTypes.h" in new code.
 */
USTRUCT(BlueprintType)
struct FAgentState {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FString JsonData;

  FAgentState() : JsonData(TEXT("{}")) {}
};

/**
 * NPC Action — Immutable action directive.
 * User Story: As action orchestration, I need a typed action payload so
 * validated instructions can move through reducers and bridge checks.
 */
USTRUCT(BlueprintType)
struct FAgentAction {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FString Type;

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FString Target;

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FString Reason;

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  float Confidence;

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FString Signature;

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FString PayloadJson;

  FAgentAction() : Confidence(1.0f) {}
};

/**
 * NPC Entity — Pure immutable data.
 * User Story: As NPC runtime state, I need one immutable entity shape so
 * reducers, thunks, and exports can share the same NPC contract.
 */
USTRUCT(BlueprintType)
struct FAgent {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FString Id;

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FString Persona;

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FAgentState State;

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  TArray<FMemoryItem> Memories;

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FString ApiUrl;

  FAgent() {}
};

/**
 * NPC Configuration — Plain data.
 * User Story: As NPC creation flows, I need a dedicated config payload so new
 * NPCs can be constructed from explicit initialization inputs.
 */
USTRUCT(BlueprintType)
struct FAgentConfig {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FString Id;

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FString Persona;

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FString ApiUrl;

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FAgentState InitialState;

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FString ApiKey;
};

/**
 * NPC Response — Plain data.
 * User Story: As NPC processing flows, I need a typed response payload so
 * dialogue, actions, and state deltas can be handled consistently.
 */
USTRUCT(BlueprintType)
struct FAgentResponse {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FString Dialogue;

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FAgentAction Action;

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FString Thought;
};

/**
 * Imported NPC — Deserialized soul data.
 * User Story: As soul import flows, I need a lightweight imported-NPC shape so
 * recovered persona and serialized data can be rehydrated into runtime state.
 */
USTRUCT(BlueprintType)
struct FImportedNpc {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FString NpcId;

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FString Persona;

  UPROPERTY(BlueprintReadOnly, Category = "NPC")
  FString DataJson;

  FImportedNpc() {}
};

namespace TypeFactory {

/**
 * Builds an agent state value from serialized JSON.
 * User Story: As NPC state hydration, I need a simple factory so JSON payloads
 * can be wrapped into the SDK state type consistently.
 */
inline FAgentState AgentState(FString JsonData) {
  FAgentState S;
  S.JsonData = MoveTemp(JsonData);
  return S;
}

/**
 * Builds an agent action value from type, target, and reason.
 * User Story: As action construction, I need a factory so gameplay and protocol
 * code can create normalized action payloads without manual field wiring.
 */
inline FAgentAction Action(FString Type, FString Target,
                           FString Reason = TEXT("")) {
  FAgentAction A;
  A.Type = MoveTemp(Type);
  A.Target = MoveTemp(Target);
  A.Reason = MoveTemp(Reason);
  return A;
}

/**
 * Builds an imported NPC payload from core soul metadata.
 * User Story: As soul import flows, I need a factory so imported NPC metadata
 * can be packaged into one transferable structure.
 */
inline FImportedNpc ImportedNpc(FString NpcId, FString Persona,
                                FString DataJson = TEXT("{}")) {
  FImportedNpc N;
  N.NpcId = MoveTemp(NpcId);
  N.Persona = MoveTemp(Persona);
  N.DataJson = MoveTemp(DataJson);
  return N;
}

} // namespace TypeFactory
