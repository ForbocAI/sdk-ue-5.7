#!/usr/bin/env bash
# check-test-quality.sh
# Audits the UE SDK and Demo tests for simulated/mocked test patterns.
#
# User Story: As a maintainer, I need to ensure tests verify actual runtime
# logic against real infrastructure, rather than using stubs or fake data.
#
# Rules:
#   1. No "fake", "stub", "simulated", or "placeholder" terms.
#   2. No no-op assertions like `TestTrue(..., true)` or `TestFalse(..., false)`.
#   3. No hard-coded JSON strings (e.g. `TEXT("{\"action\":...}")`).
#   4. No swallowing failures via AddWarning() instead of failing.
#
# Run from the SDK plugin root:
#   bash scripts/check-test-quality.sh

set -euo pipefail

# Determine search tool (rg preferred, grep as fallback)
if command -v rg >/dev/null 2>&1; then
  SEARCH_CMD="rg -ni"
  COUNT_CMD="rg -ci"
else
  SEARCH_CMD="grep -rnHiE"
  COUNT_CMD="grep -rcHiE"
fi

PLUGIN_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SDK_TESTS="$PLUGIN_ROOT/Source/ForbocAI_SDK/Private/Tests"

DEMO_ROOT=""
while [[ $# -gt 0 ]]; do
  case $1 in
    --demo-root)
      DEMO_ROOT="$2"
      shift 2
      ;;
    *)
      shift
      ;;
  esac
done

if [ -n "$DEMO_ROOT" ]; then
  DEMO_TESTS="$DEMO_ROOT/Source/DemoProject/Tests"
else
  DEMO_TESTS="$PLUGIN_ROOT/../../Source/DemoProject/Tests"
fi

TEST_DIRS=()
[ -d "$SDK_TESTS" ] && TEST_DIRS+=("$SDK_TESTS")
[ -d "$DEMO_TESTS" ] && TEST_DIRS+=("$DEMO_TESTS")

if [ ${#TEST_DIRS[@]} -eq 0 ]; then
  echo "No test directories found to scan."
  exit 0
fi

VIOLATIONS=0

echo "=== Test Quality Audit ==="
echo "Checking tests for mock, stub, and no-op patterns..."
echo ""

# ── Rule 1: No mock/stub terms ──
echo "[Rule 1] No fake, stub, simulated, or placeholder terms..."
MOCK_TERMS="fake|stub|simulated\b|placeholder"
HITS=$($COUNT_CMD "$MOCK_TERMS" "${TEST_DIRS[@]}" | grep -v ":0$" || true)
if [ -n "$HITS" ]; then
  echo "  ✗ Mock/stub terminology found:"
  $SEARCH_CMD "$MOCK_TERMS" "${TEST_DIRS[@]}" || true
  VIOLATIONS=$((VIOLATIONS + 1))
else
  echo "  ✓ No mock/stub terminology found."
fi
echo ""

# ── Rule 2: No no-op assertions ──
echo "[Rule 2] No no-op assertions..."
NOOP_EXACT="TestTrue\([^,]+,\s*true\)|TestFalse\([^,]+,\s*false\)"
HITS=$($COUNT_CMD "$NOOP_EXACT" "${TEST_DIRS[@]}" | grep -v ":0$" || true)
if [ -n "$HITS" ]; then
  echo "  ✗ No-op assertions found:"
  $SEARCH_CMD "$NOOP_EXACT" "${TEST_DIRS[@]}" || true
  VIOLATIONS=$((VIOLATIONS + 1))
else
  echo "  ✓ No no-op assertions found."
fi
echo ""

# ── Rule 3: No hard-coded JSON strings ──
echo "[Rule 3] No hard-coded JSON strings in tests..."
JSON_TERMS="TEXT\(\"\{[^\)]*\}\"\)"
HITS=$($COUNT_CMD "$JSON_TERMS" "${TEST_DIRS[@]}" | grep -v ":0$" || true)
if [ -n "$HITS" ]; then
  echo "  ✗ Hard-coded JSON found:"
  $SEARCH_CMD "$JSON_TERMS" "${TEST_DIRS[@]}" || true
  VIOLATIONS=$((VIOLATIONS + 1))
else
  echo "  ✓ No hard-coded JSON found."
fi
echo ""

# ── Rule 4: No AddWarning for infrastructure failures ──
echo "[Rule 4] No AddWarning to swallow failures..."
WARN_TERMS="AddWarning\("
HITS=$($COUNT_CMD "$WARN_TERMS" "${TEST_DIRS[@]}" | grep -v ":0$" || true)
if [ -n "$HITS" ]; then
  echo "  ✗ AddWarning usage found (should fail instead):"
  $SEARCH_CMD "$WARN_TERMS" "${TEST_DIRS[@]}" || true
  VIOLATIONS=$((VIOLATIONS + 1))
else
  echo "  ✓ No AddWarning usage found."
fi
echo ""

# ── Summary ──
echo "=== Results ==="
if [ "$VIOLATIONS" -eq 0 ]; then
  echo "✓ Test quality is clean. No simulated coverage detected."
  exit 0
else
  echo "✗ $VIOLATIONS test quality violation(s) found."
  echo "  Tests must verify real runtime logic, avoid hardcoded responses, and fail properly."
  exit 1
fi
