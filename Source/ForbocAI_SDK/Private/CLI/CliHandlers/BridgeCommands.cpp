#include "CLI/CliHandlers.h"
#include "CLI/CliOperations.h"
#include "RuntimeStore.h"

namespace CLIOps {
namespace Handlers {

HandlerResult HandleBridge(rtk::EnhancedStore<FStoreState> &Store,
                          const FString &CommandKey,
                          const TArray<FString> &Args) {
  using func::just;
  using func::nothing;

  return CommandKey == TEXT("bridge_validate")
             ? (Args.Num() < 1
                    ? just(Result::Failure(
                          "Usage: bridge_validate <actionJson>"))
                    : [&]() -> HandlerResult {
                        FAgentAction Action;
                        Action.PayloadJson = Args[0];
                        FBridgeValidationContext Context;
                        FValidationResult VResult =
                            Ops::ValidateBridge(Store, Action, Context);
                        UE_LOG(LogTemp, Display,
                               TEXT("Validation: %s"),
                               VResult.bValid ? TEXT("PASS")
                                              : TEXT("FAIL"));
                        return just(Result::Success(
                            "Bridge validation done"));
                      }())
         : CommandKey == TEXT("bridge_rules")
             ? [&]() -> HandlerResult {
                 TArray<FBridgeRule> Rules = Ops::BridgeRules(Store);
                 UE_LOG(LogTemp, Display,
                        TEXT("Found %d bridge rules"), Rules.Num());
                 return just(
                     Result::Success("Bridge rules listed"));
               }()
         : CommandKey == TEXT("bridge_preset")
             ? (Args.Num() < 1
                    ? just(Result::Failure(
                          "Usage: bridge_preset <presetName>"))
                    : [&]() -> HandlerResult {
                        FDirectiveRuleSet Preset =
                            Ops::BridgePreset(Store, Args[0]);
                        UE_LOG(LogTemp, Display,
                               TEXT("Loaded preset: %s"), *Preset.Id);
                        return just(Result::Success(
                            "Bridge preset loaded"));
                      }())
         : CommandKey == TEXT("rules_list")
             ? [&]() -> HandlerResult {
                 TArray<FDirectiveRuleSet> Rulesets =
                     Ops::RulesList(Store);
                 UE_LOG(LogTemp, Display,
                        TEXT("Found %d rulesets"), Rulesets.Num());
                 return just(
                     Result::Success("Rulesets listed"));
               }()
         : CommandKey == TEXT("rules_presets")
             ? [&]() -> HandlerResult {
                 TArray<FString> Presets =
                     Ops::RulesPresets(Store);
                 UE_LOG(LogTemp, Display,
                        TEXT("Found %d presets"), Presets.Num());
                 return just(
                     Result::Success("Rule presets listed"));
               }()
         : CommandKey == TEXT("rules_register")
             ? (Args.Num() < 1
                    ? just(Result::Failure(
                          "Usage: rules_register <rulesetJson>"))
                    : [&]() -> HandlerResult {
                        FDirectiveRuleSet Ruleset;
                        Ruleset.Id = Args[0];
                        FDirectiveRuleSet Registered =
                            Ops::RulesRegister(Store, Ruleset);
                        UE_LOG(LogTemp, Display,
                               TEXT("Registered ruleset: %s"),
                               *Registered.Id);
                        return just(Result::Success(
                            "Ruleset registered"));
                      }())
         : CommandKey == TEXT("rules_delete")
             ? (Args.Num() < 1
                    ? just(Result::Failure(
                          "Usage: rules_delete <rulesetId>"))
                    : (Ops::RulesDelete(Store, Args[0]),
                       just(Result::Success("Ruleset deleted"))))
             : nothing<Result>();
}

} // namespace Handlers
} // namespace CLIOps
