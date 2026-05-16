#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Soul/SoulSlice.h"

using namespace rtk;
using namespace SoulSlice;
namespace SoulActions = SoulSlice::Actions;

/**
 * Test: Concurrent export and import — export pending then import pending
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulConcurrentExportImportTest,
                                 "ForbocAI.Slices.Soul.ConcurrentExportImport",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FSoulConcurrentExportImportTest::RunTest(const FString &Parameters) {
  Slice<FSoulSliceState> SSlice = CreateSoulSlice();
  FSoulSliceState State;

  /**
   * Start export
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  State = SSlice.Reducer(State, SoulActions::RemoteExportSoulPending());
  TestEqual("ExportStatus exporting", State.ExportStatus,
            FString(TEXT("exporting")));
  TestEqual("ImportStatus idle", State.ImportStatus,
            FString(TEXT("idle")));

  /**
   * Start import while export is still pending
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  State = SSlice.Reducer(State, SoulActions::ImportSoulPending());
  TestEqual("ExportStatus still exporting", State.ExportStatus,
            FString(TEXT("exporting")));
  TestEqual("ImportStatus importing", State.ImportStatus,
            FString(TEXT("importing")));

  /**
   * Complete export
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  FSoulExportResult ExportResult;
  ExportResult.TxId = TEXT("export_tx_concurrent");
  State = SSlice.Reducer(State, SoulActions::RemoteExportSoulSuccess(ExportResult));
  TestEqual("ExportStatus success", State.ExportStatus,
            FString(TEXT("success")));
  TestEqual("ImportStatus still importing", State.ImportStatus,
            FString(TEXT("importing")));
  TestTrue("Has export result", State.bHasLastExport);
  TestEqual("Export TxId", State.LastExport.TxId,
            FString(TEXT("export_tx_concurrent")));

  /**
   * Complete import
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  FSoul ImportedSoul;
  ImportedSoul.Id = TEXT("import_soul_concurrent");
  ImportedSoul.Persona = TEXT("Concurrent Test Soul");
  State = SSlice.Reducer(State, SoulActions::ImportSoulSuccess(ImportedSoul));
  TestEqual("ExportStatus still success", State.ExportStatus,
            FString(TEXT("success")));
  TestEqual("ImportStatus success", State.ImportStatus,
            FString(TEXT("success")));
  TestTrue("Has import result", State.bHasLastImport);
  TestEqual("Import id", State.LastImport.Id,
            FString(TEXT("import_soul_concurrent")));

  return true;
}

/**
 * Test: Export fails while import succeeds — independent error tracking
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulExportFailImportSucceedTest,
                                 "ForbocAI.Slices.Soul.ExportFailImportSucceed",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FSoulExportFailImportSucceedTest::RunTest(const FString &Parameters) {
  Slice<FSoulSliceState> SSlice = CreateSoulSlice();
  FSoulSliceState State;

  /**
   * Export fails
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  State = SSlice.Reducer(State, SoulActions::RemoteExportSoulPending());
  State = SSlice.Reducer(State,
                         SoulActions::RemoteExportSoulFailed(TEXT("Arweave down")));
  TestEqual("ExportStatus failed", State.ExportStatus,
            FString(TEXT("failed")));
  TestEqual("Error set", State.Error, FString(TEXT("Arweave down")));

  /**
   * Import starts — should clear the shared error
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  State = SSlice.Reducer(State, SoulActions::ImportSoulPending());
  TestTrue("Error cleared by import pending", State.Error.IsEmpty());
  TestEqual("ExportStatus still failed", State.ExportStatus,
            FString(TEXT("failed")));

  /**
   * Import succeeds
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  FSoul Soul;
  Soul.Id = TEXT("import_after_fail");
  State = SSlice.Reducer(State, SoulActions::ImportSoulSuccess(Soul));
  TestEqual("ImportStatus success", State.ImportStatus,
            FString(TEXT("success")));
  TestEqual("ExportStatus still failed", State.ExportStatus,
            FString(TEXT("failed")));

  return true;
}

/**
 * Test: Import fails while export succeeds — independent error tracking
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulImportFailExportSucceedTest,
                                 "ForbocAI.Slices.Soul.ImportFailExportSucceed",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FSoulImportFailExportSucceedTest::RunTest(const FString &Parameters) {
  Slice<FSoulSliceState> SSlice = CreateSoulSlice();
  FSoulSliceState State;

  /**
   * Import fails
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  State = SSlice.Reducer(State, SoulActions::ImportSoulPending());
  State = SSlice.Reducer(State,
                         SoulActions::ImportSoulFailed(TEXT("Invalid TxId")));
  TestEqual("ImportStatus failed", State.ImportStatus,
            FString(TEXT("failed")));

  /**
   * Export starts — should clear the shared error
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  State = SSlice.Reducer(State, SoulActions::RemoteExportSoulPending());
  TestTrue("Error cleared by export pending", State.Error.IsEmpty());

  /**
   * Export succeeds
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  FSoulExportResult ExportResult;
  ExportResult.TxId = TEXT("export_after_import_fail");
  State = SSlice.Reducer(State, SoulActions::RemoteExportSoulSuccess(ExportResult));
  TestEqual("ExportStatus success", State.ExportStatus,
            FString(TEXT("success")));
  TestEqual("ImportStatus still failed", State.ImportStatus,
            FString(TEXT("failed")));

  return true;
}

/**
 * Test: Double export — second export overwrites first result
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulDoubleExportTest,
                                 "ForbocAI.Slices.Soul.DoubleExport",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FSoulDoubleExportTest::RunTest(const FString &Parameters) {
  Slice<FSoulSliceState> SSlice = CreateSoulSlice();
  FSoulSliceState State;

  /**
   * First export
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  State = SSlice.Reducer(State, SoulActions::RemoteExportSoulPending());
  FSoulExportResult Result1;
  Result1.TxId = TEXT("tx_first");
  State = SSlice.Reducer(State, SoulActions::RemoteExportSoulSuccess(Result1));
  TestEqual("First export TxId", State.LastExport.TxId,
            FString(TEXT("tx_first")));

  /**
   * Second export
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  State = SSlice.Reducer(State, SoulActions::RemoteExportSoulPending());
  TestEqual("Status back to exporting", State.ExportStatus,
            FString(TEXT("exporting")));
  /**
   * bHasLastExport should still be true from first
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  TestTrue("Still has export from first", State.bHasLastExport);

  FSoulExportResult Result2;
  Result2.TxId = TEXT("tx_second");
  State = SSlice.Reducer(State, SoulActions::RemoteExportSoulSuccess(Result2));
  TestEqual("Second export TxId overwrites", State.LastExport.TxId,
            FString(TEXT("tx_second")));

  return true;
}

/**
 * Test: Double import — second import overwrites first result
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulDoubleImportTest,
                                 "ForbocAI.Slices.Soul.DoubleImport",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FSoulDoubleImportTest::RunTest(const FString &Parameters) {
  Slice<FSoulSliceState> SSlice = CreateSoulSlice();
  FSoulSliceState State;

  /**
   * First import
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  State = SSlice.Reducer(State, SoulActions::ImportSoulPending());
  FSoul Soul1;
  Soul1.Id = TEXT("soul_first");
  Soul1.Persona = TEXT("First Persona");
  State = SSlice.Reducer(State, SoulActions::ImportSoulSuccess(Soul1));
  TestEqual("First import id", State.LastImport.Id,
            FString(TEXT("soul_first")));

  /**
   * Second import
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  State = SSlice.Reducer(State, SoulActions::ImportSoulPending());
  FSoul Soul2;
  Soul2.Id = TEXT("soul_second");
  Soul2.Persona = TEXT("Second Persona");
  State = SSlice.Reducer(State, SoulActions::ImportSoulSuccess(Soul2));
  TestEqual("Second import overwrites", State.LastImport.Id,
            FString(TEXT("soul_second")));
  TestEqual("Persona updated", State.LastImport.Persona,
            FString(TEXT("Second Persona")));

  return true;
}
