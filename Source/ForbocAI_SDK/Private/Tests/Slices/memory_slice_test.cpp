#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "Memory/MemorySlice.h"
#include "Misc/AutomationTest.h"

using namespace rtk;
using namespace MemorySlice;

/**
 * Test: MemoryStoreStart / Success / Failed lifecycle
 * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMemorySliceStoreTest,
                                 "ForbocAI.Slices.Memory.StoreLifecycle",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FMemorySliceStoreTest::RunTest(const FString &Parameters) {
  Slice<FMemorySliceState> MemSlice = CreateMemorySlice();
  FMemorySliceState State;

  /**
   * Initial state
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  TestEqual("Initial StorageStatus", State.StorageStatus,
            FString(TEXT("idle")));

  /**
   * StoreStart
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  State = MemSlice.Reducer(State, MemorySlice::Actions::MemoryStoreStart());
  TestEqual("StorageStatus storing", State.StorageStatus,
            FString(TEXT("storing")));

  /**
   * StoreSuccess
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  FMemoryItem Item;
  Item.Id = TEXT("mem_1");
  Item.Text = TEXT("The dragon appeared");
  Item.Importance = 0.9f;
  State = MemSlice.Reducer(State, MemorySlice::Actions::MemoryStoreSuccess(Item));
  TestEqual("StorageStatus idle after success", State.StorageStatus,
            FString(TEXT("idle")));

  func::Maybe<FMemoryItem> Found = SelectMemoryById(State, TEXT("mem_1"));
  TestTrue("Memory item stored", Found.hasValue);
  if (Found.hasValue) {
    TestEqual("Memory text", Found.value.Text,
              FString(TEXT("The dragon appeared")));
  }

  return true;
}

/**
 * Test: MemoryStoreFailed sets error
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMemorySliceStoreFailTest,
                                 "ForbocAI.Slices.Memory.StoreFailed",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FMemorySliceStoreFailTest::RunTest(const FString &Parameters) {
  Slice<FMemorySliceState> MemSlice = CreateMemorySlice();
  FMemorySliceState State;

  State = MemSlice.Reducer(State, MemorySlice::Actions::MemoryStoreStart());
  State = MemSlice.Reducer(State,
                           MemorySlice::Actions::MemoryStoreFailed(TEXT("Network error")));

  TestEqual("StorageStatus error", State.StorageStatus,
            FString(TEXT("error")));
  TestEqual("Error message set", State.Error, FString(TEXT("Network error")));

  return true;
}

/**
 * Test: MemoryRecallStart / Success / Failed lifecycle
 * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMemorySliceRecallTest,
                                 "ForbocAI.Slices.Memory.RecallLifecycle",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FMemorySliceRecallTest::RunTest(const FString &Parameters) {
  Slice<FMemorySliceState> MemSlice = CreateMemorySlice();
  FMemorySliceState State;

  /**
   * RecallStart
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  State = MemSlice.Reducer(State, MemorySlice::Actions::MemoryRecallStart());
  TestEqual("RecallStatus recalling", State.RecallStatus,
            FString(TEXT("recalling")));

  /**
   * RecallSuccess with multiple items
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  TArray<FMemoryItem> Items;
  FMemoryItem M1;
  M1.Id = TEXT("r1");
  M1.Text = TEXT("First memory");
  M1.Importance = 0.5f;
  Items.Add(M1);

  FMemoryItem M2;
  M2.Id = TEXT("r2");
  M2.Text = TEXT("Second memory");
  M2.Importance = 0.7f;
  Items.Add(M2);

  State = MemSlice.Reducer(State, MemorySlice::Actions::MemoryRecallSuccess(Items));
  TestEqual("RecallStatus idle after success", State.RecallStatus,
            FString(TEXT("idle")));
  TestEqual("LastRecalledIds count", State.LastRecalledIds.Num(), 2);
  TestEqual("All memories count", SelectAllMemories(State).Num(), 2);

  TArray<FMemoryItem> Recalled = SelectLastRecalledMemories(State);
  TestEqual("LastRecalled count", Recalled.Num(), 2);

  return true;
}

/**
 * Test: MemoryRecallFailed
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMemorySliceRecallFailTest,
                                 "ForbocAI.Slices.Memory.RecallFailed",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FMemorySliceRecallFailTest::RunTest(const FString &Parameters) {
  Slice<FMemorySliceState> MemSlice = CreateMemorySlice();
  FMemorySliceState State;

  State = MemSlice.Reducer(State, MemorySlice::Actions::MemoryRecallStart());
  State = MemSlice.Reducer(State,
                           MemorySlice::Actions::MemoryRecallFailed(TEXT("Timeout")));

  TestEqual("RecallStatus error", State.RecallStatus, FString(TEXT("error")));
  TestEqual("Error set", State.Error, FString(TEXT("Timeout")));

  return true;
}

/**
 * Test: MemoryClear resets to initial state
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMemorySliceClearTest,
                                 "ForbocAI.Slices.Memory.Clear",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FMemorySliceClearTest::RunTest(const FString &Parameters) {
  Slice<FMemorySliceState> MemSlice = CreateMemorySlice();
  FMemorySliceState State;

  FMemoryItem Item;
  Item.Id = TEXT("clr_1");
  Item.Text = TEXT("Will be cleared");
  Item.Importance = 0.5f;
  State = MemSlice.Reducer(State, MemorySlice::Actions::MemoryStoreSuccess(Item));
  TestEqual("One memory before clear", SelectAllMemories(State).Num(), 1);

  State = MemSlice.Reducer(State, MemorySlice::Actions::MemoryClear());
  TestEqual("No memories after clear", SelectAllMemories(State).Num(), 0);
  TestEqual("StorageStatus reset", State.StorageStatus,
            FString(TEXT("idle")));
  TestEqual("RecallStatus reset", State.RecallStatus, FString(TEXT("idle")));

  return true;
}
