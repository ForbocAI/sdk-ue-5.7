/**
 * Live API Tests for Handler Loop
 * User Story: As protocol orchestration, I need tests that run against the live API so that integration regressions
 * are caught.
 */

#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "API/APIEndpoints.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "RuntimeConfig.h"
#include "RuntimeStore.h"

using namespace rtk;

struct FProcessLiveTestState {
  bool bCompleted = false;
  bool bSuccess = false;
  FString Error;
  FNPCProcessTape Tape;
  int32 Step = 0;
  TSharedPtr<rtk::EnhancedStore<FStoreState>> Store;
};

DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(
    FProcessLiveStepWait, TSharedPtr<FProcessLiveTestState>, State, int32, PollCount);
/**
 * User Story: As a developer, I need Update to fulfill its role in the module.
 */
bool FProcessLiveStepWait::Update() {
  const int32 MaxPolls = 300;  // ~15s at 50ms

  if (State->Step == 0) {
      State->Step = 1;
      State->Store = MakeShared<rtk::EnhancedStore<FStoreState>>(createStore());
      
      FNPCProcessRequest Req;
      Req.Tape = FNPCProcessTape();
      Req.Tape.Observation = TEXT("test observation");
      
      auto Dispatch = [this](const rtk::AnyAction &A) { return State->Store->dispatch(A); };
      auto GetState = [this]() { return State->Store->getState(); };
      
      APISlice::Endpoints::postNpcProcess(TEXT("live_npc_1"), Req)(Dispatch, GetState)
          .then([this](const FNPCProcessResponse &R) {
              if (R.Instruction.Type == TEXT("IdentifyActor")) {
                  State->Step = 2;
                  State->Tape = R.Tape;
              } else {
                  State->bCompleted = true;
                  State->bSuccess = false;
                  State->Error = FString::Printf(TEXT("Expected IdentifyActor, got %s"), *R.Instruction.Type);
              }
          })
          .catch_([this](std::string E) {
              State->bCompleted = true;
              State->bSuccess = false;
              State->Error = FString(UTF8_TO_TCHAR(E.c_str()));
          })
          .execute();
      return false;
  }
  
  if (State->Step == 2) {
      State->Step = 3;
      FNPCProcessRequest Req;
      Req.Tape = State->Tape;
      
      TSharedPtr<FJsonObject> ActorRes = MakeShared<FJsonObject>();
      ActorRes->SetStringField(TEXT("type"), TEXT("IdentifyActorResult"));
      TSharedPtr<FJsonObject> ActorObj = MakeShared<FJsonObject>();
      ActorObj->SetStringField(TEXT("npcId"), TEXT("live_npc_1"));
      ActorObj->SetObjectField(TEXT("data"), MakeShared<FJsonObject>());
      ActorRes->SetObjectField(TEXT("actor"), ActorObj);
      Req.LastResult = ActorRes;
      
      auto Dispatch = [this](const rtk::AnyAction &A) { return State->Store->dispatch(A); };
      auto GetState = [this]() { return State->Store->getState(); };
      
      APISlice::Endpoints::postNpcProcess(TEXT("live_npc_1"), Req)(Dispatch, GetState)
          .then([this](const FNPCProcessResponse &R) {
              if (R.Instruction.Type == TEXT("QueryVector")) {
                  State->Step = 4;
                  State->Tape = R.Tape;
              } else {
                  State->bCompleted = true;
                  State->bSuccess = false;
                  State->Error = FString::Printf(TEXT("Expected QueryVector, got %s"), *R.Instruction.Type);
              }
          })
          .catch_([this](std::string E) {
              State->bCompleted = true;
              State->bSuccess = false;
              State->Error = FString(UTF8_TO_TCHAR(E.c_str()));
          })
          .execute();
      return false;
  }
  
  if (State->Step == 4) {
      State->Step = 5;
      FNPCProcessRequest Req;
      Req.Tape = State->Tape;
      
      TSharedPtr<FJsonObject> QueryRes = MakeShared<FJsonObject>();
      QueryRes->SetStringField(TEXT("type"), TEXT("QueryVectorResult"));
      QueryRes->SetArrayField(TEXT("memories"), TArray<TSharedPtr<FJsonValue>>());
      Req.LastResult = QueryRes;
      
      auto Dispatch = [this](const rtk::AnyAction &A) { return State->Store->dispatch(A); };
      auto GetState = [this]() { return State->Store->getState(); };
      
      APISlice::Endpoints::postNpcProcess(TEXT("live_npc_1"), Req)(Dispatch, GetState)
          .then([this](const FNPCProcessResponse &R) {
              if (R.Instruction.Type == TEXT("Decision")) {
                  State->Step = 6;
                  State->Tape = R.Tape;
              } else {
                  State->bCompleted = true;
                  State->bSuccess = false;
                  State->Error = FString::Printf(TEXT("Expected Decision, got %s"), *R.Instruction.Type);
              }
          })
          .catch_([this](std::string E) {
              State->bCompleted = true;
              State->bSuccess = false;
              State->Error = FString(UTF8_TO_TCHAR(E.c_str()));
          })
          .execute();
      return false;
  }
  
  if (State->Step == 6) {
      State->Step = 7;
      FNPCProcessRequest Req;
      Req.Tape = State->Tape;
      
      TSharedPtr<FJsonObject> DecRes = MakeShared<FJsonObject>();
      DecRes->SetStringField(TEXT("type"), TEXT("Decision"));
      TSharedPtr<FJsonObject> IntentObj = MakeShared<FJsonObject>();
      IntentObj->SetStringField(TEXT("goal"), TEXT("test"));
      IntentObj->SetStringField(TEXT("actionType"), TEXT("SPEAK"));
      DecRes->SetObjectField(TEXT("decisionIntent"), IntentObj);
      Req.LastResult = DecRes;
      
      auto Dispatch = [this](const rtk::AnyAction &A) { return State->Store->dispatch(A); };
      auto GetState = [this]() { return State->Store->getState(); };
      
      APISlice::Endpoints::postNpcProcess(TEXT("live_npc_1"), Req)(Dispatch, GetState)
          .then([this](const FNPCProcessResponse &R) {
              if (R.Instruction.Type == TEXT("Reasoning")) {
                  State->Step = 8;
                  State->Tape = R.Tape;
                  
                  if (!State->Tape.DecisionIntent.IsSet()) {
                      State->bCompleted = true;
                      State->bSuccess = false;
                      State->Error = TEXT("DecisionIntent was not stitched into tape");
                  }
              } else {
                  State->bCompleted = true;
                  State->bSuccess = false;
                  State->Error = FString::Printf(TEXT("Expected Reasoning, got %s"), *R.Instruction.Type);
              }
          })
          .catch_([this](std::string E) {
              State->bCompleted = true;
              State->bSuccess = false;
              State->Error = FString(UTF8_TO_TCHAR(E.c_str()));
          })
          .execute();
      return false;
  }

  if (State->Step == 8) {
      State->Step = 9;
      FNPCProcessRequest Req;
      Req.Tape = State->Tape;
      
      TSharedPtr<FJsonObject> RsgRes = MakeShared<FJsonObject>();
      RsgRes->SetStringField(TEXT("type"), TEXT("Reasoning"));
      TSharedPtr<FJsonObject> OutObj = MakeShared<FJsonObject>();
      OutObj->SetStringField(TEXT("reasoningText"), TEXT("I think"));
      OutObj->SetStringField(TEXT("responseText"), TEXT("I speak"));
      RsgRes->SetObjectField(TEXT("reasoningOutput"), OutObj);
      Req.LastResult = RsgRes;
      
      auto Dispatch = [this](const rtk::AnyAction &A) { return State->Store->dispatch(A); };
      auto GetState = [this]() { return State->Store->getState(); };
      
      APISlice::Endpoints::postNpcProcess(TEXT("live_npc_1"), Req)(Dispatch, GetState)
          .then([this](const FNPCProcessResponse &R) {
              if (R.Instruction.Type == TEXT("Finalize")) {
                  State->Step = 10;
                  State->Tape = R.Tape;
                  
                  if (State->Tape.GeneratedOutput.IsEmpty()) {
                      State->Error = TEXT("GeneratedOutput was not stitched into tape");
                      State->bSuccess = false;
                  } else if (!State->Tape.ReasoningOutput.IsSet()) {
                      State->Error = TEXT("ReasoningOutput was not stitched into tape");
                      State->bSuccess = false;
                  } else {
                      State->bSuccess = true;
                  }
                  State->bCompleted = true;
              } else {
                  State->bCompleted = true;
                  State->bSuccess = false;
                  State->Error = FString::Printf(TEXT("Expected Finalize, got %s"), *R.Instruction.Type);
              }
          })
          .catch_([this](std::string E) {
              State->bCompleted = true;
              State->bSuccess = false;
              State->Error = FString(UTF8_TO_TCHAR(E.c_str()));
          })
          .execute();
      return false;
  }

  if (State->bCompleted)
    return true;
  if (++PollCount >= MaxPolls) {
    State->bCompleted = true;
    State->bSuccess = false;
    State->Error = TEXT("Timeout waiting for API");
    return true;
  }
  FPlatformProcess::Sleep(0.05f);
  return false;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FProtocolHandlersLiveTest,
    "ForbocAI.Integration.Protocol.HandlersLive",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
/**
 * User Story: As a developer, I need RunTest to fulfill its role in the module.
 */
bool FProtocolHandlersLiveTest::RunTest(const FString &Parameters) {
  FString RuntimeUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("FORBOC_RUNTIME_URL"));
  if (RuntimeUrl.IsEmpty()) {
      AddInfo(TEXT("Skip: FORBOC_RUNTIME_URL not set"));
      return true;
  }
  
  SDKConfig::SetApiConfig(RuntimeUrl,
                          FPlatformMisc::GetEnvironmentVariable(
                              TEXT("FORBOCAI_API_KEY")));

  auto State = MakeShared<FProcessLiveTestState>();
  ADD_LATENT_AUTOMATION_COMMAND(FProcessLiveStepWait(State, 0));

  ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand(
      [this, State]() {
        TestTrue("API loop completed", State->bCompleted);
        if (!State->bCompleted)
          return;
        TestTrue(FString::Printf(TEXT("API loop succeeded: %s"), *State->Error), State->bSuccess);
      },
      0.01f));

  return true;
}
