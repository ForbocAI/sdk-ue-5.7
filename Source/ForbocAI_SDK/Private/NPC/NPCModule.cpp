#include "NPC/NPCModule.h"
#include "NPC/NPCId.h"
#include "Containers/UnrealString.h"
#include "Core/functional_core.hpp"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Guid.h"
#include "RuntimeConfig.h"
#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

namespace {

/**
 * Serializes NPC process context into the condensed JSON payload expected by the API.
 * User Story: As NPC protocol calls, I need contextual key/value data encoded
 * into JSON so remote processing receives the same context the game provided.
 */
FString SerializeContextMap(const TMap<FString, FString> &Context) {
  return Context.IsEmpty()
             ? FString(TEXT("{}"))
             : [&]() -> FString {
                 const TSharedRef<FJsonObject> Root =
                     MakeShared<FJsonObject>();
                 TArray<FString> Keys;
                 Context.GetKeys(Keys);
                 struct AddEntries {
                   static void apply(
                       const TMap<FString, FString> &Map,
                       const TSharedRef<FJsonObject> &Obj,
                       const TArray<FString> &K, int32 Idx) {
                     Idx >= K.Num()
                         ? void()
                         : (Obj->SetStringField(K[Idx],
                                                *Map.Find(K[Idx])),
                            apply(Map, Obj, K, Idx + 1), void());
                   }
                 };
                 AddEntries::apply(Context, Root, Keys, 0);
                 FString Json;
                 const TSharedRef<TJsonWriter<
                     TCHAR, TCondensedJsonPrintPolicy<TCHAR>>>
                     Writer =
                         TJsonWriterFactory<
                             TCHAR,
                             TCondensedJsonPrintPolicy<TCHAR>>::
                             Create(&Json);
                 FJsonSerializer::Serialize(Root, Writer);
                 return Json;
               }();
}

} // namespace

/**
 * Creates a runtime agent from config, filling defaults where needed.
 * User Story: As agent setup flows, I need configs normalized into runtime
 * agents so callers do not reimplement default id and API resolution.
 */
FAgent AgentFactory::Create(const FAgentConfig &Config) {
  FAgent Agent;
  Agent.Id = Config.Id.IsEmpty() ? NPCId::GenerateNPCId() : Config.Id;
  Agent.Persona = Config.Persona;
  Agent.State = Config.InitialState;
  Agent.ApiUrl =
      Config.ApiUrl.IsEmpty() ? SDKConfig::GetApiUrl() : Config.ApiUrl;
  return Agent;
}

/**
 * Rehydrates a runtime agent from a soul snapshot and API override.
 * User Story: As soul import flows, I need soul payloads converted back into
 * live agents so runtime state can resume from portable exports.
 */
FAgent AgentFactory::FromSoul(const FSoul &Soul, const FString &ApiUrl) {
  FAgent Agent;
  Agent.Id = Soul.Id;
  Agent.Persona = Soul.Persona;
  Agent.State = Soul.State;
  Agent.Memories = Soul.Memories;
  Agent.ApiUrl =
      ApiUrl.IsEmpty() ? SDKConfig::GetApiUrl() : ApiUrl;
  return Agent;
}

/**
 * Returns a copy of the agent with updated state data.
 * User Story: As immutable agent updates, I need state changes applied by copy
 * so higher-level flows can treat agents as persistent values.
 */
FAgent AgentOps::WithState(const FAgent &Agent, const FAgentState &NewState) {
  FAgent Updated = Agent;
  Updated.State = NewState;
  return Updated;
}

/**
 * Returns a copy of the agent with a replaced memory collection.
 * User Story: As immutable agent updates, I need memory changes applied by
 * copy so higher-level flows can preserve value semantics.
 */
FAgent AgentOps::WithMemories(const FAgent &Agent,
                              const TArray<FMemoryItem> &NewMemories) {
  FAgent Updated = Agent;
  Updated.Memories = NewMemories;
  return Updated;
}

/**
 * Merges incoming state updates using the API's full-state response contract.
 * User Story: As agent update flows, I need API state payloads normalized so
 * full-state responses replace stale local snapshots safely. Non-empty
 * `Updates.JsonData` is treated as the full replacement state, while empty
 * payloads preserve the current snapshot.
 */
FAgentState AgentOps::CalculateNewState(const FAgentState &Current,
                                        const FAgentState &Updates) {
  return (Updates.JsonData.IsEmpty() || Updates.JsonData == TEXT("{}"))
             ? Current
             : TypeFactory::AgentState(Updates.JsonData);
}

#include "RuntimeStore.h"
#include "Protocol/ProtocolThunks.h"

/**
 * Runs a protocol turn for the agent against a temporary store instance.
 * User Story: As standalone agent processing, I need a temporary store-backed
 * protocol run so agent calls can reuse the full runtime loop off-store.
 */
AgentTypes::AsyncResult<FAgentResponse>
AgentOps::Process(const FAgent &Agent, const FString &Input,
                  const TMap<FString, FString> &Context) {
  return AgentTypes::AsyncResult<FAgentResponse>::create(
      [Agent, Input, Context](std::function<void(FAgentResponse)> resolve,
                              std::function<void(std::string)> reject) {
        SDKConfig::SetApiConfig(Agent.ApiUrl, SDKConfig::GetApiKey());
        auto Store = MakeShared<rtk::EnhancedStore<FStoreState>>(createStore());

        Store->dispatch(rtk::processNPC(Agent.Id, Input,
                                        SerializeContextMap(Context),
                                        Agent.Persona, Agent.State,
                                        rtk::LocalProtocolRuntime()))
            .then([resolve, Store](const FAgentResponse &Response) {
              resolve(Response);
            })
            .catch_([reject, Store](std::string Error) { reject(Error); })
            .execute();
      });
}

/**
 * Exports the agent into the portable soul representation used by the SDK.
 * User Story: As soul export flows, I need live agents converted into portable
 * soul payloads so they can be saved or transferred across runtimes.
 */
FSoul AgentOps::Export(const FAgent &Agent) {
  return TypeFactory::Soul(Agent.Id, TEXT("1.0.0"), TEXT("NPC"),
                           Agent.Persona, Agent.State, Agent.Memories);
}
