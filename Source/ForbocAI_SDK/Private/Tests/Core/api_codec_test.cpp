#include "API/APISlice.h"
#include "Core/JsonInterop.h"
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FApiCodecSoulVerifyAliasTest, "ForbocAI.Core.API.SoulVerifyAliases",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FApiCodecSoulVerifyAliasTest::RunTest(const FString &Parameters) {
  const FString Json = TEXT(
      R"({"verifyValid":true,"verifyReason":"signature_ok"})");

  FSoulVerifyResult Result;
  TestTrue("Soul verify aliases decode",
           APISlice::Detail::DecodeSoulVerifyResponse(Json, Result));
  TestTrue("Alias validity mapped", Result.bValid);
  TestEqual("Alias reason mapped", Result.Reason,
            FString(TEXT("signature_ok")));
  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FApiCodecBridgeRulesAliasTest, "ForbocAI.Core.API.BridgeRuleAliases",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FApiCodecBridgeRulesAliasTest::RunTest(const FString &Parameters) {
  const FString Json = TEXT(R"([
    {
      "brRuleId": "rule_no_self_harm",
      "ruleDescription": "No self harm",
      "affectedActions": ["deny", "warn"]
    }
  ])");

  TArray<FBridgeRule> Rules;
  TestTrue("Bridge rule aliases decode",
           APISlice::Detail::DecodeBridgeRulesResponse(Json, Rules));
  TestEqual("One rule decoded", Rules.Num(), 1);
  if (Rules.Num() == 1) {
    TestEqual("Rule name falls back to alias", Rules[0].RuleName,
              FString(TEXT("rule_no_self_harm")));
    TestEqual("Rule description preserved", Rules[0].RuleDescription,
              FString(TEXT("No self harm")));
    TestEqual("Affected actions preserved", Rules[0].RuleActionTypes.Num(), 2);
  }
  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FApiCodecRulesetAliasTest, "ForbocAI.Core.API.RulesetAliases",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FApiCodecRulesetAliasTest::RunTest(const FString &Parameters) {
  const FString Json = TEXT(R"({
    "rulesetId": "preset_rpg",
    "rulesetRules": [
      {
        "drRuleId": "rule_1",
        "ruleName": "Stay in character",
        "ruleAction": "speak",
        "ruleReason": "Keep voice consistent"
      }
    ]
  })");

  FDirectiveRuleSet Ruleset;
  TestTrue("Ruleset aliases decode",
           APISlice::Detail::DecodeDirectiveRuleSetResponse(Json, Ruleset));
  TestEqual("Ruleset id mapped to local id", Ruleset.Id,
            FString(TEXT("preset_rpg")));
  TestEqual("RulesetId preserved", Ruleset.RulesetId,
            FString(TEXT("preset_rpg")));
  TestEqual("One nested rule decoded", Ruleset.RulesetRules.Num(), 1);
  if (Ruleset.RulesetRules.Num() == 1) {
    TestEqual("Nested rule name mapped", Ruleset.RulesetRules[0].RuleName,
              FString(TEXT("Stay in character")));
    TestEqual("Nested rule has one action",
              Ruleset.RulesetRules[0].RuleActionTypes.Num(), 1);
    if (Ruleset.RulesetRules[0].RuleActionTypes.Num() == 1) {
      TestEqual("Nested rule action mapped",
                Ruleset.RulesetRules[0].RuleActionTypes[0],
                FString(TEXT("speak")));
    }
  }
  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FApiCodecNullableProtocolFieldsTest,
    "ForbocAI.Core.API.NullableProtocolFields",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FApiCodecNullableProtocolFieldsTest::RunTest(const FString &Parameters) {
  const FString Json = TEXT(R"({
    "instruction": {
      "type": "Finalize",
      "valid": true,
      "signature": null,
      "memoryStore": [],
      "stateTransform": {},
      "action": null,
      "dialogue": "hello there"
    },
    "tape": {
      "observation": "look around",
      "context": {},
      "npcState": {},
      "persona": null,
      "memories": [],
      "prompt": null,
      "constraints": {
        "model": "local",
        "useGPU": false,
        "maxTokens": 128,
        "temperature": 0.7,
        "topK": 40,
        "topP": 0.95
      },
      "generatedOutput": null,
      "rulesetId": null,
      "vectorQueried": true
    }
  })");

  FNPCProcessResponse Response;
  TestTrue("Nullable protocol fields decode",
           APISlice::Detail::DecodeNpcProcessResponse(Json, Response));
  TestTrue("Nullable finalize signature becomes empty",
           Response.Instruction.Signature.IsEmpty());
  TestTrue("Nullable tape persona becomes empty", Response.Tape.Persona.IsEmpty());
  TestTrue("Nullable tape prompt becomes empty", Response.Tape.Prompt.IsEmpty());
  TestTrue("Nullable generated output becomes empty",
           Response.Tape.GeneratedOutput.IsEmpty());
  TestTrue("Nullable ruleset id becomes empty",
           Response.Tape.RulesetId.IsEmpty());
  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FApiCodecBridgeValidationWrapperTest,
    "ForbocAI.Core.API.BridgeValidationWrapper",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FApiCodecBridgeValidationWrapperTest::RunTest(const FString &Parameters) {
  const FString Json = TEXT(
      R"({"brResult":{"valid":false,"reason":"blocked"}})");

  FValidationResult Result;
  TestTrue("Wrapped bridge result decodes",
           APISlice::Detail::DecodeValidationResult(Json, Result));
  TestFalse("Wrapped validity mapped", Result.bValid);
  TestEqual("Wrapped reason mapped", Result.Reason, FString(TEXT("blocked")));
  return true;
}

/**
 * Response normalization: gaType→type, actionReason→reason, actionTarget→target
 * (Haskell API may return aliased field names; JsonInterop::ActionFromObject normalizes)
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FApiCodecActionFromObjectGaTypeTest,
    "ForbocAI.Core.API.ActionFromObjectGaTypeAliases",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
bool FApiCodecActionFromObjectGaTypeTest::RunTest(const FString &Parameters) {
  TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
  Obj->SetStringField(TEXT("gaType"), TEXT("speak"));
  Obj->SetStringField(TEXT("actionTarget"), TEXT("player"));
  Obj->SetStringField(TEXT("actionReason"), TEXT("greeting"));

  FAgentAction Action = JsonInterop::ActionFromObject(Obj);

  TestEqual("gaType maps to Type", Action.Type, FString(TEXT("speak")));
  TestEqual("actionTarget maps to Target", Action.Target, FString(TEXT("player")));
  TestEqual("actionReason maps to Reason", Action.Reason,
            FString(TEXT("greeting")));
  return true;
}
