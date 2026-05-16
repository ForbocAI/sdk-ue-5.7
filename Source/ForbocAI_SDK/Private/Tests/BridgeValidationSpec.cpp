#include "Bridge/BridgeModule.h"
#include "Misc/AutomationTest.h"

DEFINE_SPEC(FForbocAIBridgeSpec, "ForbocAI.Bridge.Validation",
            EAutomationTestFlags::ProductFilter |
                EAutomationTestFlags_ApplicationContextMask)

void FForbocAIBridgeSpec::Define() {
  Describe("BridgeOps::Validate", [this]() {
    It("accepts a MOVE action with payload coordinates", [this]() {
      FAgentAction Action;
      Action.Type = TEXT("MOVE");
      Action.Target = TEXT("player");
      Action.PayloadJson = TEXT("{\"x\":10,\"y\":20}");

      const TArray<FValidationRule> Rules = BridgeOps::CreateRPGRules();
      const FValidationResult Result =
          BridgeOps::Validate(Action, Rules, BridgeFactory::CreateContext());

      TestTrue("Result.bValid", Result.bValid);
    });

    It("rejects ATTACK without a target", [this]() {
      FAgentAction Action;
      Action.Type = TEXT("ATTACK");
      Action.PayloadJson = TEXT("{}");

      const TArray<FValidationRule> Rules = BridgeOps::CreateRPGRules();
      const FValidationResult Result =
          BridgeOps::Validate(Action, Rules, BridgeFactory::CreateContext());

      TestFalse("Result.bValid", Result.bValid);
      TestEqual("Result.Reason", Result.Reason, FString(TEXT("Action target cannot be empty")));
    });
  });
}
