#include "CLI/CliHandlers.h"
#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "RuntimeCommandlet.h"
#include "RuntimeStore.h"

/**
 * Test: setup_check passes commandlet validation and executes.
 * User Story: As CLI automation, I need setup commands accepted by the
 * commandlet validation layer so the supported entrypoint can run them.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSetupCommandletValidationTest,
    "ForbocAI.Integration.Setup.CommandletValidation",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FSetupCommandletValidationTest::RunTest(const FString &Parameters) {
  UForbocAICommandlet *Commandlet = NewObject<UForbocAICommandlet>();
  bool bCompleted = false;
  FString Error;

  Commandlet->createCommandPipeline(TEXT("setup_check"), TArray<FString>())
      .then([&bCompleted]() { bCompleted = true; })
      .catch_([&Error](std::string Message) {
        Error = UTF8_TO_TCHAR(Message.c_str());
      })
      .execute();

  TestTrue("setup_check completed through commandlet pipeline", bCompleted);
  TestTrue("setup_check did not fail validation", Error.IsEmpty());
  return true;
}

/**
 * Test: setup_runtime_check creates a local sqlite artifact in offline mode.
 * User Story: As runtime verification, I need a non-network smoke check so
 * native memory setup can be exercised in automated test runs.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSetupRuntimeSmokeDatabaseTest,
    "ForbocAI.Integration.Setup.RuntimeSmokeDatabase",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FSetupRuntimeSmokeDatabaseTest::RunTest(const FString &Parameters) {
  rtk::EnhancedStore<FStoreState> Store = createStore();
  const FString DatabasePath = FPaths::Combine(
      FPaths::ProjectSavedDir(), TEXT("ForbocAI"), TEXT("runtime-smoke-test"),
      FString::Printf(TEXT("runtime-smoke-%s.db"),
                      *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
  TArray<FString> Args;
  Args.Add(TEXT("--skip-cortex"));
  Args.Add(TEXT("--skip-vector"));
  Args.Add(TEXT("--skip-memory"));
  Args.Add(FString(TEXT("--database=")) + DatabasePath);

  const CLIOps::Handlers::HandlerResult Result =
      CLIOps::Handlers::HandleSetup(Store, TEXT("setup_runtime_check"), Args);

  TestTrue("setup_runtime_check is routed by setup handler", Result.hasValue);
  TestTrue("setup_runtime_check succeeded",
           Result.hasValue && Result.value.isSuccessful());
  TestTrue("runtime smoke database was created",
           IFileManager::Get().FileExists(*DatabasePath));

  IFileManager::Get().Delete(*DatabasePath, false, true, true);
  return true;
}
