import yaml

with open("../api/api/openapi.yaml", "r") as f:
    spec = yaml.safe_load(f)

tape = spec["components"]["schemas"]["NPCProcessTape"]["properties"]
print("OpenAPI NPCProcessTape fields:")
print(list(tape.keys()))

reasoning_result = spec["components"]["schemas"]["ReasoningResult"]["properties"]
print("\nOpenAPI ReasoningResult fields:")
print(list(reasoning_result.keys()))
