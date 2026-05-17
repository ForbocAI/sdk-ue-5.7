/**
 * Protocol payloads isolation tests — tests local serialization codecs
 * NO MOCKING ALLOWED.
 */

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Core/ThunkDetail.h"
#include "Protocol/ProtocolThunks.h"
#include "Protocol/ProtocolRequestTypes.h"
#include "API/APICodecs.h"

using namespace rtk;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSerializeIdentifyActorPayloadTest,
    "ForbocAI.Core.Protocol.SerializeIdentifyActorPayload",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
/**
 * User Story: As a developer, I need RunTest to fulfill its role in the module.
 */
bool FSerializeIdentifyActorPayloadTest::RunTest(const FString &Parameters) {
  FNPCActorInfo Actor;
  Actor.NpcId = TEXT("npc_test_1");
  Actor.Persona = TEXT("Tester");
  Actor.Data.JsonData = FString(TEXT("{") TEXT("\"health\": 100}"));
  
  FString Json = rtk::detail::SerializeIdentifyActorResult(Actor);
  TestTrue("Contains type", Json.Contains(TEXT("\"type\":\"IdentifyActorResult\"")));
  TestTrue("Contains npcId", Json.Contains(TEXT("\"npcId\":\"npc_test_1\"")));
  TestTrue("Contains persona", Json.Contains(TEXT("\"persona\":\"Tester\"")));
  TestTrue("Contains data", Json.Contains(TEXT("\"data\":{\"health\":100\n}")));
  
  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSerializeDecisionPayloadTest,
    "ForbocAI.Core.Protocol.SerializeDecisionPayload",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
/**
 * User Story: As a developer, I need RunTest to fulfill its role in the module.
 */
bool FSerializeDecisionPayloadTest::RunTest(const FString &Parameters) {
  FString Json =
      rtk::detail::SerializeDecisionResult(TEXT("Respond"), TEXT("SPEAK"),
                                           TEXT("Player"));
  
  TestTrue("Contains type", Json.Contains(TEXT("\"type\":\"Decision\"")));
  TestTrue("Contains goal", Json.Contains(TEXT("\"goal\":\"Respond\"")));
  TestTrue("Contains actionType", Json.Contains(TEXT("\"actionType\":\"SPEAK\"")));
  TestTrue("Contains target", Json.Contains(TEXT("\"target\":\"Player\"")));
  
  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSerializeReasoningPayloadTest,
    "ForbocAI.Core.Protocol.SerializeReasoningPayload",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
/**
 * User Story: As a developer, I need RunTest to fulfill its role in the module.
 */
bool FSerializeReasoningPayloadTest::RunTest(const FString &Parameters) {
  FString Json = rtk::detail::SerializeReasoningResult(
      TEXT("Thinking..."), TEXT("Hello there"));
  
  TestTrue("Contains type", Json.Contains(TEXT("\"type\":\"Reasoning\"")));
  TestTrue("Contains reasoningText", Json.Contains(TEXT("\"reasoningText\":\"Thinking...\"")));
  TestTrue("Contains responseText", Json.Contains(TEXT("\"responseText\":\"Hello there\"")));
  
  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FEncodeProcessTapePayloadTest,
    "ForbocAI.Core.Protocol.EncodeProcessTapePayload",
    EAutomationTestFlags_ApplicationContextMask |
        EAutomationTestFlags::EngineFilter)
/**
 * User Story: As a developer, I need RunTest to fulfill its role in the module.
 */
bool FEncodeProcessTapePayloadTest::RunTest(const FString &Parameters) {
  FNPCProcessTape Tape;
  Tape.Observation = TEXT("Saw player");
  Tape.ContextJson = FString(TEXT("{") TEXT("\"time\":\"day\"}"));
  Tape.NpcState.JsonData = FString(TEXT("{") TEXT("}"));
  Tape.Persona = TEXT("Guard");
  
  TSharedRef<FJsonObject> Obj = APISlice::Detail::EncodeProcessTapeObject(Tape);
  FString Json = APISlice::Detail::ToJsonString(Obj);
  
  TestTrue("Contains observation", Json.Contains(TEXT("\"observation\":\"Saw player\"")));
  TestTrue("Contains persona", Json.Contains(TEXT("\"structuredPersona\":\"Guard\"")));
  TestTrue("Contains context", Json.Contains(TEXT("\"context\":{\"time\":\"day\"\n}")));
  
  return true;
}
