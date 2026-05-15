import re
with open("Plugins/ForbocAI_SDK/Source/ForbocAI_SDK/Public/API/APICodecs.h", "r") as f:
    content = f.read()

# Fix persona -> structuredPersona in EncodeProcessTapeObject
content = content.replace(
    '''!Tape.Persona.IsEmpty()
          ? (Root->SetStringField(TEXT("persona"), Tape.Persona), void())
          : void(),''',
    '''!Tape.Persona.IsEmpty()
          ? (Root->SetStringField(TEXT("structuredPersona"), Tape.Persona), void())
          : void(),'''
)

# Add decisionIntent and reasoningOutput serialization to EncodeProcessTapeObject
old_encode = '''Root->SetBoolField(TEXT("vectorQueried"), Tape.bVectorQueried), Root);'''
new_encode = '''Root->SetBoolField(TEXT("vectorQueried"), Tape.bVectorQueried),
      Tape.bDecisionCompleted
          ? [&]() {
              const TSharedRef<FJsonObject> Intent = MakeShared<FJsonObject>();
              Intent->SetStringField(TEXT("goal"), Tape.DecisionIntent.Goal);
              Intent->SetStringField(TEXT("actionType"), Tape.DecisionIntent.ActionType);
              if (!Tape.DecisionIntent.Target.IsEmpty()) {
                Intent->SetStringField(TEXT("target"), Tape.DecisionIntent.Target);
              }
              Root->SetObjectField(TEXT("decisionIntent"), Intent);
            }()
          : void(),
      Tape.bReasoningCompleted
          ? [&]() {
              const TSharedRef<FJsonObject> Reasoning = MakeShared<FJsonObject>();
              Reasoning->SetStringField(TEXT("reasoningText"), Tape.ReasoningOutput.ReasoningText);
              Reasoning->SetStringField(TEXT("responseText"), Tape.ReasoningOutput.ResponseText);
              Root->SetObjectField(TEXT("reasoningOutput"), Reasoning);
            }()
          : void(),
      Root);'''

content = content.replace(old_encode, new_encode)

# Fix persona -> structuredPersona in DecodeProcessTapeObject
content = content.replace(
    '''Tape.Persona = JsonInterop::OptionalStringFromField(
                    Object, TEXT("persona")),''',
    '''Tape.Persona = JsonInterop::OptionalStringFromField(
                    Object, TEXT("structuredPersona")),'''
)

# Add decisionIntent and reasoningOutput deserialization to DecodeProcessTapeObject
old_decode = '''Tape.bVectorQueried = JsonInterop::detail::TryGetBoolAs(
                    Object, TEXT("vectorQueried"), false),
                true);'''
new_decode = '''Tape.bVectorQueried = JsonInterop::detail::TryGetBoolAs(
                    Object, TEXT("vectorQueried"), false),
                Tape.bDecisionCompleted = Object->HasTypedField<EJson::Object>(TEXT("decisionIntent")),
                Tape.bDecisionCompleted
                    ? [&]() {
                        const TSharedPtr<FJsonObject> Intent = Object->GetObjectField(TEXT("decisionIntent"));
                        Tape.DecisionIntent.Goal = Intent->GetStringField(TEXT("goal"));
                        Tape.DecisionIntent.ActionType = Intent->GetStringField(TEXT("actionType"));
                        Tape.DecisionIntent.Target = JsonInterop::OptionalStringFromField(Intent, TEXT("target"));
                      }()
                    : void(),
                Tape.bReasoningCompleted = Object->HasTypedField<EJson::Object>(TEXT("reasoningOutput")),
                Tape.bReasoningCompleted
                    ? [&]() {
                        const TSharedPtr<FJsonObject> Reasoning = Object->GetObjectField(TEXT("reasoningOutput"));
                        Tape.ReasoningOutput.ReasoningText = Reasoning->GetStringField(TEXT("reasoningText"));
                        Tape.ReasoningOutput.ResponseText = Reasoning->GetStringField(TEXT("responseText"));
                      }()
                    : void(),
                true);'''

content = content.replace(old_decode, new_decode)

with open("Plugins/ForbocAI_SDK/Source/ForbocAI_SDK/Public/API/APICodecs.h", "w") as f:
    f.write(content)
