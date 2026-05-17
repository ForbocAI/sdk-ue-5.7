#include "Misc/AutomationTest.h"
#include "RuntimeCommandlet.h"
#include "TestGame/TestGameCommandSurface.h"
#include "TestGame/TestGameLib.h"

// @covers:cli:test_game

using namespace TestGame;

namespace {

CommandSurface::FCommandExecutor MakeCommandletExecutor(
    const FString &FailCommand = FString()) {
  return [FailCommand](const FCommandSpec &Command,
                       CommandSurface::FAliasState &Aliases)
             -> CommandSurface::FCommandOutput {
    (void)Aliases;
    return !FailCommand.IsEmpty() && Command.Command == FailCommand
               ? CommandSurface::FCommandOutput{
                     ETranscriptStatus::Error, TEXT("synthetic failure"),
                     TEXT("synthetic")}
               : CommandSurface::FCommandOutput{ETranscriptStatus::Ok,
                                                TEXT("synthetic success"),
                                                TEXT("synthetic")};
  };
}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTestGameCommandletPipelineSuccessTest,
    "ForbocAI.Integration.TestGame.Commandlet.PipelineSuccess",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
/**
 * User Story: As a developer, I need RunTest to fulfill its role in the module.
 */
bool FTestGameCommandletPipelineSuccessTest::RunTest(
    const FString &Parameters) {
  (void)Parameters;
  UForbocAICommandlet *Commandlet = NewObject<UForbocAICommandlet>();
  bool bCompleted = false;
  FString Error;
  TArray<FString> Args;
  Args.Add(TEXT("autoplay"));

  Commandlet->createCommandPipeline(TEXT("test_game"), Args,
                                    MakeCommandletExecutor())
      .then([&bCompleted]() { bCompleted = true; })
      .catch_([&Error](std::string Message) {
        Error = UTF8_TO_TCHAR(Message.c_str());
      })
      .execute();

  TestTrue("test_game command completes through commandlet pipeline",
           bCompleted);
  TestTrue("test_game command does not reject on success", Error.IsEmpty());
  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTestGameCommandletPipelineFailureTest,
    "ForbocAI.Integration.TestGame.Commandlet.PipelineFailure",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
/**
 * User Story: As a developer, I need RunTest to fulfill its role in the module.
 */
bool FTestGameCommandletPipelineFailureTest::RunTest(
    const FString &Parameters) {
  (void)Parameters;
  AddExpectedError(TEXT("LOG_ERR_CRITICAL // BIT_ROT_DETECTED"),
                   EAutomationExpectedErrorFlags::Contains, 1);
  AddExpectedError(TEXT("synthetic failure"),
                   EAutomationExpectedErrorFlags::Contains, 1);

  UForbocAICommandlet *Commandlet = NewObject<UForbocAICommandlet>();
  bool bCompleted = false;
  FString Error;
  TArray<FString> Args;
  Args.Add(TEXT("autoplay"));

  Commandlet
      ->createCommandPipeline(TEXT("test_game"), Args,
                               MakeCommandletExecutor(TEXT("forbocai status")))
      .then([&bCompleted]() { bCompleted = true; })
      .catch_([&Error](std::string Message) {
        Error = UTF8_TO_TCHAR(Message.c_str());
      })
      .execute();

  TestFalse("test_game command rejects when the harness run fails", bCompleted);
  TestTrue("test_game command returns the run summary on failure",
           Error.Contains(TEXT("VOID_GAPS_DETECTED")));
  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTestGameRuntimeUrlResolutionTest,
    "ForbocAI.Integration.TestGame.RuntimeUrlResolution",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
/**
 * User Story: As a developer, I need RunTest to fulfill its role in the module.
 */
bool FTestGameRuntimeUrlResolutionTest::RunTest(const FString &Parameters) {
  (void)Parameters;

  const FString ExplicitRuntime = ResolveConfiguredRuntimeUrl(
      TEXT("https://runtime.example"), TEXT("https://api.forboc.ai"));
  TestEqual("ResolveConfiguredRuntimeUrl prefers FORBOC_RUNTIME_URL",
            ExplicitRuntime, FString(TEXT("https://runtime.example")));

  const FString ApiUrlConfigured = ResolveConfiguredRuntimeUrl(
      FString(), TEXT("https://api.forboc.ai"));
  TestEqual("ResolveConfiguredRuntimeUrl falls back to FORBOCAI_API_URL",
            ApiUrlConfigured, FString(TEXT("https://api.forboc.ai")));

  const FString MissingConfiguredRuntime =
      ResolveConfiguredRuntimeUrl(FString(), FString());
  TestTrue("ResolveConfiguredRuntimeUrl returns empty when unset",
           MissingConfiguredRuntime.IsEmpty());

  const FString LocalPreferred = ResolveVerificationRuntimeUrl(
      [](const FString &Url) { return Url.Contains(TEXT("localhost:8080")); });
  TestEqual("ResolveVerificationRuntimeUrl prefers localhost when available",
            LocalPreferred, FString(TEXT("http://localhost:8080")));

  const FString RemoteFallback = ResolveVerificationRuntimeUrl([](const FString &Url) {
    return Url.Contains(TEXT("api.forboc.ai"));
  });
  TestEqual("ResolveVerificationRuntimeUrl falls back to remote API when localhost fails",
            RemoteFallback, FString(TEXT("https://api.forboc.ai")));

  const FString MissingRuntime =
      ResolveVerificationRuntimeUrl([](const FString &) { return false; });
  TestTrue("ResolveVerificationRuntimeUrl returns empty when no runtime is reachable",
           MissingRuntime.IsEmpty());
  return true;
}
