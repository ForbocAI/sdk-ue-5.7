#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CLI/CliOperations.h"
#include "RuntimeBlueprintLibrary.generated.h"

/**
 * Blueprint-callable wrappers around Ops.
 * Exposes the core ForbocAI operations to Blueprints.
 * User Story: As a Blueprint integrator, I need this API note so I can call the SDK surface correctly from Unreal gameplay code.
 */
UCLASS()
class FORBOCAI_SDK_API UForbocAIBlueprintLibrary
    : public UBlueprintFunctionLibrary {
  GENERATED_BODY()

public:
  /**
   * ---- System ----
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */

  UFUNCTION(BlueprintCallable, Category = "ForbocAI|System")
  static FString CheckApiStatus();

  /**
   * ---- NPC ----
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */

  UFUNCTION(BlueprintCallable, Category = "ForbocAI|NPC")
  static FString CreateNpc(const FString &Persona);

  UFUNCTION(BlueprintCallable, Category = "ForbocAI|NPC")
  static FString ProcessNpc(const FString &NpcId, const FString &Text);

  /**
   * Sends a chat message to an NPC and returns the dialogue response.
   * Convenience wrapper around ProcessNpc for conversational use.
   * @param NpcId  The NPC to chat with.
   * @param Message  The player's message.
   * @return The NPC's dialogue response.
   * User Story: As an SDK integrator, I need this type or module note so I can understand the role of the surrounding API surface quickly.
   */
  UFUNCTION(BlueprintCallable, Category = "ForbocAI|NPC",
            meta = (DisplayName = "Chat With NPC"))
  static FString ChatNpc(const FString &NpcId, const FString &Message);

  UFUNCTION(BlueprintCallable, Category = "ForbocAI|NPC")
  static bool HasActiveNpc();

  /**
   * ---- Memory ----
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */

  UFUNCTION(BlueprintCallable, Category = "ForbocAI|Memory")
  static void MemoryStore(const FString &NpcId, const FString &Observation);

  UFUNCTION(BlueprintCallable, Category = "ForbocAI|Memory")
  static void MemoryClear(const FString &NpcId);

  /**
   * ---- Ghost ----
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */

  UFUNCTION(BlueprintCallable, Category = "ForbocAI|Ghost")
  static FString GhostRun(const FString &TestSuite, int32 Duration = 300);

  UFUNCTION(BlueprintCallable, Category = "ForbocAI|Ghost")
  static FString GhostStop(const FString &SessionId);

  /**
   * ---- Soul ----
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */

  UFUNCTION(BlueprintCallable, Category = "ForbocAI|Soul")
  static FString ExportSoul(const FString &NpcId);

  UFUNCTION(BlueprintCallable, Category = "ForbocAI|Soul")
  static FString ImportSoul(const FString &TxId);

  UFUNCTION(BlueprintCallable, Category = "ForbocAI|Soul")
  static bool VerifySoul(const FString &TxId);

  /**
   * ---- Bridge ----
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */

  UFUNCTION(BlueprintCallable, Category = "ForbocAI|Bridge")
  static bool ValidateBridgeAction(const FString &ActionJson);

  /**
   * ---- Config ----
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */

  UFUNCTION(BlueprintCallable, Category = "ForbocAI|Config")
  static void ConfigSet(const FString &Key, const FString &Value);

  UFUNCTION(BlueprintCallable, Category = "ForbocAI|Config")
  static FString ConfigGet(const FString &Key);
};
