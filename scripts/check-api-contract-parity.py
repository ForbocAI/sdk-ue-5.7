#!/usr/bin/env python3
"""Static UE parity guard for the API-published test-game contract."""

from __future__ import annotations

import json
import os
import re
import sys
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
PLUGIN_ROOT = SCRIPT_DIR.parent
REPO_ROOT = PLUGIN_ROOT
WORKSPACE_ROOT = REPO_ROOT.parent


def read_text(relative: str) -> str:
    return (PLUGIN_ROOT / relative).read_text(encoding="utf-8")


def resolve_contract_path() -> Path:
    candidates: list[Path] = []
    for env_name in ("API_TEST_GAME_CONTRACT", "TEST_GAME_CONTRACT", "UE_API_CONTRACT"):
        value = os.environ.get(env_name)
        if value:
            raw = Path(value)
            candidates.append(raw if raw.is_absolute() else Path.cwd() / raw)

    candidates.extend(
        [
            Path.cwd() / "api-checkout/api/contract/test-game-contract.json",
            REPO_ROOT / "api-checkout/api/contract/test-game-contract.json",
            WORKSPACE_ROOT / "api/api/contract/test-game-contract.json",
        ]
    )

    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()

    print("API test-game contract not found.", file=sys.stderr)
    print(
        "Set API_TEST_GAME_CONTRACT to ForbocAI/api/api/contract/test-game-contract.json.",
        file=sys.stderr,
    )
    sys.exit(2)


def sorted_difference(expected: set[str], actual: set[str], label: str) -> list[str]:
    missing = sorted(expected - actual)
    extra = sorted(actual - expected)
    return [
        *[f"{label}: missing {item}" for item in missing],
        *[f"{label}: unexpected {item}" for item in extra],
    ]


def validate_contract_shape(contract: dict) -> list[str]:
    failures: list[str] = []
    if not contract.get("version"):
        failures.append("contract is missing version")
    if not contract.get("slotContractVersion"):
        failures.append("contract is missing slotContractVersion")
    if not isinstance(contract.get("requiredCommandGroups"), list):
        failures.append("contract is missing requiredCommandGroups")
    if not isinstance(contract.get("aliasRules"), dict):
        failures.append("contract is missing aliasRules")
    if not isinstance(contract.get("scenarios"), list):
        failures.append("contract is missing scenarios")
    return failures


def validate_contract_matrix(contract: dict) -> list[str]:
    failures: list[str] = []
    scenarios = contract.get("scenarios") or []
    required_groups = set(contract.get("requiredCommandGroups") or [])
    seen_ids: set[str] = set()
    used_groups: set[str] = set()

    for scenario in scenarios:
        scenario_id = scenario.get("id")
        if not scenario_id:
            failures.append("scenario is missing id")
            continue
        if scenario_id in seen_ids:
            failures.append(f"duplicate scenario id: {scenario_id}")
        seen_ids.add(scenario_id)

        for command in scenario.get("commands") or []:
            group = command.get("group")
            if group not in required_groups:
                failures.append(
                    f"{scenario_id}: command group {group} is not in requiredCommandGroups"
                )
            used_groups.add(group)
            routes = command.get("expectedRoutes")
            if not isinstance(routes, list) or not routes:
                failures.append(f"{scenario_id}: command {command.get('command')} has no expectedRoutes")

    failures.extend(sorted_difference(required_groups, used_groups, "contract command group usage"))
    return failures


def parse_cpp_command_group_mappings(header: str) -> set[str]:
    return set(re.findall(r'GroupStr == TEXT\("([^"]+)"\)\s*\?\s*ECommandGroup::', header))


def parse_transcript_fields(header: str) -> set[str]:
    match = re.search(r"struct\s+FTranscriptEntry\s*\{(?P<body>.*?)\};", header, re.S)
    if not match:
        return set()
    return set(re.findall(r"\b(?:FString|ECommandGroup|TArray<FString>|ETranscriptStatus)\s+(\w+);", match.group("body")))


def validate_ue_sources(contract: dict) -> list[str]:
    failures: list[str] = []
    contract_header = read_text("Source/ForbocAI_SDK/Public/TestGame/TestGameContract.h")
    types_header = read_text("Source/ForbocAI_SDK/Public/TestGame/TestGameTypes.h")
    command_surface = read_text("Source/ForbocAI_SDK/Public/TestGame/TestGameCommandSurface.h")
    orchestrator = read_text("Source/ForbocAI_SDK/Public/TestGame/TestGameOrchestrator.h")

    required_groups = set(contract.get("requiredCommandGroups") or [])
    mapped_groups = parse_cpp_command_group_mappings(contract_header)
    failures.extend(sorted_difference(required_groups, mapped_groups, "UE ParseCommandGroup mappings"))
    for group in sorted(mapped_groups):
        if "-" in group:
            failures.append(f"UE ParseCommandGroup uses hyphenated group {group}; API uses underscores")

    required_contract_snippets = [
        'Root->GetStringField(TEXT("version"))',
        'Root->GetStringField(TEXT("slotContractVersion"))',
        'Root->TryGetArrayField(TEXT("requiredCommandGroups")',
        'Root->TryGetObjectField(TEXT("aliasRules")',
        'Root->TryGetArrayField(TEXT("scenarios")',
        'ScenObj->GetStringField(TEXT("id"))',
        'CmdObj->GetStringField(TEXT("group"))',
        'CmdObj->TryGetArrayField(TEXT("expectedRoutes")',
        "Local.ExpectedRoutes = Cmd.ExpectedRoutes",
        "Step.Id = Scenario.Id",
    ]
    for snippet in required_contract_snippets:
        if snippet not in contract_header:
            failures.append(f"UE contract parser missing snippet: {snippet}")

    expected_transcript_fields = {
        "Id",
        "ScenarioId",
        "CommandGroup",
        "Command",
        "ExpectedRoutes",
        "Status",
        "Output",
        "Timestamp",
    }
    transcript_fields = parse_transcript_fields(types_header)
    failures.extend(
        sorted_difference(expected_transcript_fields, transcript_fields, "UE transcript fields")
    )

    transcript_text = command_surface + "\n" + orchestrator
    required_transcript_snippets = [
        "ScenarioId",
        "CommandGroup",
        "Command",
        "ExpectedRoutes",
        "Status",
        "Output",
    ]
    for snippet in required_transcript_snippets:
        if snippet not in transcript_text:
            failures.append(f"UE transcript recording missing {snippet}")

    return failures


def main() -> int:
    contract_path = resolve_contract_path()
    contract = json.loads(contract_path.read_text(encoding="utf-8"))

    failures = [
        *validate_contract_shape(contract),
        *validate_contract_matrix(contract),
        *validate_ue_sources(contract),
    ]

    if failures:
        print("UE API contract parity check failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1

    scenario_count = len(contract.get("scenarios") or [])
    group_count = len(contract.get("requiredCommandGroups") or [])
    route_count = sum(
        1
        for scenario in contract.get("scenarios") or []
        for command in scenario.get("commands") or []
        for route in command.get("expectedRoutes") or []
        if route != "local only"
    )
    print(
        f"UE API contract parity OK - {scenario_count} scenarios, "
        f"{group_count} groups, {route_count} API routes."
    )
    print(f"API contract: {contract_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
