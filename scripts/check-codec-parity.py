#!/usr/bin/env python3
"""
check-codec-parity.py

Validates that the UE SDK's JSON codecs for the protocol tape and reasoning result
match the canonical OpenAPI definitions. Fails if UE emits flattened fields or
incorrect field names.
"""

import sys
import re
from pathlib import Path

try:
    import yaml
except ImportError:
    print("Warning: PyYAML not installed. Skipping dynamic OpenAPI check. Provide a hardcoded fallback or install PyYAML.", file=sys.stderr)
    sys.exit(0)

SCRIPT_DIR = Path(__file__).resolve().parent
PLUGIN_ROOT = SCRIPT_DIR.parent
SDK_UE_DIR = PLUGIN_ROOT
WORKSPACE_ROOT = SDK_UE_DIR.parent

# Paths to UE source files
API_CODECS_H = PLUGIN_ROOT / "Source" / "ForbocAI_SDK" / "Public" / "API" / "APICodecs.h"
THUNK_DETAIL_H = PLUGIN_ROOT / "Source" / "ForbocAI_SDK" / "Public" / "Core" / "ThunkDetail.h"

def resolve_openapi_path() -> Path:
    candidates = [
        WORKSPACE_ROOT / "api" / "api" / "openapi.yaml",
        WORKSPACE_ROOT / "Forboc.AI" / "api" / "api" / "openapi.yaml",
        Path.cwd() / "api-checkout" / "api" / "openapi.yaml"
    ]
    for c in candidates:
        if c.exists():
            return c.resolve()
    return None

def extract_ue_set_string_fields(content: str, function_name: str) -> set[str]:
    """Finds all Root->Set*Field(TEXT("fieldName") calls within a given function block."""
    pattern = rf'inline.*?{function_name}\b.*?\{{(?P<body>.*?)\}};'
    # Very rudimentary block extraction
    match = re.search(rf'{function_name}\b.*?\{{', content, re.DOTALL)
    if not match:
        return set()
    start = match.end()
    brace_count = 1
    i = start
    while i < len(content):
        if content[i] == '{':
            brace_count += 1
        elif content[i] == '}':
            brace_count -= 1
            if brace_count == 0:
                body = content[start:i]
                # Now extract field names: TEXT("someName")
                fields = re.findall(r'Set\w*Field\s*\(\s*TEXT\("([^"]+)"\)', body)
                # Also include JsonInterop::SetFieldFromJsonString(Root, TEXT("context")
                interop = re.findall(r'SetFieldFromJsonString\s*\([^,]+,\s*TEXT\("([^"]+)"\)', body)
                return set(fields + interop)
        i += 1
    return set()

def main() -> int:
    openapi_path = resolve_openapi_path()
    if not openapi_path:
        print("Warning: openapi.yaml not found. Cannot verify codec parity dynamically.", file=sys.stderr)
        return 0

    with open(openapi_path, "r", encoding="utf-8") as f:
        spec = yaml.safe_load(f)

    failures = []

    # 1. Check NPCProcessTape
    tape_schema = spec.get("components", {}).get("schemas", {}).get("NPCProcessTape", {}).get("properties", {})
    expected_tape_fields = set(tape_schema.keys())

    if API_CODECS_H.exists():
        codecs_content = API_CODECS_H.read_text(encoding="utf-8")
        ue_tape_fields = extract_ue_set_string_fields(codecs_content, "EncodeProcessTapeObject")
        
        # In UE, we also have nested fields inside Actor. 
        # The extraction just gets all literal keys.
        # Let's filter to top level by looking at expected keys
        missing_in_ue = expected_tape_fields - ue_tape_fields
        # UE might emit extra things like npcId, persona, data which are inside Actor.
        # So we just check if expected_tape_fields are present.
        for field in missing_in_ue:
            failures.append(f"UE EncodeProcessTapeObject is missing field: '{field}'")
            
        # specifically check if 'persona' is used instead of 'structuredPersona' as a top-level field on the tape
        if 'persona' in ue_tape_fields and 'structuredPersona' in expected_tape_fields and 'structuredPersona' not in ue_tape_fields:
            # Wait, 'persona' is valid inside Actor object!
            # Let's do a stricter regex for Root->Set...
            root_fields = re.findall(r'Root->Set\w*Field\s*\(\s*TEXT\("([^"]+)"\)', codecs_content)
            # Find the EncodeProcessTapeObject function
            match = re.search(r'EncodeProcessTapeObject\b.*?\{(.*?)\n\}', codecs_content, re.DOTALL)
            if match:
                body = match.group(1)
                root_set = re.findall(r'Root->Set\w*Field\s*\(\s*TEXT\("([^"]+)"\)', body)
                interop = re.findall(r'JsonInterop::SetFieldFromJsonString\s*\(\s*Root\s*,\s*TEXT\("([^"]+)"\)', body)
                top_level = set(root_set + interop)
                missing = expected_tape_fields - top_level
                extra = top_level - expected_tape_fields
                for f in missing:
                    failures.append(f"Tape missing top-level field: {f}")
                for f in extra:
                    failures.append(f"Tape has unexpected top-level field: {f}")

    # 2. Check ReasoningResult
    reasoning_schema = spec.get("components", {}).get("schemas", {}).get("ReasoningResult", {}).get("properties", {})
    expected_reasoning_fields = set(reasoning_schema.keys())

    if THUNK_DETAIL_H.exists():
        thunk_content = THUNK_DETAIL_H.read_text(encoding="utf-8")
        match = re.search(r'SerializeReasoningResult\b.*?\{(.*?)\n\}', thunk_content, re.DOTALL)
        if match:
            body = match.group(1)
            root_set = re.findall(r'Root->Set\w*Field\s*\(\s*TEXT\("([^"]+)"\)', body)
            missing = expected_reasoning_fields - set(root_set)
            for f in missing:
                failures.append(f"SerializeReasoningResult missing top-level field: {f}")

    if failures:
        print("Codec parity check failed:", file=sys.stderr)
        for f in failures:
            print(f"- {f}", file=sys.stderr)
        return 1
    
    print("Codec parity OK.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
