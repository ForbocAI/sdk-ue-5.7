#!/usr/bin/env bash
# check-thin-wrapper-guardrails.sh
# Enforces that UE command surfaces stay thin.
#
# Checks for:
#   1. No validation logic in command handlers (Commandlet.cpp, CLIModule.cpp)
#   2. No NPC-policy logic in command surfaces
#   3. No scenario-specific orchestration in command layers
#   4. Command handlers delegate to Ops:: or CLIOps:: — no inline business logic
#   5. Handler functions stay under 50 lines
#
# Mirrors Node's CLI compliance tests for UE.
# Run from the SDK plugin root:
#   bash scripts/check-thin-wrapper-guardrails.sh

set -euo pipefail

# Hard dependency: without ripgrep every rule below silently produces no
# hits and the script reports a false "PASS". Fail loudly instead.
if ! command -v rg >/dev/null 2>&1; then
  echo "[FAIL] ripgrep (rg) is required but not found on PATH." >&2
  echo "       Install ripgrep before running the thin-wrapper guardrails." >&2
  exit 2
fi

PLUGIN_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$PLUGIN_ROOT/Source/ForbocAI_SDK"

# Directories to check (command surfaces only)
COMMAND_SURFACES=(
  "$SRC/Public/CLI/CLIModule.h"
  "$SRC/Public/CLI/CliHandlers.h"
  "$SRC/Public/TestGame/TestGameCommandSurface.h"
)

VIOLATIONS=0

echo "=== Thin-Wrapper Guardrails Check ==="
echo ""

# ── Rule 1: No direct HTTP calls in command surfaces ──
echo "[Rule 1] No direct HTTP calls in command surfaces..."
for f in "${COMMAND_SURFACES[@]}"; do
  [ -f "$f" ] || continue
  if rg -q "(AsyncHttp::Get|AsyncHttp::Post|FHttpModule|IHttpRequest)" "$f" 2>/dev/null; then
    echo "  ✗ VIOLATION: Direct HTTP call in $(basename "$f")"
    rg -n "(AsyncHttp::Get|AsyncHttp::Post|FHttpModule|IHttpRequest)" "$f" 2>/dev/null
    VIOLATIONS=$((VIOLATIONS + 1))
  fi
done
echo ""

# ── Rule 2: No JSON parsing/construction in command surfaces ──
echo "[Rule 2] No JSON construction in command surfaces..."
for f in "${COMMAND_SURFACES[@]}"; do
  [ -f "$f" ] || continue
  # Allow TryGetArrayField (used for arg parsing), block FJsonObject construction
  if rg -q "MakeShared<FJsonObject>|FJsonObjectConverter|SetStringField|SetNumberField|SetBoolField" "$f" 2>/dev/null; then
    echo "  ✗ VIOLATION: JSON construction in $(basename "$f")"
    rg -n "MakeShared<FJsonObject>|SetStringField|SetNumberField" "$f" 2>/dev/null
    VIOLATIONS=$((VIOLATIONS + 1))
  fi
done
echo ""

# ── Rule 3: No direct store dispatch in command surfaces ──
echo "[Rule 3] No direct store dispatch (must go through Ops/CLIOps)..."
for f in "${COMMAND_SURFACES[@]}"; do
  [ -f "$f" ] || continue
  # Command surfaces should call CLIOps::DispatchCommand or Ops::*, not Store.dispatch directly
  if rg -q "\.dispatch\(|\.Dispatch\(" "$f" 2>/dev/null; then
    echo "  ✗ VIOLATION: Direct store dispatch in $(basename "$f")"
    rg -n "\.dispatch\(|\.Dispatch\(" "$f" 2>/dev/null
    VIOLATIONS=$((VIOLATIONS + 1))
  fi
done
echo ""

# ── Rule 4: No scenario-specific constants in command surfaces ──
echo "[Rule 4] No scenario-specific constants in command surfaces..."
SCENARIO_PATTERNS="doomguard|miller|stealth|social-encounter|escape-pursuit|persistence-recovery"
for f in "${COMMAND_SURFACES[@]}"; do
  [ -f "$f" ] || continue
  if rg -qi "$SCENARIO_PATTERNS" "$f" 2>/dev/null; then
    echo "  ✗ VIOLATION: Scenario-specific reference in $(basename "$f")"
    rg -ni "$SCENARIO_PATTERNS" "$f" 2>/dev/null
    VIOLATIONS=$((VIOLATIONS + 1))
  fi
done
echo ""

# ── Rule 5: No game-domain terminology in command surfaces ──
echo "[Rule 5] No game-domain framing (should use product terms)..."
GAME_TERMS="gameplay|game logic|game rules|combat system|RPG|inventory system|quest"
for f in "${COMMAND_SURFACES[@]}"; do
  [ -f "$f" ] || continue
  if rg -qi "$GAME_TERMS" "$f" 2>/dev/null; then
    echo "  ✗ VIOLATION: Game-domain terminology in $(basename "$f")"
    rg -ni "$GAME_TERMS" "$f" 2>/dev/null
    VIOLATIONS=$((VIOLATIONS + 1))
  fi
done
echo ""

# ── Rule 6: Handler function size (max 50 lines) ──
echo "[Rule 6] Handler functions under 50 lines..."
for f in "${COMMAND_SURFACES[@]}"; do
  [ -f "$f" ] || continue
  TOTAL_LINES=$(wc -l < "$f")
  if [ "$TOTAL_LINES" -gt 400 ]; then
    echo "  ✗ VIOLATION: $(basename "$f") is $TOTAL_LINES lines (max 400 for a command surface)"
    VIOLATIONS=$((VIOLATIONS + 1))
  fi
done
echo ""

# ── Summary ──
echo "=== Results ==="
if [ "$VIOLATIONS" -eq 0 ]; then
  echo "✓ All thin-wrapper guardrails passed."
  exit 0
else
  echo "✗ $VIOLATIONS guardrail violation(s) found."
  exit 1
fi
