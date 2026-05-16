#include "RuntimeBlueprintLibrary.h"
#include "CLI/CliOperations.h"
#include "RuntimeStore.h"

namespace {
/**
 * Returns the singleton store used by blueprint convenience wrappers.
 * User Story: As blueprint utility calls, I need one shared store so
 * convenience functions reflect the same runtime state across blueprint nodes.
 */
rtk::EnhancedStore<FStoreState> &GetBPStore() {
  static rtk::EnhancedStore<FStoreState> Store = ConfigureStore();
  return Store;
}
} // namespace

/**
 * Returns the remote API status string exposed to blueprints.
 * User Story: As blueprint status panels, I need the API health string so
 * designers can inspect connectivity without writing C++.
 */
FString UForbocAIBlueprintLibrary::CheckApiStatus() {
  FApiStatusResponse Resp = Ops::CheckApiStatus(GetBPStore());
  return Resp.Status;
}

/**
 * Creates an NPC and returns the generated NPC id.
 * User Story: As blueprint setup flows, I need a node that creates NPCs and
 * returns ids so designers can spawn runtime agents without C++.
 */
FString UForbocAIBlueprintLibrary::CreateNpc(const FString &Persona) {
  FNPCInternalState Npc = Ops::CreateNpc(GetBPStore(), Persona);
  return Npc.Id;
}

/**
 * Processes NPC input and returns the dialogue string only.
 * User Story: As blueprint interaction flows, I need dialogue-only processing
 * so designers can wire NPC text responses into UI quickly.
 */
FString UForbocAIBlueprintLibrary::ProcessNpc(const FString &NpcId,
                                               const FString &Text) {
  FAgentResponse Resp = Ops::ProcessNpc(GetBPStore(), NpcId, Text);
  return Resp.Dialogue;
}

/**
 * Processes a chat-style NPC input and returns the dialogue string.
 * User Story: As blueprint chat flows, I need a chat-friendly wrapper so
 * dialogue requests can be triggered with minimal blueprint setup.
 */
FString UForbocAIBlueprintLibrary::ChatNpc(const FString &NpcId,
                                            const FString &Message) {
  FAgentResponse Resp = Ops::ProcessNpc(GetBPStore(), NpcId, Message);
  return Resp.Dialogue;
}

/**
 * Reports whether the shared blueprint store currently has an active NPC.
 * User Story: As blueprint control flow, I need to know whether an active NPC
 * exists so graphs can branch safely before issuing commands.
 */
bool UForbocAIBlueprintLibrary::HasActiveNpc() {
  func::Maybe<FNPCInternalState> Active = Ops::GetActiveNpc(GetBPStore());
  return Active.hasValue;
}

/**
 * Stores a new memory observation for the given NPC.
 * User Story: As blueprint memory flows, I need a node that stores
 * observations so designers can persist events without custom C++ glue.
 */
void UForbocAIBlueprintLibrary::MemoryStore(const FString &NpcId,
                                             const FString &Observation) {
  Ops::MemoryStore(GetBPStore(), NpcId, Observation);
}

/**
 * Clears all stored memories for the given NPC.
 * User Story: As blueprint reset flows, I need a node that clears memories so
 * tests and scripted resets can start from a clean slate.
 */
void UForbocAIBlueprintLibrary::MemoryClear(const FString &NpcId) {
  Ops::MemoryClear(GetBPStore(), NpcId);
}

/**
 * Starts a ghost run and returns the remote session id.
 * User Story: As blueprint automation flows, I need a node that launches ghost
 * runs so designers can trigger runtime tests from gameplay tools.
 */
FString UForbocAIBlueprintLibrary::GhostRun(const FString &TestSuite,
                                             int32 Duration) {
  FGhostRunResponse Resp = Ops::GhostRun(GetBPStore(), TestSuite, Duration);
  return Resp.SessionId;
}

/**
 * Stops a ghost run and returns the stop status string.
 * User Story: As blueprint automation flows, I need a node that stops ghost
 * runs so long-running tests can be cancelled from gameplay tools.
 */
FString UForbocAIBlueprintLibrary::GhostStop(const FString &SessionId) {
  FGhostStopResponse Resp = Ops::GhostStop(GetBPStore(), SessionId);
  return Resp.StopStatus;
}

/**
 * Exports an NPC soul and returns the resulting transaction id.
 * User Story: As blueprint soul-export flows, I need a node that exports souls
 * so designers can persist NPC snapshots without custom code.
 */
FString UForbocAIBlueprintLibrary::ExportSoul(const FString &NpcId) {
  FSoulExportResult Result = Ops::ExportSoul(GetBPStore(), NpcId);
  return Result.TxId;
}

/**
 * Imports a soul and returns the imported soul id.
 * User Story: As blueprint soul-import flows, I need a node that imports souls
 * so designers can restore NPC data from transactions directly.
 */
FString UForbocAIBlueprintLibrary::ImportSoul(const FString &TxId) {
  FSoul Soul = Ops::ImportSoul(GetBPStore(), TxId);
  return Soul.Id;
}

/**
 * Verifies a soul transaction and returns whether it is valid.
 * User Story: As blueprint verification flows, I need a node that validates
 * soul transactions so trust checks can be built without C++.
 */
bool UForbocAIBlueprintLibrary::VerifySoul(const FString &TxId) {
  FSoulVerifyResult Result = Ops::VerifySoul(GetBPStore(), TxId);
  return Result.bValid;
}

/**
 * Validates a bridge action represented as raw JSON.
 * User Story: As blueprint bridge-validation flows, I need a node that checks
 * raw actions so design tools can test bridge rules without native code.
 */
bool UForbocAIBlueprintLibrary::ValidateBridgeAction(const FString &ActionJson) {
  FAgentAction Action;
  Action.PayloadJson = ActionJson;
  FBridgeValidationContext Context;
  FValidationResult Result = Ops::ValidateBridge(GetBPStore(), Action, Context);
  return Result.bValid;
}

/**
 * Persists a runtime config value from blueprint code.
 * User Story: As blueprint config tools, I need a node that persists settings
 * so designers can update runtime config without editing files manually.
 */
void UForbocAIBlueprintLibrary::ConfigSet(const FString &Key,
                                           const FString &Value) {
  Ops::ConfigSet(Key, Value);
}

/**
 * Reads a runtime config value from blueprint code.
 * User Story: As blueprint config tools, I need a node that reads settings so
 * designers can inspect runtime config without C++ helpers.
 */
FString UForbocAIBlueprintLibrary::ConfigGet(const FString &Key) {
  return Ops::ConfigGet(Key);
}
