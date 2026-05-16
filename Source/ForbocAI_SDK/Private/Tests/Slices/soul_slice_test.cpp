#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Soul/SoulSlice.h"

using namespace rtk;
using namespace SoulSlice;

/**
 * Test: SoulExport Pending / Success / Failed lifecycle
 * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulSliceExportTest,
                                 "ForbocAI.Slices.Soul.ExportLifecycle",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FSoulSliceExportTest::RunTest(const FString &Parameters) {
  Slice<FSoulSliceState> SSlice = CreateSoulSlice();
  FSoulSliceState State;

  /**
   * Initial
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  TestEqual("Initial ExportStatus", State.ExportStatus,
            FString(TEXT("idle")));
  TestFalse("No last export", State.bHasLastExport);

  /**
   * Pending
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  State = SSlice.Reducer(State, SoulSlice::Actions::RemoteExportSoulPending());
  TestEqual("ExportStatus exporting", State.ExportStatus,
            FString(TEXT("exporting")));
  TestTrue("Error cleared", State.Error.IsEmpty());

  /**
   * Success
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  FSoulExportResult ExportResult;
  ExportResult.TxId = TEXT("arweave_tx_abc");
  State = SSlice.Reducer(State, SoulSlice::Actions::RemoteExportSoulSuccess(ExportResult));
  TestEqual("ExportStatus success", State.ExportStatus,
            FString(TEXT("success")));
  TestTrue("Has last export", State.bHasLastExport);
  TestEqual("TxId matches", State.LastExport.TxId,
            FString(TEXT("arweave_tx_abc")));

  return true;
}

/**
 * Test: SoulExport Failed
 * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulSliceExportFailTest,
                                 "ForbocAI.Slices.Soul.ExportFailed",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FSoulSliceExportFailTest::RunTest(const FString &Parameters) {
  Slice<FSoulSliceState> SSlice = CreateSoulSlice();
  FSoulSliceState State;

  State = SSlice.Reducer(State, SoulSlice::Actions::RemoteExportSoulPending());
  State = SSlice.Reducer(
      State, SoulSlice::Actions::RemoteExportSoulFailed(TEXT("Arweave down")));

  TestEqual("ExportStatus failed", State.ExportStatus,
            FString(TEXT("failed")));
  TestEqual("Error set", State.Error, FString(TEXT("Arweave down")));

  return true;
}

/**
 * Test: SoulImport Pending / Success / Failed lifecycle
 * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulSliceImportTest,
                                 "ForbocAI.Slices.Soul.ImportLifecycle",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FSoulSliceImportTest::RunTest(const FString &Parameters) {
  Slice<FSoulSliceState> SSlice = CreateSoulSlice();
  FSoulSliceState State;

  /**
   * Initial
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  TestEqual("Initial ImportStatus", State.ImportStatus,
            FString(TEXT("idle")));

  /**
   * Pending
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  State = SSlice.Reducer(State, SoulSlice::Actions::ImportSoulPending());
  TestEqual("ImportStatus importing", State.ImportStatus,
            FString(TEXT("importing")));

  /**
   * Success
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  FSoul Soul;
  Soul.Id = TEXT("npc_soul");
  Soul.Persona = TEXT("Wise sage");
  State = SSlice.Reducer(State, SoulSlice::Actions::ImportSoulSuccess(Soul));
  TestEqual("ImportStatus success", State.ImportStatus,
            FString(TEXT("success")));
  TestTrue("Has last import", State.bHasLastImport);
  TestEqual("Imported soul id", State.LastImport.Id,
            FString(TEXT("npc_soul")));

  return true;
}

/**
 * Test: SoulImport Failed
 * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulSliceImportFailTest,
                                 "ForbocAI.Slices.Soul.ImportFailed",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FSoulSliceImportFailTest::RunTest(const FString &Parameters) {
  Slice<FSoulSliceState> SSlice = CreateSoulSlice();
  FSoulSliceState State;

  State = SSlice.Reducer(State, SoulSlice::Actions::ImportSoulPending());
  State = SSlice.Reducer(State,
                         SoulSlice::Actions::ImportSoulFailed(TEXT("Invalid txId")));

  TestEqual("ImportStatus failed", State.ImportStatus,
            FString(TEXT("failed")));
  TestEqual("Error set", State.Error, FString(TEXT("Invalid txId")));

  return true;
}

/**
 * Test: SetSoulList and ClearSoulState
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoulSliceListAndClearTest,
                                 "ForbocAI.Slices.Soul.ListAndClear",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FSoulSliceListAndClearTest::RunTest(const FString &Parameters) {
  Slice<FSoulSliceState> SSlice = CreateSoulSlice();
  FSoulSliceState State;

  TArray<FSoulListItem> SoulList;
  FSoulListItem Item;
  Item.TxId = TEXT("tx_list_1");
  SoulList.Add(Item);

  State = SSlice.Reducer(State, SoulSlice::Actions::SetSoulList(SoulList));
  TestEqual("AvailableSouls count", State.AvailableSouls.Num(), 1);
  TestEqual("Soul txId", State.AvailableSouls[0].TxId,
            FString(TEXT("tx_list_1")));

  /**
   * Clear resets everything
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  State = SSlice.Reducer(State, SoulSlice::Actions::ClearSoulState());
  TestEqual("ExportStatus reset", State.ExportStatus, FString(TEXT("idle")));
  TestEqual("ImportStatus reset", State.ImportStatus, FString(TEXT("idle")));
  TestFalse("No last export", State.bHasLastExport);
  TestFalse("No last import", State.bHasLastImport);
  TestEqual("Souls list cleared", State.AvailableSouls.Num(), 0);

  return true;
}
