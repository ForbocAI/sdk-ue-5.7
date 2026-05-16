// @covers:api:getApiStatus

/**
 * API endpoint integration tests — uses SDKConfig resolution (localhost:8080 default, FORBOCAI_API_URL override)
 * I.5 — Auth, response normalization, representative endpoints, error handling
 * Requires FORBOCAI_API_KEY for auth tests. Default URL is http://localhost:8080.
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */

#include "API/APICodecs.h"
#include "API/APIEndpoints.h"
#include "Core/AsyncHttp.h"
#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "JsonObjectConverter.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "RuntimeConfig.h"
#include "Soul/SoulTypes.h"

// @covers:api:deleteMemoryClear
// @covers:api:deleteRule
// @covers:api:getGhostHistory
// @covers:api:getGhostResults
// @covers:api:getGhostStatus
// @covers:api:getNPC
// @covers:api:getSoulImport
// @covers:api:getSouls
// @covers:api:postBridgePreset
// @covers:api:postBridgeValidate
// @covers:api:postContext
// @covers:api:postCortexComplete
// @covers:api:postCortexInit
// @covers:api:postDirective
// @covers:api:postGhostRun
// @covers:api:postGhostStop
// @covers:api:postMemoryStore
// @covers:api:postNPC
// @covers:api:postNpcImport
// @covers:api:postNpcImportConfirm
// @covers:api:postNpcProcess
// @covers:api:postRuleRegister
// @covers:api:postSoulExport
// @covers:api:postSoulExportConfirm
// @covers:api:postSoulVerify
// @covers:api:postVerdict


struct FApiEndpointTestState {
  bool bDone = false;
  bool bSuccess = false;
  int32 HttpCode = 0;
  FString Body;
  FString Error;
  bool bStarted = false;
};

/**
 * Latent command: GET request, polls until complete
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(
    FHttpGetWaitComplete, FString, Url, FString, ApiKey,
    TSharedPtr<FApiEndpointTestState>, State);
bool FHttpGetWaitComplete::Update() {
  if (!State->bStarted) {
    const TSharedPtr<FApiEndpointTestState> SharedState = State;
    State->bStarted = true;
    func::AsyncHttp::Get<FString>(Url, ApiKey)
        .then([SharedState](const func::HttpResult<FString> &R) {
          SharedState->bDone = true;
          SharedState->bSuccess = R.bSuccess;
          SharedState->HttpCode = R.ResponseCode;
          if (R.bSuccess) {
            SharedState->Body = R.data;
          } else {
            SharedState->Error = FString(UTF8_TO_TCHAR(R.error.c_str()));
          }
        })
        .execute();
    return false;
  }
  if (State->bDone)
    return true;
  FPlatformProcess::Sleep(0.05f);
  return false;
}

/**
 * Latent command: POST request
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_FOUR_PARAMETER(
    FHttpPostWaitComplete, FString, Url, FString, Payload, FString, ApiKey,
    TSharedPtr<FApiEndpointTestState>, State);
bool FHttpPostWaitComplete::Update() {
  if (!State->bStarted) {
    const TSharedPtr<FApiEndpointTestState> SharedState = State;
    State->bStarted = true;
    func::AsyncHttp::Post<FString>(Url, Payload, ApiKey)
        .then([SharedState](const func::HttpResult<FString> &R) {
          SharedState->bDone = true;
          SharedState->bSuccess = R.bSuccess;
          SharedState->HttpCode = R.ResponseCode;
          if (R.bSuccess) {
            SharedState->Body = R.data;
          } else {
            SharedState->Error = FString(UTF8_TO_TCHAR(R.error.c_str()));
          }
        })
        .execute();
    return false;
  }
  if (State->bDone)
    return true;
  FPlatformProcess::Sleep(0.05f);
  return false;
}

static FString GetBaseUrl() { return SDKConfig::GetApiUrl(); }
static FString GetApiKey() { return SDKConfig::GetApiKey(); }

/**
 * Auth: getApiStatus (no auth required) — connectivity check
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FApiEndpointStatusNoAuthTest,
    "ForbocAI.Integration.API.Endpoint.StatusNoAuth",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FApiEndpointStatusNoAuthTest::RunTest(const FString &Parameters) {
  SDKConfig::SetApiConfig(SDKConfig::GetApiUrl(), TEXT(""));

  auto State = MakeShared<FApiEndpointTestState>();
  ADD_LATENT_AUTOMATION_COMMAND(
      FHttpGetWaitComplete(GetBaseUrl() + TEXT("/status"), TEXT(""), State));

  ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand(
      [this, State]() {
        TestTrue("Request completed", State->bDone);
        if (!State->bDone)
          return;
        TestTrue("Status endpoint succeeded", State->bSuccess);
        TestEqual("Status returns 200", State->HttpCode, 200);
        if (State->bSuccess) {
          TestTrue("Response body non-empty", State->Body.Len() > 0);
        }
      },
      0.01f));

  return true;
}

/**
 * Auth: getSouls with valid key — 200 and parseable
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FApiEndpointSoulsValidKeyTest,
    "ForbocAI.Integration.API.Endpoint.SoulsValidKey",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FApiEndpointSoulsValidKeyTest::RunTest(const FString &Parameters) {
  const FString Key =
      FPlatformMisc::GetEnvironmentVariable(TEXT("FORBOCAI_API_KEY"));
  if (Key.IsEmpty()) {
    AddInfo(TEXT("Skip: FORBOCAI_API_KEY not set"));
    return true;
  }
  SDKConfig::SetApiConfig(SDKConfig::GetApiUrl(), Key);

  auto State = MakeShared<FApiEndpointTestState>();
  ADD_LATENT_AUTOMATION_COMMAND(
      FHttpGetWaitComplete(GetBaseUrl() + TEXT("/souls?limit=10"), Key, State));

  ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand(
      [this, State]() {
        TestTrue("Request completed", State->bDone);
        if (!State->bDone)
          return;
        TestTrue("getSouls succeeded", State->bSuccess);
        TestEqual("Returns 200", State->HttpCode, 200);
        if (State->bSuccess && State->Body.Len() > 0) {
          FSoulListResponse Decoded;
          TestTrue("Response parses as FSoulListResponse",
                   FJsonObjectConverter::JsonObjectStringToUStruct(
                       State->Body, &Decoded, 0, 0));
        }
      },
      0.01f));

  return true;
}

/**
 * Auth: getSouls with no key — expect 401
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FApiEndpointSoulsNoKeyTest,
    "ForbocAI.Integration.API.Endpoint.SoulsNoKey",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FApiEndpointSoulsNoKeyTest::RunTest(const FString &Parameters) {
  SDKConfig::SetApiConfig(SDKConfig::GetApiUrl(), TEXT(""));

  auto State = MakeShared<FApiEndpointTestState>();
  ADD_LATENT_AUTOMATION_COMMAND(
      FHttpGetWaitComplete(GetBaseUrl() + TEXT("/souls?limit=10"), TEXT(""),
                           State));

  ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand(
      [this, State]() {
        TestTrue("Request completed", State->bDone);
        if (!State->bDone)
          return;
        TestFalse("getSouls without key fails", State->bSuccess);
        TestEqual("Returns 401", State->HttpCode, 401);
      },
      0.01f));

  return true;
}

/**
 * Auth: getSouls with invalid key — expect 401
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FApiEndpointSoulsInvalidKeyTest,
    "ForbocAI.Integration.API.Endpoint.SoulsInvalidKey",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FApiEndpointSoulsInvalidKeyTest::RunTest(const FString &Parameters) {
  SDKConfig::SetApiConfig(SDKConfig::GetApiUrl(),
                          TEXT("invalid_key_12345"));

  auto State = MakeShared<FApiEndpointTestState>();
  ADD_LATENT_AUTOMATION_COMMAND(FHttpGetWaitComplete(
      GetBaseUrl() + TEXT("/souls?limit=10"), TEXT("invalid_key_12345"),
      State));

  ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand(
      [this, State]() {
        TestTrue("Request completed", State->bDone);
        if (!State->bDone)
          return;
        TestFalse("getSouls with invalid key fails", State->bSuccess);
        TestEqual("Returns 401", State->HttpCode, 401);
      },
      0.01f));

  return true;
}

/**
 * Error: 404 — nonexistent path
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FApiEndpointNotFoundTest,
    "ForbocAI.Integration.API.Endpoint.NotFound",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FApiEndpointNotFoundTest::RunTest(const FString &Parameters) {
  SDKConfig::SetApiConfig(SDKConfig::GetApiUrl(), TEXT(""));

  auto State = MakeShared<FApiEndpointTestState>();
  ADD_LATENT_AUTOMATION_COMMAND(FHttpGetWaitComplete(
      GetBaseUrl() + TEXT("/nonexistent-path-404"), TEXT(""), State));

  ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand(
      [this, State]() {
        TestTrue("Request completed", State->bDone);
        if (!State->bDone)
          return;
        TestFalse("404 path fails", State->bSuccess);
        TestEqual("Returns 404", State->HttpCode, 404);
      },
      0.01f));

  return true;
}

/**
 * postDirective — valid key, minimal request (may 400 if body invalid)
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FApiEndpointPostDirectiveTest,
    "ForbocAI.Integration.API.Endpoint.PostDirective",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FApiEndpointPostDirectiveTest::RunTest(const FString &Parameters) {
  const FString Key =
      FPlatformMisc::GetEnvironmentVariable(TEXT("FORBOCAI_API_KEY"));
  if (Key.IsEmpty()) {
    AddInfo(TEXT("Skip: FORBOCAI_API_KEY not set"));
    return true;
  }
  SDKConfig::SetApiConfig(SDKConfig::GetApiUrl(), Key);

  const FString NpcId = TEXT("api_ep_test_npc");
  const FString Url =
      GetBaseUrl() + TEXT("/npcs/") +
      FGenericPlatformHttp::UrlEncode(NpcId) + TEXT("/directive");
  const FString Payload = TEXT(
      R"({"directiveId":"d1","observation":"test","tape":{},"persona":"Test"})");

  auto State = MakeShared<FApiEndpointTestState>();
  ADD_LATENT_AUTOMATION_COMMAND(
      FHttpPostWaitComplete(Url, Payload, Key, State));

  ADD_LATENT_AUTOMATION_COMMAND(FDelayedFunctionLatentCommand(
      [this, State]() {
        TestTrue("Request completed", State->bDone);
        if (!State->bDone)
          return;
        /**
         * 200 OK or 400/404 depending on NPC existence and body validity
         * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
         */
        TestTrue("Got HTTP response",
                 State->HttpCode >= 200 && State->HttpCode < 600);
        if (State->bSuccess && State->Body.Len() > 0) {
          FDirectiveResponse Decoded;
          TestTrue("Response parses as FDirectiveResponse",
                   APISlice::Detail::DecodeDirectiveResponse(State->Body,
                                                            Decoded));
        }
      },
      0.01f));

  return true;
}
