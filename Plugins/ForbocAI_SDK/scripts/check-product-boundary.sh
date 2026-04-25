#!/usr/bin/env bash
# check-product-boundary.sh
# Audits the UE SDK for game-specific terminology outside the test-game namespace.
#
# The SDK should be game-agnostic. Only the test-game harness (Public/TestGame/*)
# should contain scenario-specific language.
#
# This script checks all non-TestGame headers for:
#   1. Game-domain framing (gameplay, game logic, combat system, etc.)
#   2. Scenario-specific references (doomguard, miller, stealth, etc.)
#   3. Test-game types leaking into generic surfaces
#   4. Harness-specific helpers exported as product APIs
#
# Run from the SDK plugin root:
#   bash scripts/check-product-boundary.sh

set -euo pipefail

PLUGIN_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$PLUGIN_ROOT/Source/ForbocAI_SDK"
PUBLIC="$SRC/Public"

VIOLATIONS=0

echo "=== Product Boundary Audit ==="
echo "Checking non-TestGame surfaces for game-specific terminology..."
echo ""

# Define excluded paths (test-game harness is allowed to be scenario-rich)
EXCLUDE_DIRS="--glob=!**/TestGame/**"
EXCLUDE_TESTS="--glob=!**/Tests/**"

# ── Rule 1: No game-domain framing in generic headers ──
echo "[Rule 1] No game-domain framing outside TestGame/..."
GAME_TERMS="gameplay|game logic|game rules|game engine|combat system|RPG system|inventory system|quest system|leveling|skill tree|character class"
HITS=$(rg -ci "$GAME_TERMS" "$PUBLIC" $EXCLUDE_DIRS $EXCLUDE_TESTS 2>/dev/null || true)
if [ -n "$HITS" ]; then
  echo "  ✗ Game-domain framing found:"
  rg -ni "$GAME_TERMS" "$PUBLIC" $EXCLUDE_DIRS $EXCLUDE_TESTS 2>/dev/null || true
  VIOLATIONS=$((VIOLATIONS + 1))
else
  echo "  ✓ No game-domain framing in generic surfaces."
fi
echo ""

# ── Rule 2: No scenario-specific references in generic headers ──
echo "[Rule 2] No scenario-specific references outside TestGame/..."
SCENARIO_TERMS="doomguard|miller|stealth-door|social-miller|escape-realtime|persistence-recovery|Scout"
HITS=$(rg -ci "$SCENARIO_TERMS" "$PUBLIC" $EXCLUDE_DIRS $EXCLUDE_TESTS 2>/dev/null || true)
if [ -n "$HITS" ]; then
  echo "  ✗ Scenario-specific references found:"
  rg -ni "$SCENARIO_TERMS" "$PUBLIC" $EXCLUDE_DIRS $EXCLUDE_TESTS 2>/dev/null || true
  VIOLATIONS=$((VIOLATIONS + 1))
else
  echo "  ✓ No scenario-specific references in generic surfaces."
fi
echo ""

# ── Rule 3: No TestGame types imported in generic headers ──
echo "[Rule 3] No TestGame type imports in generic headers..."
TG_IMPORTS="TestGame/TestGame|FTestGameState|FScenarioStep|FCommandSpec|ECommandGroup|FTranscriptEntry|ETranscriptStatus"
# Check CLI, Protocol, Core, Blueprint directories
GENERIC_DIRS=("$PUBLIC/CLI" "$PUBLIC/Protocol" "$PUBLIC/Core")
for dir in "${GENERIC_DIRS[@]}"; do
  [ -d "$dir" ] || continue
  HITS=$(rg -ci "$TG_IMPORTS" "$dir" 2>/dev/null || true)
  if [ -n "$HITS" ]; then
    echo "  ✗ TestGame types leaked into $(basename "$dir"):"
    rg -ni "$TG_IMPORTS" "$dir" 2>/dev/null || true
    VIOLATIONS=$((VIOLATIONS + 1))
  fi
done
echo ""

# ── Rule 4: Product terms used correctly ──
echo "[Rule 4] Verifying product-boundary terminology..."
echo "  Expected terms in generic SDK headers:"
echo "    - NPC decisioning, thought/context flow, rule validation"
echo "    - memory, soul/ghost, host-local execution"
echo "  Checking for correct usage..."

# Positive check: ensure key product terms exist somewhere in the generic surface
PRODUCT_TERMS=("AgentOps" "BridgeOps" "MemoryOps" "SoulOps" "GhostOps")
for term in "${PRODUCT_TERMS[@]}"; do
  if ! rg -q "$term" "$PUBLIC" $EXCLUDE_DIRS 2>/dev/null; then
    echo "  ⚠ Warning: Product term '$term' not found in generic headers"
  fi
done
echo ""

# ── Rule 5: No ASCII grid rendering outside TestGame ──
echo "[Rule 5] No rendering helpers outside TestGame/..."
RENDER_TERMS="RenderGrid|RenderRow|CellAt|RenderLegend|ASCII grid"
HITS=$(rg -ci "$RENDER_TERMS" "$PUBLIC" $EXCLUDE_DIRS $EXCLUDE_TESTS 2>/dev/null || true)
if [ -n "$HITS" ]; then
  echo "  ✗ Rendering helpers found outside TestGame:"
  rg -ni "$RENDER_TERMS" "$PUBLIC" $EXCLUDE_DIRS $EXCLUDE_TESTS 2>/dev/null || true
  VIOLATIONS=$((VIOLATIONS + 1))
else
  echo "  ✓ No rendering helpers in generic surfaces."
fi
echo ""

# ── Rule 6: No transcript/harness types outside TestGame ──
echo "[Rule 6] No transcript/harness types outside TestGame/..."
HARNESS_TERMS="FTranscriptEntry|ETranscriptStatus|FHarnessState|FScenarioSliceState|EEventType"
HITS=$(rg -ci "$HARNESS_TERMS" "$PUBLIC" $EXCLUDE_DIRS $EXCLUDE_TESTS 2>/dev/null || true)
if [ -n "$HITS" ]; then
  echo "  ✗ Harness types found outside TestGame:"
  rg -ni "$HARNESS_TERMS" "$PUBLIC" $EXCLUDE_DIRS $EXCLUDE_TESTS 2>/dev/null || true
  VIOLATIONS=$((VIOLATIONS + 1))
else
  echo "  ✓ No harness types in generic surfaces."
fi
echo ""

# ── Summary ──
echo "=== Results ==="
if [ "$VIOLATIONS" -eq 0 ]; then
  echo "✓ Product boundary is clean. No game-specific terminology outside TestGame/."
  exit 0
else
  echo "✗ $VIOLATIONS boundary violation(s) found."
  echo "  Generic SDK surfaces should describe: NPC decisioning, thought/context flow,"
  echo "  rule validation, memory, soul/ghost, host-local execution."
  echo "  Only TestGame/* should contain scenario-specific language."
  exit 1
fi
