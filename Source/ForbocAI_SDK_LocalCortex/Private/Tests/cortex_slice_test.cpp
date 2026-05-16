#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "Cortex/CortexSlice.h"
#include "Misc/AutomationTest.h"

using namespace rtk;
using namespace CortexSlice;

/**
 * Test: CortexInit Pending / Success / Failed lifecycle
 * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCortexSliceInitTest,
                                 "ForbocAI.Slices.Cortex.InitLifecycle",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FCortexSliceInitTest::RunTest(const FString &Parameters) {
  Slice<FCortexSliceState> CSlice = CreateCortexSlice();
  FCortexSliceState State;

  /**
   * Initial
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  TestTrue("Initial status Idle",
           State.Status == ECortexEngineStatus::Idle);

  /**
   * Pending
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  State = CSlice.Reducer(
      State, CortexSlice::Actions::CortexInitPending(TEXT("llama-3.2-1b")));
  TestTrue("Status Initializing",
           State.Status == ECortexEngineStatus::Initializing);
  TestEqual("Model set", State.EngineStatus.Model,
            FString(TEXT("llama-3.2-1b")));
  TestTrue("Error cleared", State.Error.IsEmpty());

  /**
   * Success
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  FCortexStatus CortexStatus;
  CortexStatus.Model = TEXT("llama-3.2-1b");
  CortexStatus.bReady = true;
  State = CSlice.Reducer(State, CortexSlice::Actions::CortexInitFulfilled(CortexStatus));
  TestTrue("Status Ready", State.Status == ECortexEngineStatus::Ready);
  TestTrue("Engine ready", State.EngineStatus.bReady);

  return true;
}

/**
 * Test: CortexInit Failed
 * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCortexSliceInitFailTest,
                                 "ForbocAI.Slices.Cortex.InitFailed",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FCortexSliceInitFailTest::RunTest(const FString &Parameters) {
  Slice<FCortexSliceState> CSlice = CreateCortexSlice();
  FCortexSliceState State;

  State = CSlice.Reducer(
      State, CortexSlice::Actions::CortexInitPending(TEXT("bad-model")));
  State = CSlice.Reducer(
      State, CortexSlice::Actions::CortexInitRejected(TEXT("Model not found")));

  TestTrue("Status Error", State.Status == ECortexEngineStatus::Error);
  TestEqual("Error message", State.Error, FString(TEXT("Model not found")));
  TestEqual("EngineStatus error", State.EngineStatus.Error,
            FString(TEXT("Model not found")));

  return true;
}

/**
 * Test: CortexComplete Pending / Success / Failed lifecycle
 * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCortexSliceCompleteTest,
                                 "ForbocAI.Slices.Cortex.CompleteLifecycle",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FCortexSliceCompleteTest::RunTest(const FString &Parameters) {
  Slice<FCortexSliceState> CSlice = CreateCortexSlice();
  FCortexSliceState State;

  /**
   * Init first
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  FCortexStatus CortexStatus;
  CortexStatus.Model = TEXT("llama-3.2-1b");
  CortexStatus.bReady = true;
  State = CSlice.Reducer(State, CortexSlice::Actions::CortexInitFulfilled(CortexStatus));

  /**
   * Complete Pending
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  State = CSlice.Reducer(
      State, CortexSlice::Actions::CortexCompletePending(TEXT("What is the meaning?")));
  TestEqual("LastPrompt set", State.LastPrompt,
            FString(TEXT("What is the meaning?")));
  TestTrue("Error cleared on pending", State.Error.IsEmpty());

  /**
   * Complete Success
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  FCortexResponse Response;
  Response.Text = TEXT("42");
  State = CSlice.Reducer(State, CortexSlice::Actions::CortexCompleteFulfilled(Response));
  TestEqual("LastResponseText set", State.LastResponseText,
            FString(TEXT("42")));

  return true;
}

/**
 * Test: CortexComplete Failed
 * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCortexSliceCompleteFailTest,
                                 "ForbocAI.Slices.Cortex.CompleteFailed",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FCortexSliceCompleteFailTest::RunTest(const FString &Parameters) {
  Slice<FCortexSliceState> CSlice = CreateCortexSlice();
  FCortexSliceState State;

  State = CSlice.Reducer(
      State, CortexSlice::Actions::CortexCompletePending(TEXT("Hello")));
  State = CSlice.Reducer(
      State, CortexSlice::Actions::CortexCompleteRejected(TEXT("OOM")));

  TestEqual("Error set", State.Error, FString(TEXT("OOM")));

  return true;
}
