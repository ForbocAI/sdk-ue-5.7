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

TEST_DIRS=("$SDK_TESTS")
[ -d "$DEMO_TESTS" ] && TEST_DIRS+=("$DEMO_TESTS")

VIOLATIONS=0

echo "=== Test Quality Audit ==="
echo "Checking tests for mock, stub, and no-op patterns..."
echo ""

# ── Rule 1: No mock/stub terms ──
echo "[Rule 1] No fake, stub, simulated, or placeholder terms..."
MOCK_TERMS="fake|stub|simulated\b|placeholder"
HITS=$(rg -ci "$MOCK_TERMS" "${TEST_DIRS[@]}" 2>/dev/null || true)
if [ -n "$HITS" ]; then
  echo "  ✗ Mock/stub terminology found:"
  rg -ni "$MOCK_TERMS" "${TEST_DIRS[@]}" 2>/dev/null || true
  VIOLATIONS=$((VIOLATIONS + 1))
else
  echo "  ✓ No mock/stub terminology found."
fi
echo ""

# ── Rule 2: No no-op assertions ──
echo "[Rule 2] No no-op assertions..."
NOOP_TERMS="TestTrue\([^,]*, true\)|TestFalse\([^,]*, false\)|TestEqual\([^,]*, \w+, \w+\)"
# Specifically matching TestTrue("...", true) or TestFalse("...", false)
# Actually, TestEqual might have identical arguments but regex is hard. Let's stick to true/false.
NOOP_EXACT="TestTrue\([^,]+,\s*true\)|TestFalse\([^,]+,\s*false\)"
HITS=$(rg -ci "$NOOP_EXACT" "${TEST_DIRS[@]}" 2>/dev/null || true)
if [ -n "$HITS" ]; then
  echo "  ✗ No-op assertions found:"
  rg -ni "$NOOP_EXACT" "${TEST_DIRS[@]}" 2>/dev/null || true
  VIOLATIONS=$((VIOLATIONS + 1))
else
  echo "  ✓ No no-op assertions found."
fi
echo ""

# ── Rule 3: No hard-coded JSON strings ──
echo "[Rule 3] No hard-coded JSON strings in tests..."
# E.g. TEXT("{\"action\":...")
JSON_TERMS="TEXT\(\"\{.*\}\"\)"
HITS=$(rg -ci "$JSON_TERMS" "${TEST_DIRS[@]}" 2>/dev/null || true)
if [ -n "$HITS" ]; then
  echo "  ✗ Hard-coded JSON found:"
  rg -ni "$JSON_TERMS" "${TEST_DIRS[@]}" 2>/dev/null || true
  VIOLATIONS=$((VIOLATIONS + 1))
else
  echo "  ✓ No hard-coded JSON found."
fi
echo ""

# ── Rule 4: No AddWarning for infrastructure failures ──
echo "[Rule 4] No AddWarning to swallow failures..."
WARN_TERMS="AddWarning\("
HITS=$(rg -ci "$WARN_TERMS" "${TEST_DIRS[@]}" 2>/dev/null || true)
if [ -n "$HITS" ]; then
  echo "  ✗ AddWarning usage found (should fail instead):"
  rg -ni "$WARN_TERMS" "${TEST_DIRS[@]}" 2>/dev/null || true
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
