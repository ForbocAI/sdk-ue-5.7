#include "Bridge/BridgeSlice.h"
#include "Core/rtk.hpp"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

using namespace rtk;
using namespace BridgeSlice;

/**
 * Test: BridgeValidationPending / Success / Failure lifecycle
 * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FBridgeSliceValidationLifecycleTest,
    "ForbocAI.Slices.Bridge.ValidationLifecycle",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FBridgeSliceValidationLifecycleTest::RunTest(const FString &Parameters) {
  Slice<FBridgeSliceState> BSlice = CreateBridgeSlice();
  FBridgeSliceState State;

  /**
   * Initial
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  TestEqual("Initial status idle", State.Status, FString(TEXT("idle")));
  TestFalse("No validation yet", State.bHasLastValidation);

  /**
   * Pending
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  State = BSlice.Reducer(State, BridgeSlice::Actions::BridgeValidationPending());
  TestEqual("Status validating", State.Status, FString(TEXT("validating")));
  TestTrue("Error cleared", State.Error.IsEmpty());

  /**
   * Success
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  FValidationResult Result;
  Result.bValid = true;
  State = BSlice.Reducer(State, BridgeSlice::Actions::BridgeValidationSuccess(Result));
  TestEqual("Status idle after success", State.Status, FString(TEXT("idle")));
  TestTrue("Has last validation", State.bHasLastValidation);
  TestTrue("Validation is valid", State.LastValidation.bValid);

  return true;
}

/**
 * Test: BridgeValidationFailure sets error state
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBridgeSliceValidationFailTest,
                                 "ForbocAI.Slices.Bridge.ValidationFailure",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FBridgeSliceValidationFailTest::RunTest(const FString &Parameters) {
  Slice<FBridgeSliceState> BSlice = CreateBridgeSlice();
  FBridgeSliceState State;

  State = BSlice.Reducer(State, BridgeSlice::Actions::BridgeValidationPending());
  State = BSlice.Reducer(
      State, BridgeSlice::Actions::BridgeValidationFailure(TEXT("Rule violation")));

  TestEqual("Status error", State.Status, FString(TEXT("error")));
  TestEqual("Error message", State.Error, FString(TEXT("Rule violation")));
  TestTrue("Has last validation", State.bHasLastValidation);

  return true;
}

/**
 * Test: AddActivePreset and SetActivePresets
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBridgeSlicePresetsTest,
                                 "ForbocAI.Slices.Bridge.Presets",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FBridgeSlicePresetsTest::RunTest(const FString &Parameters) {
  Slice<FBridgeSliceState> BSlice = CreateBridgeSlice();
  FBridgeSliceState State;

  /**
   * Add preset
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  FDirectiveRuleSet FirstPreset;
  FirstPreset.Id = TEXT("rpg_default");
  State = BSlice.Reducer(State, BridgeSlice::Actions::AddActivePreset(FirstPreset));
  TestEqual("One active preset", State.ActivePresets.Num(), 1);
  TestEqual("Preset is rpg_default", State.ActivePresets[0].Id,
            FString(TEXT("rpg_default")));

  /**
   * Duplicate add should not duplicate
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  State = BSlice.Reducer(State, BridgeSlice::Actions::AddActivePreset(FirstPreset));
  TestEqual("Still one preset (no dupe)", State.ActivePresets.Num(), 1);

  /**
   * Add another
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  FDirectiveRuleSet SecondPreset;
  SecondPreset.Id = TEXT("combat");
  State = BSlice.Reducer(State, BridgeSlice::Actions::AddActivePreset(SecondPreset));
  TestEqual("Two presets", State.ActivePresets.Num(), 2);

  /**
   * SetActivePresets replaces all
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  TArray<FDirectiveRuleSet> NewPresets;
  FDirectiveRuleSet StealthPreset;
  StealthPreset.Id = TEXT("stealth");
  NewPresets.Add(StealthPreset);
  State = BSlice.Reducer(State, BridgeSlice::Actions::SetActivePresets(NewPresets));
  TestEqual("Replaced to one", State.ActivePresets.Num(), 1);
  TestEqual("Preset is stealth", State.ActivePresets[0].Id,
            FString(TEXT("stealth")));

  return true;
}

/**
 * Test: SetAvailableRulesets
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBridgeSliceRulesetsTest,
                                 "ForbocAI.Slices.Bridge.Rulesets",
                                 EAutomationTestFlags_ApplicationContextMask |
                                     EAutomationTestFlags::EngineFilter)
bool FBridgeSliceRulesetsTest::RunTest(const FString &Parameters) {
  Slice<FBridgeSliceState> BSlice = CreateBridgeSlice();
  FBridgeSliceState State;

  TArray<FDirectiveRuleSet> Rulesets;
  FDirectiveRuleSet Rs;
  Rs.Id = TEXT("rs_1");
  Rs.RulesetId = TEXT("Default Rules");
  Rulesets.Add(Rs);

  State = BSlice.Reducer(State, BridgeSlice::Actions::SetAvailableRulesets(Rulesets));
  TestEqual("One ruleset available", State.AvailableRulesets.Num(), 1);
  TestEqual("Ruleset id", State.AvailableRulesets[0].Id,
            FString(TEXT("rs_1")));

  FDirectiveRuleSet ActivePreset;
  ActivePreset.Id = TEXT("preset_active");
  State = BSlice.Reducer(State, BridgeSlice::Actions::AddActivePreset(ActivePreset));

  TArray<FString> PresetIds;
  PresetIds.Add(TEXT("preset_active"));
  PresetIds.Add(TEXT("preset_alt"));
  State =
      BSlice.Reducer(State, BridgeSlice::Actions::SetAvailablePresetIds(PresetIds));

  FValidationResult Result;
  Result.bValid = false;
  Result.Reason = TEXT("unsafe");
  State = BSlice.Reducer(State, BridgeSlice::Actions::BridgeValidationSuccess(Result));

  /**
   * Clear only resets validation fields.
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  State = BSlice.Reducer(State, BridgeSlice::Actions::ClearBridgeValidation());
  TestEqual("Status reset", State.Status, FString(TEXT("idle")));
  TestFalse("No last validation", State.bHasLastValidation);
  TestTrue("Error cleared", State.Error.IsEmpty());
  TestEqual("Active presets preserved", State.ActivePresets.Num(), 1);
  TestEqual("Available rulesets preserved", State.AvailableRulesets.Num(), 1);
  TestEqual("Preset ids preserved", State.AvailablePresetIds.Num(), 2);

  return true;
}
