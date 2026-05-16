#pragma once

#include "Core/functional_core.hpp"
#include "CoreMinimal.h"
#include "Types.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include <functional>

/**
 * Bridge Module — Strict FP (Neuro-Symbolic Validation)
 * All types are pure data structs. NO constructors,
 * NO member functions. Construction via BridgeFactory
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
// namespace. Operations via BridgeOps namespace.
/**
 * Enhanced with functional core patterns for better
 * validation and composition.
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */

namespace BridgeTypes {
using func::AsyncResult;
using func::Curried;
using func::Either;
using func::Lazy;
using func::Pipeline;
using func::ValidationPipeline;

using func::make_left;
using func::make_right;

using ValidationResult = Either<FString, FValidationResult>;
} // namespace BridgeTypes

/**
 * Validation Context — Pure data.
 * User Story: As bridge validation, I need a context container so agent and
 * world state travel together into rule evaluation.
 */
struct FBridgeRuleContext {
  /**
   * Pointer to the Agent's state (optional).
   * User Story: As validation rules, I need optional agent state access so
   * rule checks can inspect the current NPC snapshot when present.
   */
  const FAgentState *AgentState;
  /**
   * Key-value map of the current world state.
   * User Story: As validation rules, I need world facts packaged with the
   * context so environmental constraints can influence rule outcomes.
   */
  const TMap<FString, FString> WorldState;
};

/**
 * Validation Rule — Pure data wrapping a function.
 * User Story: As bridge rule registration, I need a rule model so metadata and
 * executable validation logic can be stored together.
 * The Validator field holds a pure function: (Action, Context) -> Result
 */
struct FValidationRule {
  /**
   * Unique ID for the rule.
   * User Story: As rule catalogs, I need a stable identifier so rules can be
   * persisted, compared, and updated reliably.
   */
  FString Id;
  /**
   * Human-readable name of the rule.
   * User Story: As bridge tooling, I need a readable label so users can
   * understand which rule produced a validation outcome.
   */
  FString Name;
  /**
   * List of action types this rule applies to.
   * User Story: As rule dispatch, I need action-type filters so only relevant
   * rules run for a given action.
   */
  TArray<FString> ActionTypes;
  /**
   * The validation function to execute.
   * User Story: As rule execution, I need the callable attached to the rule so
   * validation can be evaluated from stored rule data.
   */
  std::function<FValidationResult(const FAgentAction &,
                                  const FBridgeRuleContext &)>
      Validator;
};

/**
 * Factory functions for Bridge types.
 * User Story: As bridge type construction, I need one namespace for factories
 * so rule and context values are built consistently.
 */
namespace BridgeFactory {

/**
 * Creates a validation context.
 * User Story: As bridge validation setup, I need a context factory so agent
 * and world state can be packaged consistently for rule evaluation.
 * @param State Optional pointer to agent state.
 * @param World Optional world state map.
 * @return A new FBridgeRuleContext.
 */
inline FBridgeRuleContext
CreateContext(const FAgentState *State = nullptr,
              TMap<FString, FString> World = {}) {
  return FBridgeRuleContext{State, MoveTemp(World)};
}

/**
 * Creates a validation rule.
 * User Story: As bridge rule registration, I need a factory so rule metadata
 * and validators can be assembled into one reusable structure.
 * @param Id Unique ID.
 * @param Name Human-readable name.
 * @param Types Action types this rule applies to.
 * @param InValidator The validation function.
 * @return A new FValidationRule.
 */
inline FValidationRule
CreateRule(FString Id, FString Name, TArray<FString> Types,
           std::function<FValidationResult(const FAgentAction &,
                                           const FBridgeRuleContext &)>
               InValidator) {
  FValidationRule R;
  R.Id = MoveTemp(Id);
  R.Name = MoveTemp(Name);
  R.ActionTypes = MoveTemp(Types);
  R.Validator = MoveTemp(InValidator);
  return R;
}

} // namespace BridgeFactory

/**
 * Bridge Operations — stateless free functions.
 * User Story: As bridge validation flows, I need one namespace for operations
 * so rule creation, validation, and registration stay discoverable.
 */
namespace BridgeOps {

/**
 * Creates the default set of validation rules.
 * User Story: As bridge validation bootstrap, I need baseline rules so common
 * action checks are available without custom configuration.
 * @return An array of default validation rules.
 */
FORBOCAI_SDK_API TArray<FValidationRule> CreateDefaultRules();

/**
 * Creates a preset of RPG-specific validation rules.
 * User Story: As RPG gameplay validation, I need genre-specific rules so
 * bridge checks can enforce movement and combat conventions.
 * @return An array of RPG validation rules.
 */
FORBOCAI_SDK_API TArray<FValidationRule> CreateRPGRules();

/**
 * Validates an action against a set of rules.
 * User Story: As bridge validation, I need one validation function so actions
 * can be checked against the active rule set before execution.
 * @param Action The action to validate.
 * @param Rules The rules to validate against.
 * @param Context The validation context.
 * @return The result of the validation.
 */
FORBOCAI_SDK_API FValidationResult
Validate(const FAgentAction &Action, const TArray<FValidationRule> &Rules,
         const FBridgeRuleContext &Context);

/**
 * Registers a rule with the API.
 * User Story: As bridge rule publishing, I need an async registration function
 * so custom validation rules can be persisted to the API.
 * @param Rule The rule to register.
 * @param ApiUrl The API endpoint.
 * @return An AsyncResult indicating success/failure.
 */
FORBOCAI_SDK_API BridgeTypes::AsyncResult<FDirectiveRuleSet>
RegisterRule(const FValidationRule &Rule, const FString &ApiUrl);

} // namespace BridgeOps
