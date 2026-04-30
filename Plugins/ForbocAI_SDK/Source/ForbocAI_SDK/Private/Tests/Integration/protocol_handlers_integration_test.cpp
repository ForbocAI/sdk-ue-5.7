/**
 * Protocol handlers isolation tests — against live API
 * Requires FORBOCAI_API_KEY. Set FORBOCAI_API_URL for production.
 * NO MOCKING ALLOWED.
 */

#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "NPC/NPCSlice.h"
#include "Protocol/ProtocolThunks.h"
#include "RuntimeConfig.h"
#include "RuntimeStore.h"

using namespace rtk;

struct FHandlerTestState {
  bool bCompleted = false;
  bool bSuccess = false;
  FString Error;
  FAgentResponse Response;
  TSharedPtr<rtk::EnhancedStore<FStoreState>> Store;
};

struct FHandlerTestParams {
  FString NpcId;
  FString Input;
  FString Persona;
  int32 HandlerType; // 0 for IdentifyActor, 1 for Decision, 2 for Reasoning
};

DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(
    FHandlerWaitComplete, TSharedPtr<FHandlerTestState>, State,
    FHandlerTestParams, Params, int32, PollCount);
bool FHandlerWaitComplete::Update() {
  const int32 MaxPolls = 300;

  if (!State->Store.IsValid()) {
    const TSharedPtr<FHandlerTestState> SharedState = State;
    State->Store = MakeShared<rtk::EnhancedStore<FStoreState>>(createStore());

    FNPCInternalState Info;
    Info.Id = Params.NpcId;
    Info.Persona = Params.Persona;
    State->Store->dispatch(NPCSlice::Actions::SetNPCInfo(Info));
    State->Store->dispatch(NPCSlice::Actions::SetActiveNPC(Params.NpcId));

    FString RunId = FString::Printf(TEXT("%s:run"), *Params.NpcId);
    FNPCProcessTape Tape = TypeFactory::ProcessTape(Params.Input, TEXT("{}"), FAgentState(), Params.Persona);
    
    // Create a fake response to pass to the handler
    FNPCProcessResponse FakeResponse;
    FakeResponse.Tape = Tape;
    
    func::AsyncResult<FAgentResponse> ResultPromise = func::RejectAsync<FAgentResponse>("Init");

    if (Params.HandlerType == 0) {
      // IdentifyActor
      FakeResponse.Instruction.Type = ENPCInstructionType::IdentifyActor;
      ResultPromise = detail::HandleIdentifyActor(FakeResponse, Params.NpcId, Params.Input, RunId, 0, LocalProtocolRuntime(), State->Store->dispatch, State->Store->getState);
    } else if (Params.HandlerType == 1) {
      // Decision
      FakeResponse.Instruction.Type = ENPCInstructionType::Decision;
      FakeResponse.Tape.bVectorQueried = true;
      ResultPromise = detail::HandleDecision(FakeResponse, Params.NpcId, Params.Input, RunId, 1, LocalProtocolRuntime(), State->Store->dispatch, State->Store->getState);
    } else if (Params.HandlerType == 2) {
      // Reasoning
      FakeResponse.Instruction.Type = ENPCInstructionType::Reasoning;
      FakeResponse.Tape.bVectorQueried = true;
      FakeResponse.Tape.bDecisionCompleted = true;
      FakeResponse.Tape.DecisionIntent.Goal = TEXT("Respond to input");
      FakeResponse.Tape.DecisionIntent.ActionType = TEXT("SPEAK");
      FakeResponse.Tape.ReasoningOutput.ReasoningText = TEXT("Server side reasoning");
      FakeResponse.Tape.ReasoningOutput.ResponseText = TEXT("Server side response");
      FakeResponse.Tape.GeneratedOutput = TEXT("Server side response");
      ResultPromise = detail::HandleReasoning(FakeResponse, Params.NpcId, Params.Input, RunId, 2, LocalProtocolRuntime(), State->Store->dispatch, State->Store->getState);
    }

    ResultPromise
        .then([SharedState](const FAgentResponse &R) {
          SharedState->bCompleted = true;
          SharedState->bSuccess = true;
          SharedState->Response = R;
        })
        .catch_([SharedState](std::string E) {
          SharedState->bCompleted = true;
          SharedState->bSuccess = false;
          SharedState->Error = FString(UTF8_TO_TCHAR(E.c_str()));
        })
        .execute();
    return false;
  }
  if (State->bCompleted)
    return true;
  if (++PollCount >= MaxPolls) {
    State->bCompleted = true;
    State->bSuccess = false;
    State->Error = TEXT("Timeout waiting for handler");
    return true;
  }
  FPlatformProcess::Sleep(0.05f);
  return false;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FHandleIdentifyActorIntegrationTest,
    "ForbocAI.Integration.Protocol.HandleIdentifyActorLive",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FHandleIdentifyActorIntegrationTest::RunTest(const FString &Parameters) {
  SDKConfig::SetApiConfig(SDKConfig::GetApiUrl(),
                          FPlatformMisc::GetEnvironmentVariable(
                              TEXT("FORBOCAI_API_KEY")));

  auto State = MakeShared<FHandlerTestState>();
  ADD_LATENT_AUTOMATION_COMMAND(FHandlerWaitComplete(
      State, FHandlerTestParams{TEXT("npc_id_1"), TEXT("Hello"), TEXT("Tester"), 0},
      0));

  ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand(
      [this, State]() {
        TestTrue("handler completed", State->bCompleted);
        if (!State->bCompleted)
          return;
        TestTrue("handler succeeded", State->bSuccess);
        if (!State->bSuccess) {
          AddError(FString::Printf(TEXT("API error: %s"), *State->Error));
        }
      },
      0.01f));

  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FHandleDecisionIntegrationTest,
    "ForbocAI.Integration.Protocol.HandleDecisionLive",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FHandleDecisionIntegrationTest::RunTest(const FString &Parameters) {
  SDKConfig::SetApiConfig(SDKConfig::GetApiUrl(),
                          FPlatformMisc::GetEnvironmentVariable(
                              TEXT("FORBOCAI_API_KEY")));

  auto State = MakeShared<FHandlerTestState>();
  ADD_LATENT_AUTOMATION_COMMAND(FHandlerWaitComplete(
      State, FHandlerTestParams{TEXT("npc_decision_1"), TEXT("Hello"), TEXT("Tester"), 1},
      0));

  ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand(
      [this, State]() {
        TestTrue("handler completed", State->bCompleted);
        if (!State->bCompleted)
          return;
        TestTrue("handler succeeded", State->bSuccess);
        if (!State->bSuccess) {
          AddError(FString::Printf(TEXT("API error: %s"), *State->Error));
        }
      },
      0.01f));

  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FHandleReasoningIntegrationTest,
    "ForbocAI.Integration.Protocol.HandleReasoningLive",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FHandleReasoningIntegrationTest::RunTest(const FString &Parameters) {
  SDKConfig::SetApiConfig(SDKConfig::GetApiUrl(),
                          FPlatformMisc::GetEnvironmentVariable(
                              TEXT("FORBOCAI_API_KEY")));

  auto State = MakeShared<FHandlerTestState>();
  ADD_LATENT_AUTOMATION_COMMAND(FHandlerWaitComplete(
      State, FHandlerTestParams{TEXT("npc_reasoning_1"), TEXT("Hello"), TEXT("Tester"), 2},
      0));

  ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand(
      [this, State]() {
        TestTrue("handler completed", State->bCompleted);
        if (!State->bCompleted)
          return;
        TestTrue("handler succeeded", State->bSuccess);
        if (!State->bSuccess) {
          AddError(FString::Printf(TEXT("API error: %s"), *State->Error));
        }
      },
      0.01f));

  return true;
}
