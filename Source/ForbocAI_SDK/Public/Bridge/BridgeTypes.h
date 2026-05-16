#pragma once

// clang-format off
#include "CoreMinimal.h"
#include "NPC/NPCBaseTypes.h"
#include "BridgeTypes.generated.h"
// clang-format on

/**
 * Validation Result — Immutable data.
 * User Story: As bridge validation flows, I need one immutable result shape so
 * success and failure outcomes can move through reducers consistently.
 */
USTRUCT(BlueprintType)
struct FValidationResult {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Bridge")
  bool bValid;

  UPROPERTY(BlueprintReadOnly, Category = "Bridge")
  FString Reason;

  UPROPERTY(BlueprintReadOnly, Category = "Bridge")
  FAgentAction CorrectedAction;

  FValidationResult() : bValid(true) {}
};

/**
 * Validation Context
 * User Story: As bridge validation inputs, I need a dedicated context payload
 * so NPC, world, and constraint state are passed together.
 */
USTRUCT(BlueprintType)
struct FBridgeValidationContext {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Bridge")
  FString NpcStateJson;

  UPROPERTY(BlueprintReadOnly, Category = "Bridge")
  FString WorldStateJson;

  UPROPERTY(BlueprintReadOnly, Category = "Bridge")
  FString ConstraintsJson;

  FBridgeValidationContext() {}
};

/**
 * Bridge Rule
 * User Story: As bridge rule catalogs, I need a typed rule model so server and
 * local validation rules can be represented in one shared shape.
 */
USTRUCT(BlueprintType)
struct FBridgeRule {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Bridge")
  FString RuleName;

  UPROPERTY(BlueprintReadOnly, Category = "Bridge")
  FString RuleDescription;

  UPROPERTY(BlueprintReadOnly, Category = "Bridge")
  TArray<FString> RuleActionTypes;

  FBridgeRule() {}
};

/**
 * Directive Rule Set
 * User Story: As bridge preset management, I need a ruleset model so grouped
 * validation rules can be stored, selected, and exchanged consistently.
 */
USTRUCT(BlueprintType)
struct FDirectiveRuleSet {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Bridge")
  FString Id;

  UPROPERTY(BlueprintReadOnly, Category = "Bridge")
  FString RulesetId;

  UPROPERTY(BlueprintReadOnly, Category = "Bridge")
  TArray<FBridgeRule> RulesetRules;

  FDirectiveRuleSet() {}
};

USTRUCT(BlueprintType)
struct FBridgeValidateRequest {
  GENERATED_BODY()

  UPROPERTY(BlueprintReadOnly, Category = "Bridge")
  FAgentAction Action;

  UPROPERTY(BlueprintReadOnly, Category = "Bridge")
  FBridgeValidationContext Context;
};

namespace TypeFactory {

/**
 * Builds a successful bridge validation result.
 * User Story: As bridge validation, I need a factory for success results so
 * reducers and callers can construct valid outcomes consistently.
 */
inline FValidationResult Valid(FString Reason) {
  FValidationResult R;
  R.bValid = true;
  R.Reason = MoveTemp(Reason);
  return R;
}

/**
 * Builds a failed bridge validation result.
 * User Story: As bridge validation, I need a factory for invalid results so
 * rejection reasons are carried through one shared shape.
 */
inline FValidationResult Invalid(FString Reason) {
  FValidationResult R;
  R.bValid = false;
  R.Reason = MoveTemp(Reason);
  return R;
}

/**
 * Builds the bridge validation request payload.
 * User Story: As bridge API calls, I need a request factory so action and
 * context are packaged consistently before dispatch.
 */
inline FBridgeValidateRequest
BridgeValidateRequest(const FAgentAction &Action,
                      const FBridgeValidationContext &Context) {
  FBridgeValidateRequest Request;
  Request.Action = Action;
  Request.Context = Context;
  return Request;
}

} // namespace TypeFactory
