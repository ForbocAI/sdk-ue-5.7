#pragma once

#include "CoreMinimal.h"
#include "RuntimeConfig.h"
#include "RuntimeStore.h"
#include "Types.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RuntimeSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNPCActionReceived, FAgentAction,
                                            Action);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMessageReceived, FString,
                                            Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTTSRequested, FString,
                                            Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTypingStart);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTypingEnd);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSoulExportComplete, FString,
                                            TxId);

/**
 * Forboc AI SDK Subsystem
 * High-level Blueprint-friendly interface to the underlying RTK store and
 * thunks. Provides a bridge between Unreal's object-oriented visual scripting
 * and the functional Redux-style core.
 * User Story: As a Blueprint integrator, I need this API note so I can call the SDK surface correctly from Unreal gameplay code.
 */
UCLASS(BlueprintType, Blueprintable)
class FORBOCAI_SDK_API UForbocAISubsystem : public UGameInstanceSubsystem {
  GENERATED_BODY()

public:
  /**
   * Initializes the subsystem and wires the runtime store.
   * User Story: As Unreal runtime startup, I need subsystem initialization so
   * the SDK store and listeners are ready before gameplay begins.
   */
  virtual void Initialize(FSubsystemCollectionBase &Collection) override;
  /**
   * Tears down the subsystem and releases runtime resources.
   * User Story: As Unreal runtime shutdown, I need subsystem cleanup so the SDK
   * store and listeners do not outlive the game instance.
   */
  virtual void Deinitialize() override;

  /**
   * Initializes the SDK with the provided configuration.
   * User Story: As Blueprint callers, I need runtime SDK configuration so API
   * access can be set up before NPC operations begin.
   */
  UFUNCTION(BlueprintCallable, Category = "Forboc AI|SDK")
  void Init(FString ApiKey, FString ApiUrl = TEXT(""));

  /**
   * Triggers the recursive protocol loop for an NPC.
   * This is an asynchronous operation.
   * User Story: As Blueprint gameplay flows, I need one NPC processing entry
   * point so input can drive the full protocol loop.
   */
  UFUNCTION(BlueprintCallable, Category = "Forboc AI|NPC")
  void ProcessNPC(FString NpcId, FString Input = TEXT(""));

  /**
   * Exports an NPC's Soul to Arweave.
   * User Story: As Blueprint soul export flows, I need a callable export entry
   * point so gameplay code can publish an NPC soul.
   */
  UFUNCTION(BlueprintCallable, Category = "Forboc AI|Soul")
  void ExportSoul(FString AgentId);

  /**
   * Gets the current state of an NPC.
   * User Story: As Blueprint state inspection, I need direct NPC state access
   * so gameplay systems can read the latest agent state.
   */
  UFUNCTION(BlueprintPure, Category = "Forboc AI|NPC")
  FAgentState GetNPCState(FString NpcId) const;

  /**
   * Gets the active NPC id, if any.
   * User Story: As Blueprint UI binding, I need the active NPC id so widgets can
   * focus on the currently selected or processed agent.
   */
  UFUNCTION(BlueprintPure, Category = "Forboc AI|NPC")
  FString GetActiveNPCId() const;

  /**
   * Gets the active NPC internal state, if any.
   * User Story: As Blueprint gameplay systems, I need the active NPC payload so
   * I can read its full runtime state in one call.
   */
  UFUNCTION(BlueprintPure, Category = "Forboc AI|NPC")
  bool GetActiveNPC(FNPCInternalState &OutNPC) const;

  /**
   * Gets the last recalled memories for the active NPC turn.
   * Wrapper over the memory slice selector for Blueprint consumers.
   * User Story: As Blueprint memory views, I need the last recalled memories so
   * gameplay or UI can show what influenced the latest turn.
   */
  UFUNCTION(BlueprintPure, Category = "Forboc AI|Memory")
  TArray<FMemoryItem> GetLastRecalledMemories() const;

  /**
   * Gets the last bridge validation result, if present.
   * User Story: As Blueprint validation feedback, I need the last bridge result
   * so gameplay can inspect whether an action was accepted.
   */
  UFUNCTION(BlueprintPure, Category = "Forboc AI|Bridge")
  bool GetLastBridgeValidation(FValidationResult &OutResult) const;

  /**
   * Gets the last imported Soul, if present.
   * User Story: As Blueprint import flows, I need access to the last imported
   * soul so gameplay can inspect or present the imported data.
   */
  UFUNCTION(BlueprintPure, Category = "Forboc AI|Soul")
  bool GetLastImportedSoul(FSoul &OutSoul) const;

  /**
   * Delegate triggered when a new action is received from the NPC.
   * User Story: As a runtime subscriber, I need this event contract so gameplay and UI systems know when to react to SDK state changes.
   */
  UPROPERTY(BlueprintAssignable, Category = "Forboc AI|Events")
  FOnNPCActionReceived OnNPCActionReceived;

  /**
   * Delegate triggered when finalized dialogue is produced for an NPC turn.
   * User Story: As a runtime subscriber, I need this event contract so gameplay and UI systems know when to react to SDK state changes.
   */
  UPROPERTY(BlueprintAssignable, Category = "Forboc AI|Events")
  FOnMessageReceived OnMessageReceived;

  /**
   * Delegate triggered when finalized dialogue is ready for TTS consumers.
   * User Story: As a runtime subscriber, I need this event contract so gameplay and UI systems know when to react to SDK state changes.
   */
  UPROPERTY(BlueprintAssignable, Category = "Forboc AI|Events")
  FOnTTSRequested OnTTSRequested;

  /**
   * Delegate triggered when an NPC turn begins asynchronous processing.
   * User Story: As a runtime subscriber, I need this event contract so gameplay and UI systems know when to react to SDK state changes.
   */
  UPROPERTY(BlueprintAssignable, Category = "Forboc AI|Events")
  FOnTypingStart OnTypingStart;

  /**
   * Delegate triggered when an NPC turn finishes asynchronous processing.
   * User Story: As a runtime subscriber, I need this event contract so gameplay and UI systems know when to react to SDK state changes.
   */
  UPROPERTY(BlueprintAssignable, Category = "Forboc AI|Events")
  FOnTypingEnd OnTypingEnd;

  /**
   * Delegate triggered when soul export is complete.
   * User Story: As a runtime subscriber, I need this event contract so gameplay and UI systems know when to react to SDK state changes.
   */
  UPROPERTY(BlueprintAssignable, Category = "Forboc AI|Events")
  FOnSoulExportComplete OnSoulExportComplete;

private:
  /**
   * The underlying functional Redux store.
   * User Story: As an SDK integrator, I need this type or module note so I can understand the role of the surrounding API surface quickly.
   */
  TSharedPtr<rtk::EnhancedStore<FStoreState>> Store;

  /**
   * Handles store actions emitted through listener middleware.
   * User Story: As subsystem event bridging, I need action callbacks so the
   * functional store can drive Blueprint-facing delegates.
   */
  void HandleAction(const rtk::AnyAction &Action);
};
