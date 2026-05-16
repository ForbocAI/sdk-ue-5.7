#include "Bridge/BridgeModule.h"
#include "Bridge/BridgeSlice.h"
#include "CLI/CliOperations.h"
#include "NPC/NPCModule.h"
#include "Core/functional_core.hpp"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RuntimeStore.h"
#include "Serialization/JsonSerializer.h"
#include "Bridge/BridgeThunks.h"

/**
 * PURE VALIDATION FUNCTIONS
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */

namespace BridgeRules {

FValidationResult ValidateMovement(const FAgentAction &Action,
                                   const FBridgeRuleContext &Context) {
  (void)Context;
  return (Action.PayloadJson.Contains(TEXT("x")) &&
          Action.PayloadJson.Contains(TEXT("y")))
             ? TypeFactory::Valid(TEXT("Valid coordinates"))
             : TypeFactory::Invalid(TEXT("Missing x,y in payload"));
}

FValidationResult ValidateAttack(const FAgentAction &Action,
                                 const FBridgeRuleContext &Context) {
  (void)Context;
  return Action.Target.IsEmpty()
             ? TypeFactory::Invalid(TEXT("Missing target"))
             : TypeFactory::Valid(TEXT("Target specified"));
}

} // namespace BridgeRules

namespace BridgeHelpers {
BridgeTypes::ValidationPipeline<FAgentAction, FString>
bridgeValidationPipeline();
}

/**
 * LOCAL VALIDATION — Pure, used by thunk
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */

namespace BridgeHelpers {

FValidationResult RunLocalBridgeValidation(const FAgentAction &Action,
                                          const TArray<FValidationRule> &Rules,
                                          const FBridgeRuleContext &Context) {
  auto validationPipeline = bridgeValidationPipeline();
  auto result = func::runValidation(validationPipeline, Action);

  return result.isLeft
             ? TypeFactory::Invalid(result.left)
             : [&]() -> FValidationResult {
                 const FAgentAction validatedAction = result.right;
                 struct CheckRules {
                   static FValidationResult apply(
                       const TArray<FValidationRule> &R,
                       const FAgentAction &VA,
                       const FBridgeRuleContext &Ctx, int32 Idx) {
                     return Idx >= R.Num()
                         ? TypeFactory::Valid(TEXT("All rules passed"))
                         : R[Idx].ActionTypes.Contains(VA.Type)
                             ? [&]() -> FValidationResult {
                                 const FValidationResult RuleResult =
                                     R[Idx].Validator(VA, Ctx);
                                 return !RuleResult.bValid
                                            ? RuleResult
                                            : apply(R, VA, Ctx, Idx + 1);
                               }()
                             : apply(R, VA, Ctx, Idx + 1);
                   }
                 };
                 return CheckRules::apply(Rules, validatedAction, Context, 0);
               }();
}

} // namespace BridgeHelpers

/**
 * BRIDGE OPERATIONS — Free functions
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */

TArray<FValidationRule> BridgeOps::CreateDefaultRules() {
  /**
   * Game-Agnostic: No default rules are enforced by the protocol.
   * Games must explicitly register rules or use presets.
   * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
   */
  return TArray<FValidationRule>();
}

FValidationResult BridgeOps::Validate(const FAgentAction &Action,
                                      const TArray<FValidationRule> &Rules,
                                      const FBridgeRuleContext &Context) {
  auto Store = ConfigureStore();
  return Ops::WaitForResult(
      Store.dispatch(rtk::localValidateBridgeThunk(Action, Rules, Context)));
}

TArray<FValidationRule> BridgeOps::CreateRPGRules() {
  TArray<FValidationRule> Rules;

  Rules.Add(
      BridgeFactory::CreateRule(TEXT("move-val"), TEXT("Movement Validation"),
                                {TEXT("MOVE")}, BridgeRules::ValidateMovement));

  Rules.Add(
      BridgeFactory::CreateRule(TEXT("attack-val"), TEXT("Attack Validation"),
                                {TEXT("ATTACK")}, BridgeRules::ValidateAttack));

  return Rules;
}

BridgeTypes::AsyncResult<FDirectiveRuleSet>
BridgeOps::RegisterRule(const FValidationRule &Rule, const FString &ApiUrl) {
  /**
   * Dispatch through the store — no direct HTTP calls.
   * User Story: As a maintainer, I need this step note so I can follow the scenario progression and reason about the expected state changes.
   */
  static_cast<void>(ApiUrl);
  auto Store = ConfigureStore();
  FDirectiveRuleSet Ruleset;
  Ruleset.Id = Rule.Id;
  Ruleset.RulesetId = Rule.Name;
  FBridgeRule BridgeRule;
  BridgeRule.RuleName = Rule.Name;
  BridgeRule.RuleDescription = Rule.Name;
  BridgeRule.RuleActionTypes = Rule.ActionTypes;
  Ruleset.RulesetRules.Add(BridgeRule);
  return Store.dispatch(rtk::registerRulesetThunk(Ruleset));
}

/**
 * Functional helper implementations
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
namespace BridgeHelpers {
/**
 * Implementation of lazy HTTP request creation
 * User Story: As a maintainer, I need this note so the surrounding code intent stays clear during maintenance and debugging.
 */
BridgeTypes::Lazy<TSharedRef<IHttpRequest, ESPMode::ThreadSafe>>
createLazyHttpRequest(const FString &url) {
  return func::lazy([url]() -> TSharedRef<IHttpRequest, ESPMode::ThreadSafe> {
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
        FHttpModule::Get().CreateRequest();
    Request->SetURL(url);
    return Request;
  });
}

/**
 * Implementation of bridge validation pipeline
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
BridgeTypes::ValidationPipeline<FAgentAction, FString>
bridgeValidationPipeline() {
  return func::validationPipeline<FAgentAction, FString>() |
         [](const FAgentAction &action)
             -> BridgeTypes::Either<FString, FAgentAction> {
           return action.Type.IsEmpty()
                      ? BridgeTypes::make_left(
                            FString(TEXT("Action type cannot be empty")),
                            FAgentAction{})
                      : BridgeTypes::make_right(FString(), action);
         } |
         [](const FAgentAction &action)
             -> BridgeTypes::Either<FString, FAgentAction> {
           return action.Target.IsEmpty()
                      ? BridgeTypes::make_left(
                            FString(TEXT("Action target cannot be empty")),
                            FAgentAction{})
                      : BridgeTypes::make_right(FString(), action);
         };
}

/**
 * Implementation of bridge processing pipeline
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
BridgeTypes::Pipeline<FAgentAction>
bridgeProcessingPipeline(const FAgentAction &action) {
  return func::pipe(action);
}

/**
 * Implementation of curried bridge validation
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
typedef decltype(func::curry<2>(std::function<BridgeTypes::ValidationResult(
    FAgentAction, FBridgeRuleContext)>())) FCurriedBridgeValidation;

inline FCurriedBridgeValidation curriedBridgeValidation() {
  std::function<BridgeTypes::ValidationResult(FAgentAction, FBridgeRuleContext)>
      Creator =
          [](FAgentAction action,
             FBridgeRuleContext context) -> BridgeTypes::ValidationResult {
        try {
          FValidationResult result =
              BridgeOps::Validate(action, TArray<FValidationRule>(), context);
          return BridgeTypes::make_right(FString(), result);
        } catch (const std::exception &e) {
          return BridgeTypes::make_left(FString(e.what()), FValidationResult{});
        }
      };
  return func::curry<2>(Creator);
}

/**
 * Implementation of rule registration pipeline
 * User Story: As a maintainer, I need this section note so related declarations and logic stay easy to locate.
 */
BridgeTypes::Pipeline<FValidationRule>
ruleRegistrationPipeline(const FValidationRule &rule) {
  return func::pipe(rule);
}
} // namespace BridgeHelpers
