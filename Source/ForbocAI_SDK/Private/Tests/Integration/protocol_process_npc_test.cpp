/**
 * processNPC full protocol loop — uses SDKConfig resolution (localhost:8080 default) — I.2 Thunk Integration Tests
 * Requires FORBOCAI_API_KEY. Set FORBOCAI_API_URL for production. Exercises tape evolution, NPC state, history, block behavior.
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */

#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "DirectiveSlice.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "NPC/NPCSlice.h"
#include "Protocol/ProtocolThunks.h"
#include "RuntimeConfig.h"
#include "RuntimeStore.h"

using namespace rtk;

/**
 * Shared state for latent async: completed, success, store, response, error
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
struct FProcessNPCTestState {
  bool bCompleted = false;
  bool bSuccess = false;
  FString Error;
  FAgentResponse Response;
  TSharedPtr<rtk::EnhancedStore<FStoreState>> Store;
};

struct FProcessNPCParams {
  FString NpcId;
  FString Input;
  FString Persona;
};

/**
 * Starts processNPC and polls until complete. Uses SDKConfig resolution.
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(
    FProcessNPCWaitComplete, TSharedPtr<FProcessNPCTestState>, State,
    FProcessNPCParams, Params, int32, PollCount);
bool FProcessNPCWaitComplete::Update() {
  const int32 MaxPolls = 300;  // ~15s at 50ms

  if (!State->Store.IsValid()) {
    const TSharedPtr<FProcessNPCTestState> SharedState = State;
    State->Store = MakeShared<rtk::EnhancedStore<FStoreState>>(createStore());
    State->Store->dispatch(rtk::processNPC(
        Params.NpcId, Params.Input, TEXT("{}"), Params.Persona, FAgentState(),
        rtk::LocalProtocolRuntime()))
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
    State->Error = TEXT("Timeout waiting for processNPC");
    return true;
  }
  FPlatformProcess::Sleep(0.05f);
  return false;
}

/**
 * Test: processNPC with real API — full flow (valid verdict)
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FProcessNPCMockFinalizeValidTest,
    "ForbocAI.Integration.Protocol.ProcessNPCMockFinalizeValid",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FProcessNPCMockFinalizeValidTest::RunTest(const FString &Parameters) {
  SDKConfig::SetApiConfig(SDKConfig::GetApiUrl(),
                          FPlatformMisc::GetEnvironmentVariable(
                              TEXT("FORBOCAI_API_KEY")));

  auto State = MakeShared<FProcessNPCTestState>();
  ADD_LATENT_AUTOMATION_COMMAND(FProcessNPCWaitComplete(
      State, FProcessNPCParams{TEXT("npc_valid_1"), TEXT("Hello!"),
                               TEXT("A friendly merchant")},
      0));

  ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand(
      [this, State]() {
        TestTrue("processNPC completed", State->bCompleted);
        if (!State->bCompleted)
          return;
        TestTrue("processNPC succeeded", State->bSuccess);
        if (!State->bSuccess) {
          AddError(FString::Printf(TEXT("API error: %s"), *State->Error));
          return;
        }

        FStoreState StoreState = State->Store->getState();
        auto Run = DirectiveSlice::SelectDirectiveById(
            StoreState.Directives,
            DirectiveSlice::SelectActiveDirectiveId(StoreState.Directives));
        TestTrue("Directive run exists", Run.hasValue);
        if (Run.hasValue) {
          TestEqual("Run completed",
                    static_cast<int32>(Run.value.Status),
                    static_cast<int32>(
                        EDirectiveStatus::Completed));
        }

        auto Npc =
            NPCSlice::SelectNPCById(StoreState.NPCs, TEXT("npc_valid_1"));
        TestTrue("NPC exists", Npc.hasValue);
        if (Npc.hasValue) {
          TestTrue("History has entries", Npc.value.History.Num() >= 1);
          TestFalse("NPC not blocked", Npc.value.bIsBlocked);
        }
      },
      0.01f));

  return true;
}

/**
 * Test: processNPC with real API — block behavior (invalid verdict)
 * Uses observation that may be blocked by API rules.
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FProcessNPCMockFinalizeInvalidTest,
    "ForbocAI.Integration.Protocol.ProcessNPCMockFinalizeInvalid",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FProcessNPCMockFinalizeInvalidTest::RunTest(const FString &Parameters) {
  SDKConfig::SetApiConfig(SDKConfig::GetApiUrl(),
                          FPlatformMisc::GetEnvironmentVariable(
                              TEXT("FORBOCAI_API_KEY")));

  auto State = MakeShared<FProcessNPCTestState>();
  ADD_LATENT_AUTOMATION_COMMAND(FProcessNPCWaitComplete(
      State, FProcessNPCParams{TEXT("npc_block_1"),
                               TEXT("I murder the innocent villager."),
                               TEXT("A village guard")},
      0));

  ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand(
      [this, State]() {
        TestTrue("processNPC completed", State->bCompleted);
        if (!State->bCompleted)
          return;
        if (!State->bSuccess) {
          AddError(FString::Printf(TEXT("API error: %s"), *State->Error));
          return;
        }

        FStoreState StoreState = State->Store->getState();
        auto Npc =
            NPCSlice::SelectNPCById(StoreState.NPCs, TEXT("npc_block_1"));
        TestTrue("NPC exists", Npc.hasValue);
        if (Npc.hasValue && Npc.value.bIsBlocked) {
          TestTrue("Block reason set when blocked",
                   !Npc.value.BlockReason.IsEmpty());
        }
      },
      0.01f));

  return true;
}

/**
 * Test: processNPC directive lifecycle — Started -> Completed
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FProcessNPCDirectiveLifecycleTest,
    "ForbocAI.Integration.Protocol.ProcessNPCDirectiveLifecycle",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FProcessNPCDirectiveLifecycleTest::RunTest(const FString &Parameters) {
  SDKConfig::SetApiConfig(SDKConfig::GetApiUrl(),
                          FPlatformMisc::GetEnvironmentVariable(
                              TEXT("FORBOCAI_API_KEY")));

  auto State = MakeShared<FProcessNPCTestState>();
  ADD_LATENT_AUTOMATION_COMMAND(FProcessNPCWaitComplete(
      State, FProcessNPCParams{TEXT("npc_lc_1"), TEXT("What do you sell?"),
                               TEXT("Test merchant")},
      0));

  ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand(
      [this, State]() {
        TestTrue("Completed", State->bCompleted);
        if (!State->bCompleted)
          return;
        if (!State->bSuccess) {
          AddError(FString::Printf(TEXT("API error: %s"), *State->Error));
          return;
        }

        FStoreState StoreState = State->Store->getState();
        FString ActiveId =
            DirectiveSlice::SelectActiveDirectiveId(StoreState.Directives);
        TestFalse("Active directive set", ActiveId.IsEmpty());

        auto Run =
            DirectiveSlice::SelectDirectiveById(StoreState.Directives, ActiveId);
        TestTrue("Run exists", Run.hasValue);
        if (Run.hasValue) {
          TestEqual("Status completed",
                    static_cast<int32>(Run.value.Status),
                    static_cast<int32>(
                        EDirectiveStatus::Completed));
        }
      },
      0.01f));

  return true;
}
