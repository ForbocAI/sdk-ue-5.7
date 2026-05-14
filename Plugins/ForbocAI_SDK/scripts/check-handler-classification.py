#!/usr/bin/env python3
"""
check-handler-classification.py

Enforces the handler classification rules in ProtocolThunks.h:
1. Pass-through handlers must not import/use cortex inference (e.g. Runtime.CompleteInference).
2. Local-capability handlers must return their required result type.
3. The UE classification table must match the TS table (if the SDK repo is cloned alongside).
"""

import sys
import os
import re
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
ROOT_DIR = SCRIPT_DIR.parent.parent.parent
PROTOCOL_THUNKS = SCRIPT_DIR.parent / "Source" / "ForbocAI_SDK" / "Public" / "Protocol" / "ProtocolThunks.h"
TS_SDK_HANDLERS = ROOT_DIR.parent / "sdk" / "packages" / "core" / "src" / "protocolHandlers" / "index.ts"

def parse_md_table(text):
    classifications = {}
    lines = text.split('\n')
    in_table = False
    for line in lines:
        if line.strip().startswith('// | Instruction'):
            in_table = True
            continue
        if in_table and line.strip().startswith('// | ---'):
            continue
        if in_table and line.strip().startswith('// |'):
            parts = [p.strip() for p in line.strip().split('|')]
            if len(parts) >= 3:
                instruction = parts[1]
                classification = parts[2]
                classifications[instruction] = classification
        elif in_table and not line.strip().startswith('// |'):
            break
    return classifications

def extract_function_body(text, func_name):
    # Very rudimentary extraction for C++ functions
    pattern = rf'inline func::AsyncResult<FAgentResponse>\s*Handle{func_name}\b.*?\{{'
    match = re.search(pattern, text, re.DOTALL)
    if not match:
        return ""
    start = match.end()
    brace_count = 1
    i = start
    while i < len(text):
        if text[i] == '{':
            brace_count += 1
        elif text[i] == '}':
            brace_count -= 1
            if brace_count == 0:
                return text[start:i]
        i += 1
    return ""

def main():
    if not PROTOCOL_THUNKS.exists():
        print(f"Error: {PROTOCOL_THUNKS} not found.")
        return 1

    with open(PROTOCOL_THUNKS, "r", encoding="utf-8") as f:
        ue_code = f.read()

    ue_classifications = parse_md_table(ue_code)
    if not ue_classifications:
        print("Error: Could not parse classification table from ProtocolThunks.h")
        return 1

    # Filter out ExecuteInference from UE as it's legacy TS only
    # Actually wait, TS has it but UE doesn't. We'll handle this in the diff.

    failures = 0

    # 1 & 2: Check each handler
    for instruction, classification in ue_classifications.items():
        body = extract_function_body(ue_code, instruction)
        if not body:
            print(f"Error: Could not find function Handle{instruction} in ProtocolThunks.h")
            failures += 1
            continue

        if classification == "Pass-through":
            if "CompleteInference" in body or "nodeCortexThunk" in body:
                print(f"[FAIL] Pass-through handler Handle{instruction} uses Cortex/Inference.")
                failures += 1
            else:
                print(f"[OK] Pass-through handler Handle{instruction} stays clear of inference.")

            # Pass-through handlers should return Serialize<Instruction>Result except ExecuteInference
            if instruction == "Reasoning":
                if "SerializeReasoningResult" not in body:
                    print(f"[FAIL] Handle{instruction} missing SerializeReasoningResult.")
                    failures += 1
                else:
                    print(f"[OK] Handle{instruction} returns correct result type.")

        elif classification == "Local":
            if instruction == "IdentifyActor":
                if "SerializeIdentifyActorResult" not in body:
                    print(f"[FAIL] Local handler Handle{instruction} missing SerializeIdentifyActorResult.")
                    failures += 1
                else:
                    print(f"[OK] Local handler Handle{instruction} returns correct result type.")
            elif instruction == "QueryVector":
                if "SerializeQueryVectorResult" not in body:
                    print(f"[FAIL] Local handler Handle{instruction} missing SerializeQueryVectorResult.")
                    failures += 1
                else:
                    print(f"[OK] Local handler Handle{instruction} returns correct result type.")
            elif instruction == "Decision":
                if "SerializeDecisionResult" not in body:
                    print(f"[FAIL] Local handler Handle{instruction} missing SerializeDecisionResult.")
                    failures += 1
                else:
                    print(f"[OK] Local handler Handle{instruction} returns correct result type.")
            elif instruction == "Finalize":
                if "BuildAgentResponse" not in body:
                    print(f"[FAIL] Local handler Handle{instruction} missing BuildAgentResponse.")
                    failures += 1
                else:
                    print(f"[OK] Local handler Handle{instruction} returns correct result type.")

    # 3: Divergence check
    if TS_SDK_HANDLERS.exists():
        with open(TS_SDK_HANDLERS, "r", encoding="utf-8") as f:
            ts_code = f.read()
        
        # In TS, table is formatted with ' * |' instead of '// |'
        ts_classifications = {}
        in_table = False
        for line in ts_code.split('\n'):
            if line.strip().startswith('* | Instruction'):
                in_table = True
                continue
            if in_table and line.strip().startswith('* | ---'):
                continue
            if in_table and line.strip().startswith('* |'):
                parts = [p.strip() for p in line.strip().split('|')]
                if len(parts) >= 3:
                    ts_classifications[parts[1]] = parts[2]
            elif in_table and not line.strip().startswith('* |'):
                break

        # Remove ExecuteInference for parity check if it's TS-only legacy
        if 'ExecuteInference' in ts_classifications:
            del ts_classifications['ExecuteInference']
        if 'ExecuteInference' in ue_classifications:
            del ue_classifications['ExecuteInference']

        if ue_classifications != ts_classifications:
            print(f"[FAIL] UE and TS classification tables diverge!")
            print(f"  UE: {ue_classifications}")
            print(f"  TS: {ts_classifications}")
            failures += 1
        else:
            print("[OK] UE and TS classification tables match.")
    else:
        print(f"[WARN] TS SDK not found at {TS_SDK_HANDLERS}, skipping divergence check.")

    if failures > 0:
        print(f"\nFailed {failures} checks.")
        return 1
    
    print("\nAll handler classification checks passed.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
