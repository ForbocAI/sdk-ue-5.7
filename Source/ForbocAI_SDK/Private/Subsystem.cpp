#include "RuntimeSubsystem.h"
#include "NPC/NPCSlice.h"
#include "RuntimeConfig.h"
#include "RuntimeStore.h"
#include "Protocol/ProtocolThunks.h"
#include "Soul/SoulThunks.h"

/**
 * Initializes the runtime store and wires action middleware for broadcasts.
 * User Story: As game runtime startup, I need the subsystem to create and wire
 * the store so gameplay events can observe SDK state changes. This registers
 * the NPC-removal listener and the action-broadcast middleware before store
 * creation.
 */
void UForbocAISubsystem::Initialize(FSubsystemCollectionBase &Collection) {
  Super::Initialize(Collection);

  std::vector<rtk::Middleware<FStoreState>> Middlewares;
  Middlewares.push_back(createNpcRemovalListener());

  Middlewares.push_back([this](const rtk::MiddlewareApi<FStoreState> &Api)
                            -> std::function<rtk::Dispatcher(rtk::Dispatcher)> {
    return [this](rtk::Dispatcher Next) -> rtk::Dispatcher {
      return [this, Next](const rtk::AnyAction &Action) {
        this->HandleAction(Action);
        return Next(Action);
      };
    };
  });

  Store = MakeShared<rtk::EnhancedStore<FStoreState>>(
      rtk::configureStore<FStoreState>(&StoreReducer, FStoreState(),
                                       Middlewares));
}

/**
 * Releases the runtime store during subsystem shutdown.
 * User Story: As game runtime shutdown, I need the subsystem to release store
 * resources so teardown does not leak runtime state.
 */
void UForbocAISubsystem::Deinitialize() {
  Store.Reset();
  Super::Deinitialize();
}

/**
 * Applies API credentials and URL overrides to the runtime config.
 * User Story: As subsystem setup flows, I need one init entry point for API
 * config so gameplay code can point the runtime at the right backend.
 */
void UForbocAISubsystem::Init(FString ApiKey, FString ApiUrl) {
  SDKConfig::SetApiConfig(
      ApiUrl.IsEmpty() ? FString(SDKConfig::DEFAULT_API_URL) : ApiUrl, ApiKey);
}

/**
 * Runs a protocol turn for an NPC and broadcasts dialogue or actions.
 * User Story: As gameplay interaction flows, I need NPC turns processed from
 * the subsystem so dialogue, typing, and action events are broadcast to game code.
 */
void UForbocAISubsystem::ProcessNPC(FString NpcId, FString Input) {
  !Store.IsValid()
      ? void()
      : (OnTypingStart.Broadcast(),
         Store
             ->dispatch(rtk::processNPC(NpcId, Input, TEXT("{}"), TEXT(""),
                                        FAgentState(),
                                        rtk::LocalProtocolRuntime()))
             .then([this](const FAgentResponse &Result) {
               !Result.Dialogue.IsEmpty()
                   ? (OnMessageReceived.Broadcast(Result.Dialogue),
                      OnTTSRequested.Broadcast(Result.Dialogue), void())
                   : void();
               !Result.Action.Type.IsEmpty()
                   ? (OnNPCActionReceived.Broadcast(Result.Action), void())
                   : void();
               OnTypingEnd.Broadcast();
             })
             .catch_([this](std::string Error) {
               static_cast<void>(Error);
               OnTypingEnd.Broadcast();
             })
             .execute());
}

/**
 * Exports an NPC soul and broadcasts the completed transaction id.
 * User Story: As gameplay soul-export flows, I need subsystem-triggered export
 * events so game code can react when a soul has been published.
 */
void UForbocAISubsystem::ExportSoul(FString AgentId) {
  !Store.IsValid()
      ? void()
      : (void)Store->dispatch(rtk::exportSoulThunk(AgentId))
            .then([this](const FSoulExportResult &Result) {
              OnSoulExportComplete.Broadcast(Result.TxId);
            })
            .execute();
}

/**
 * Returns the latest state snapshot for the requested NPC.
 * User Story: As gameplay state queries, I need the latest NPC state from the
 * subsystem so UI and logic can inspect current agent data.
 */
FAgentState UForbocAISubsystem::GetNPCState(FString NpcId) const {
  return !Store.IsValid()
             ? FAgentState()
             : func::or_else(
                   func::fmap(
                       NPCSlice::SelectNPCById(Store->getState().NPCs, NpcId),
                       [](const FNPCInternalState &Npc) { return Npc.State; }),
                   FAgentState());
}

/**
 * Returns the id of the active NPC, if any.
 * User Story: As gameplay targeting flows, I need the active NPC id so other
 * systems can address the current actor consistently.
 */
FString UForbocAISubsystem::GetActiveNPCId() const {
  return !Store.IsValid()
             ? FString()
             : NPCSlice::SelectActiveNpcId(Store->getState().NPCs);
}

/**
 * Writes the active NPC into OutNPC when one is present.
 * User Story: As gameplay state queries, I need the active NPC materialized so
 * consumers can read the full runtime record without manual store access.
 */
bool UForbocAISubsystem::GetActiveNPC(FNPCInternalState &OutNPC) const {
  return !Store.IsValid()
             ? false
             : func::match(
                   NPCSlice::SelectActiveNPC(Store->getState().NPCs),
                   [&OutNPC](const FNPCInternalState &Active) {
                     OutNPC = Active;
                     return true;
                   },
                   []() { return false; });
}

/**
 * Returns the last memory recall result emitted by the store.
 * User Story: As gameplay memory UIs, I need the last recall batch so recent
 * memory results can be rendered without reissuing the query.
 */
TArray<FMemoryItem> UForbocAISubsystem::GetLastRecalledMemories() const {
  return !Store.IsValid()
             ? TArray<FMemoryItem>()
             : MemorySlice::SelectLastRecalledMemories(
                   Store->getState().Memory);
}

/**
 * Writes the most recent bridge validation result into OutResult.
 * User Story: As gameplay validation feedback, I need the latest bridge result
 * so designers can inspect whether an action was valid.
 */
bool UForbocAISubsystem::GetLastBridgeValidation(
    FValidationResult &OutResult) const {
  return !Store.IsValid() ? false
                          : [this, &OutResult]() -> bool {
    const FStoreState State = Store->getState();
    return State.Bridge.bHasLastValidation
               ? (OutResult = State.Bridge.LastValidation, true)
               : false;
  }();
}

/**
 * Writes the last imported soul into OutSoul when one exists.
 * User Story: As gameplay soul-import flows, I need the latest imported soul
 * exposed so game tools can inspect the restored payload.
 */
bool UForbocAISubsystem::GetLastImportedSoul(FSoul &OutSoul) const {
  return !Store.IsValid() ? false
                          : [this, &OutSoul]() -> bool {
    const FStoreState State = Store->getState();
    return State.Soul.bHasLastImport
               ? (OutSoul = State.Soul.LastImport, true)
               : false;
  }();
}

/**
 * Broadcasts selected store actions through the subsystem event delegates.
 * User Story: As gameplay event listeners, I need store actions translated
 * into delegates so blueprint and C++ subscribers can react to runtime changes.
 */
void UForbocAISubsystem::HandleAction(const rtk::AnyAction &Action) {
  NPCSlice::Actions::SetLastActionActionCreator().match(Action)
      ? [this, &Action]() {
          const auto Payload =
              NPCSlice::Actions::SetLastActionActionCreator().extract(Action);
          Payload.hasValue
              ? (OnNPCActionReceived.Broadcast(Payload.value.Action), void())
              : void();
        }()
      : void();
}
