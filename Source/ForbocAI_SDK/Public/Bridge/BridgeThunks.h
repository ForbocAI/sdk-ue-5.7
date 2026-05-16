#pragma once

#include "Bridge/BridgeModule.h"
#include "Bridge/BridgeSlice.h"
#include "Core/ThunkDetail.h"
#include "Errors.h"
#include "RuntimeConfig.h"

namespace BridgeHelpers {
/**
 * Runs the pure local bridge validation path.
 * User Story: As offline bridge validation, I need a local validator so rule
 * checks can run without depending on the remote API.
 */
FValidationResult RunLocalBridgeValidation(const FAgentAction &Action,
                                          const TArray<FValidationRule> &Rules,
                                          const FBridgeRuleContext &Context);
} // namespace BridgeHelpers

namespace rtk {

/**
 * Builds the thunk that performs local rule-based bridge validation.
 * User Story: As bridge validation flows, I need a local thunk so actions can
 * be validated and reflected in slice state without remote calls.
 */
inline ThunkAction<FValidationResult, FStoreState>
localValidateBridgeThunk(const FAgentAction &Action,
                         const TArray<FValidationRule> &Rules,
                         const FBridgeRuleContext &Context) {
  return [Action, Rules, Context](
             std::function<AnyAction(const AnyAction &)> Dispatch,
             std::function<FStoreState()> GetState)
             -> func::AsyncResult<FValidationResult> {
    Dispatch(BridgeSlice::Actions::BridgeValidationPending());

    FValidationResult Result =
        BridgeHelpers::RunLocalBridgeValidation(Action, Rules, Context);

    Result.bValid
        ? (Dispatch(BridgeSlice::Actions::BridgeValidationSuccess(Result)),
           void())
        : (Dispatch(
               BridgeSlice::Actions::BridgeValidationFailure(Result.Reason)),
           void());
    return detail::ResolveAsync(Result);
  };
}

/**
 * Builds the thunk that validates a bridge action through the API.
 * User Story: As remote bridge validation, I need a thunk that checks API
 * prerequisites and stores the resulting validation outcome.
 */
inline ThunkAction<FValidationResult, FStoreState>
validateBridgeThunk(const FAgentAction &Action,
                    const FBridgeValidationContext &Context,
                    const FString &NpcId = TEXT("")) {
  return [Action, Context, NpcId](
             std::function<AnyAction(const AnyAction &)> Dispatch,
             std::function<FStoreState()> GetState)
             -> func::AsyncResult<FValidationResult> {
    const auto ApiKeyError = Errors::requireApiKeyGuidance(
        SDKConfig::GetApiUrl(), SDKConfig::GetApiKey());
    return ApiKeyError.hasValue
        ? detail::RejectAsync<FValidationResult>(ApiKeyError.value)
        : (Dispatch(BridgeSlice::Actions::BridgeValidationPending()),
           func::AsyncChain::then<FValidationResult, FValidationResult>(
               APISlice::Endpoints::postBridgeValidate(
                   NpcId, TypeFactory::BridgeValidateRequest(Action, Context))(
                   Dispatch, GetState),
               [Dispatch](const FValidationResult &Result) {
                 Dispatch(
                     BridgeSlice::Actions::BridgeValidationSuccess(Result));
                 return detail::ResolveAsync(Result);
               })
               .catch_([Dispatch](std::string Error) {
                 Dispatch(BridgeSlice::Actions::BridgeValidationFailure(
                     FString(UTF8_TO_TCHAR(Error.c_str()))));
               }));
  };
}

/**
 * Builds the thunk that loads and activates one named bridge preset.
 * User Story: As bridge preset selection, I need a thunk that fetches preset
 * rules and stores them as active rules for the current runtime.
 */
inline ThunkAction<FDirectiveRuleSet, FStoreState>
loadBridgePresetThunk(const FString &PresetName) {
  return [PresetName](std::function<AnyAction(const AnyAction &)> Dispatch,
                      std::function<FStoreState()> GetState)
             -> func::AsyncResult<FDirectiveRuleSet> {
    const auto ApiKeyError = Errors::requireApiKeyGuidance(
        SDKConfig::GetApiUrl(), SDKConfig::GetApiKey());
    return ApiKeyError.hasValue
        ? detail::RejectAsync<FDirectiveRuleSet>(ApiKeyError.value)
        : func::AsyncChain::then<FDirectiveRuleSet, FDirectiveRuleSet>(
              APISlice::Endpoints::postBridgePreset(PresetName)(Dispatch,
                                                                GetState),
              [Dispatch, PresetName](const FDirectiveRuleSet &Ruleset) {
                FDirectiveRuleSet ActiveRuleset = Ruleset;
                ActiveRuleset.Id = ActiveRuleset.Id.IsEmpty()
                                       ? PresetName
                                       : ActiveRuleset.Id;
                Dispatch(BridgeSlice::Actions::AddActivePreset(ActiveRuleset));
                return detail::ResolveAsync(Ruleset);
              });
  };
}

/**
 * Builds the thunk that fetches the available bridge rules.
 * User Story: As bridge rule inspection, I need a thunk that loads rule
 * metadata so tools can display server-provided validation rules.
 */
inline ThunkAction<TArray<FBridgeRule>, FStoreState> getBridgeRulesThunk() {
  return [](std::function<AnyAction(const AnyAction &)> Dispatch,
            std::function<FStoreState()> GetState)
             -> func::AsyncResult<TArray<FBridgeRule>> {
    const auto ApiKeyError = Errors::requireApiKeyGuidance(
        SDKConfig::GetApiUrl(), SDKConfig::GetApiKey());
    return ApiKeyError.hasValue
        ? detail::RejectAsync<TArray<FBridgeRule>>(ApiKeyError.value)
        : APISlice::Endpoints::getBridgeRules()(Dispatch, GetState);
  };
}

/**
 * Builds the thunk that loads all registered bridge rulesets.
 * User Story: As bridge ruleset management, I need a thunk that refreshes the
 * ruleset catalog so the slice reflects current server state.
 */
inline ThunkAction<TArray<FDirectiveRuleSet>, FStoreState> listRulesetsThunk() {
  return [](std::function<AnyAction(const AnyAction &)> Dispatch,
            std::function<FStoreState()> GetState)
             -> func::AsyncResult<TArray<FDirectiveRuleSet>> {
    const auto ApiKeyError = Errors::requireApiKeyGuidance(
        SDKConfig::GetApiUrl(), SDKConfig::GetApiKey());
    return ApiKeyError.hasValue
        ? detail::RejectAsync<TArray<FDirectiveRuleSet>>(ApiKeyError.value)
        : func::AsyncChain::then<TArray<FDirectiveRuleSet>,
                                  TArray<FDirectiveRuleSet>>(
              APISlice::Endpoints::getRulesets()(Dispatch, GetState),
              [Dispatch](const TArray<FDirectiveRuleSet> &Rulesets) {
                Dispatch(
                    BridgeSlice::Actions::SetAvailableRulesets(Rulesets));
                return detail::ResolveAsync(Rulesets);
              });
  };
}

/**
 * Builds the thunk that loads available bridge preset ids.
 * User Story: As preset pickers, I need a thunk that fetches preset ids so the
 * UI can offer the current list of server-defined presets.
 */
inline ThunkAction<TArray<FString>, FStoreState> listRulePresetsThunk() {
  return [](std::function<AnyAction(const AnyAction &)> Dispatch,
            std::function<FStoreState()> GetState)
             -> func::AsyncResult<TArray<FString>> {
    const auto ApiKeyError = Errors::requireApiKeyGuidance(
        SDKConfig::GetApiUrl(), SDKConfig::GetApiKey());
    return ApiKeyError.hasValue
        ? detail::RejectAsync<TArray<FString>>(ApiKeyError.value)
        : func::AsyncChain::then<TArray<FString>, TArray<FString>>(
              APISlice::Endpoints::getRulePresets()(Dispatch, GetState),
              [Dispatch](const TArray<FString> &PresetIds) {
                Dispatch(
                    BridgeSlice::Actions::SetAvailablePresetIds(PresetIds));
                return detail::ResolveAsync(PresetIds);
              });
  };
}

/**
 * Builds the thunk that registers or updates a bridge ruleset.
 * User Story: As bridge ruleset editing, I need a thunk that persists a ruleset
 * and refreshes local state with the saved server version.
 */
inline ThunkAction<FDirectiveRuleSet, FStoreState>
registerRulesetThunk(const FDirectiveRuleSet &Ruleset) {
  return [Ruleset](std::function<AnyAction(const AnyAction &)> Dispatch,
                   std::function<FStoreState()> GetState)
             -> func::AsyncResult<FDirectiveRuleSet> {
    const auto ApiKeyError = Errors::requireApiKeyGuidance(
        SDKConfig::GetApiUrl(), SDKConfig::GetApiKey());
    return ApiKeyError.hasValue
        ? detail::RejectAsync<FDirectiveRuleSet>(ApiKeyError.value)
        : func::AsyncChain::then<FDirectiveRuleSet, FDirectiveRuleSet>(
              APISlice::Endpoints::postRuleRegister(Ruleset)(Dispatch,
                                                             GetState),
              [Dispatch, GetState](const FDirectiveRuleSet &Registered) {
                const TArray<FDirectiveRuleSet> Updated = [&]() {
                  TArray<FDirectiveRuleSet> R =
                      GetState().Bridge.AvailableRulesets;
                  const int32 Found = R.IndexOfByPredicate(
                      [&Registered](const FDirectiveRuleSet &X) {
                        return X.Id == Registered.Id;
                      });
                  Found != INDEX_NONE ? (void)(R[Found] = Registered)
                                      : (void)R.Add(Registered);
                  return R;
                }();
                Dispatch(
                    BridgeSlice::Actions::SetAvailableRulesets(Updated));
                return detail::ResolveAsync(Registered);
              });
  };
}

/**
 * Builds the thunk that deletes a bridge ruleset.
 * User Story: As bridge ruleset maintenance, I need a thunk that removes a
 * ruleset remotely and updates the local cache to match.
 */
inline ThunkAction<rtk::FEmptyPayload, FStoreState>
deleteRulesetThunk(const FString &RulesetId) {
  return [RulesetId](std::function<AnyAction(const AnyAction &)> Dispatch,
                     std::function<FStoreState()> GetState)
             -> func::AsyncResult<rtk::FEmptyPayload> {
    const auto ApiKeyError = Errors::requireApiKeyGuidance(
        SDKConfig::GetApiUrl(), SDKConfig::GetApiKey());
    return ApiKeyError.hasValue
        ? detail::RejectAsync<rtk::FEmptyPayload>(ApiKeyError.value)
        : func::AsyncChain::then<rtk::FEmptyPayload, rtk::FEmptyPayload>(
              APISlice::Endpoints::deleteRule(RulesetId)(Dispatch, GetState),
              [Dispatch, GetState,
               RulesetId](const rtk::FEmptyPayload &Payload) {
                TArray<FDirectiveRuleSet> Rulesets =
                    GetState().Bridge.AvailableRulesets;
                Rulesets.RemoveAll(
                    [RulesetId](const FDirectiveRuleSet &Ruleset) {
                      return Ruleset.Id == RulesetId ||
                             Ruleset.RulesetId == RulesetId;
                    });
                Dispatch(
                    BridgeSlice::Actions::SetAvailableRulesets(Rulesets));
                return detail::ResolveAsync(Payload);
              });
  };
}

} // namespace rtk
