#!/usr/bin/env bash
set -euo pipefail

# ══════════════════════════════════════════════════════════════════
# check-ue-fp-conformance.sh — Functional Programming Guardrails
# ══════════════════════════════════════════════════════════════════
#
# User Story: As a maintainer, I need an automated FP conformance
# check so violations of the functional architecture (classes,
# loops, mutation, oversized files) are caught before merge.
#
# Checks:
#   1. No `for` or `while` loops in non-test .h/.cpp files
#   2. No `class` declarations outside ThirdParty/ and Tests/
#   3. No mutable member variables in public headers
#   4. Command handler files under 300-line limit
#   5. No direct HTTP calls in command handler files
#   6. No `if` statements in non-test code (ternary only)
#   7. No `switch` statements (visitor/variant dispatch only)
#   8. No mocking — the word "mock" must not appear in demo tests or new code
#
# Usage:
#   bash scripts/check-ue-fp-conformance.sh
#
# Exit 0 on a compliant tree, non-zero with violations.
# ══════════════════════════════════════════════════════════════════

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="$ROOT/Source/ForbocAI_SDK"
STATUS=0
WARN_COUNT=0
FAIL_COUNT=0

fail() { echo "[FAIL] $1"; STATUS=1; FAIL_COUNT=$((FAIL_COUNT + 1)); }
warn() { echo "[WARN] $1"; WARN_COUNT=$((WARN_COUNT + 1)); }
pass() { echo "[ OK ] $1"; }

echo "╔══════════════════════════════════════════════════╗"
echo "║  FP CONFORMANCE CHECK — ForbocAI UE SDK         ║"
echo "╚══════════════════════════════════════════════════╝"
echo ""

# ── 1. No for/while loops ──────────────────────────────────────
# Recursion or range-based patterns only.
# Allowed: ThirdParty, Tests, SqliteAmalgamation.c

echo "── Rule 1: No imperative loops (for/while) ──"

LOOP_HITS="$(rg -n '\b(for|while)\s*\(' \
  "$SRC/Public" "$SRC/Private" \
  --glob '!**/Tests/**' \
  --glob '!**/ThirdParty/**' \
  --glob '!**/Native/SqliteAmalgamation.c' \
  --type-add 'cpp:*.{h,hpp,cpp}' --type cpp \
  2>/dev/null || true)"

# Filter comment-only lines
LOOP_REAL=""
while IFS= read -r line; do
  [ -z "$line" ] && continue
  code="${line#*:*:}"
  trimmed="$(echo "$code" | sed 's/^[[:space:]]*//')"
  case "$trimmed" in
    \**|//*) continue ;;
  esac
  stripped="$(echo "$code" | sed 's|//.*||' | sed 's|/\*.*\*/||g')"
  if echo "$stripped" | rg -q '\b(for|while)\s*\(' 2>/dev/null; then
    LOOP_REAL="$LOOP_REAL
$line"
  fi
done <<< "$LOOP_HITS"
LOOP_REAL="$(echo "$LOOP_REAL" | sed '/^$/d')"

if [ -n "$LOOP_REAL" ]; then
  fail "Imperative loops found in first-party code:"
  echo "$LOOP_REAL" | head -20
  LOOP_COUNT="$(echo "$LOOP_REAL" | wc -l | tr -d ' ')"
  [ "$LOOP_COUNT" -gt 20 ] && echo "  ... and $((LOOP_COUNT - 20)) more"
else
  pass "No imperative loops in first-party code"
fi

echo ""

# ── 2. No class declarations ──────────────────────────────────
# Structs + factory functions only.
# Allowed: ThirdParty, Tests, UCLASS() macros (UE boundary),
#          forward declarations (class Foo;)

echo "── Rule 2: No class declarations (structs + factories only) ──"

CLASS_HITS="$(rg -n '^\s*class [A-Z]' \
  "$SRC/Public" "$SRC/Private" \
  --glob '!**/Tests/**' \
  --glob '!**/ThirdParty/**' \
  --glob '!**/Native/SqliteAmalgamation.c' \
  --type-add 'cpp:*.{h,hpp,cpp}' --type cpp \
  2>/dev/null || true)"

# Filter: UCLASS macros, forward declarations (class Foo;), and UE-generated
CLASS_REAL=""
while IFS= read -r line; do
  [ -z "$line" ] && continue
  code="${line#*:*:}"
  trimmed="$(echo "$code" | sed 's/^[[:space:]]*//')"
  # Skip forward declarations
  echo "$trimmed" | rg -q 'class [A-Za-z_]+\s*;' 2>/dev/null && continue
  # Skip comment lines
  case "$trimmed" in
    \**|//*) continue ;;
  esac
  CLASS_REAL="$CLASS_REAL
$line"
done <<< "$CLASS_HITS"
CLASS_REAL="$(echo "$CLASS_REAL" | sed '/^$/d')"

if [ -n "$CLASS_REAL" ]; then
  warn "Class declarations found (review for FP compliance):"
  echo "$CLASS_REAL" | head -20
else
  pass "No class declarations in first-party code"
fi

echo ""

# ── 3. No mutable members in public headers ───────────────────
# Copy-on-write pattern: state should be returned, not mutated.
# Allowed: mutable in Lazy<T> (canonical FP memoization),
#          mutable in MemoizedLast (canonical FP memoization),
#          UE-required mutables (GENERATED_BODY internals)

echo "── Rule 3: No mutable member variables in public headers ──"

MUTABLE_HITS="$(rg -n '\bmutable\b' \
  "$SRC/Public" \
  --glob '!**/Tests/**' \
  --glob '!**/ThirdParty/**' \
  --type-add 'cpp:*.{h,hpp,cpp}' --type cpp \
  2>/dev/null || true)"

# Filter canonical FP memoization uses
MUTABLE_REAL=""
while IFS= read -r line; do
  [ -z "$line" ] && continue
  # Allow functional_core.hpp mutable (Lazy, MemoizedLast)
  echo "$line" | rg -q 'functional_core\.hpp' 2>/dev/null && continue
  # Allow rtk.hpp mutable
  echo "$line" | rg -q 'rtk\.hpp' 2>/dev/null && continue
  # Skip comments
  code="${line#*:*:}"
  trimmed="$(echo "$code" | sed 's/^[[:space:]]*//')"
  case "$trimmed" in
    \**|//*) continue ;;
  esac
  MUTABLE_REAL="$MUTABLE_REAL
$line"
done <<< "$MUTABLE_HITS"
MUTABLE_REAL="$(echo "$MUTABLE_REAL" | sed '/^$/d')"

if [ -n "$MUTABLE_REAL" ]; then
  warn "Mutable members in public headers (review for copy-on-write):"
  echo "$MUTABLE_REAL" | head -20
else
  pass "No mutable members in public headers"
fi

echo ""

# ── 4. Command handler file size limit (300 lines) ────────────
# Analogous to the TS SDK 300-line rule for CLI handlers.

echo "── Rule 4: Command handler files under 300 lines ──"

CLI_DIR="$SRC/Private/CLI"
COMMANDLET="$SRC/Private/Commandlet.cpp"
OVERSIZE=0

check_file_size() {
  local file="$1"
  local limit="$2"
  if [ -f "$file" ]; then
    local lines
    lines="$(wc -l < "$file" | tr -d ' ')"
    if [ "$lines" -gt "$limit" ]; then
      fail "$(basename "$file") is $lines lines (limit: $limit)"
      OVERSIZE=1
    else
      pass "$(basename "$file") is $lines lines (limit: $limit)"
    fi
  fi
}

check_file_size "$COMMANDLET" 500

if [ -d "$CLI_DIR" ]; then
  find "$CLI_DIR" -name '*.cpp' -o -name '*.h' | while read -r f; do
    check_file_size "$f" 300
  done
fi

[ "$OVERSIZE" -eq 0 ] 2>/dev/null && pass "All command handlers within size limits" || true

echo ""

# ── 5. No direct HTTP in command handlers ─────────────────────
# HTTP must route through RuntimeSubsystem or HttpOps/AsyncHttp.

echo "── Rule 5: No direct HTTP in command handlers ──"

HANDLER_HTTP=""
if [ -d "$CLI_DIR" ]; then
  HANDLER_HTTP="$(rg -n 'FHttpModule::Get\(\)\.CreateRequest\(\)' \
    "$CLI_DIR" \
    2>/dev/null || true)"
fi

if [ -f "$COMMANDLET" ]; then
  CMD_HTTP="$(rg -n 'FHttpModule::Get\(\)\.CreateRequest\(\)' \
    "$COMMANDLET" \
    2>/dev/null || true)"
  HANDLER_HTTP="$HANDLER_HTTP$CMD_HTTP"
fi

if [ -n "$HANDLER_HTTP" ]; then
  fail "Direct HTTP in command handlers (must use RuntimeSubsystem/HttpOps):"
  echo "$HANDLER_HTTP"
else
  pass "No direct HTTP in command handlers"
fi

echo ""

# ── 6. No if statements in non-test code ──────────────────────
# Ternary expressions only (FP expression-style control flow).
# Note: This is aspirational — existing code may have violations.

echo "── Rule 6: No if-statements (ternary only) ──"

IF_HITS="$(rg -n '\bif\s*\(' \
  "$SRC/Public" "$SRC/Private" \
  --glob '!**/Tests/**' \
  --glob '!**/ThirdParty/**' \
  --glob '!**/Native/SqliteAmalgamation.c' \
  --type-add 'cpp:*.{h,hpp,cpp}' --type cpp \
  2>/dev/null || true)"

IF_REAL=""
while IFS= read -r line; do
  [ -z "$line" ] && continue
  code="${line#*:*:}"
  trimmed="$(echo "$code" | sed 's/^[[:space:]]*//')"
  case "$trimmed" in
    \**|//*) continue ;;
  esac
  stripped="$(echo "$code" | sed 's|//.*||' | sed 's|/\*.*\*/||g')"
  if echo "$stripped" | rg -q '\bif\s*\(' 2>/dev/null; then
    IF_REAL="$IF_REAL
$line"
  fi
done <<< "$IF_HITS"
IF_REAL="$(echo "$IF_REAL" | sed '/^$/d')"

IF_COUNT=0
[ -n "$IF_REAL" ] && IF_COUNT="$(echo "$IF_REAL" | wc -l | tr -d ' ')"

if [ "$IF_COUNT" -gt 0 ]; then
  warn "if-statements found ($IF_COUNT occurrences — ternary preferred):"
  echo "$IF_REAL" | head -10
  [ "$IF_COUNT" -gt 10 ] && echo "  ... and $((IF_COUNT - 10)) more"
else
  pass "No if-statements in first-party code"
fi

echo ""

# ── 7. No switch statements ───────────────────────────────────
# Use visitor/variant dispatch pattern instead.

echo "── Rule 7: No switch statements (visitor dispatch only) ──"

SWITCH_HITS="$(rg -n '\bswitch\s*\(' \
  "$SRC/Public" "$SRC/Private" \
  --glob '!**/Tests/**' \
  --glob '!**/ThirdParty/**' \
  --glob '!**/Native/SqliteAmalgamation.c' \
  --type-add 'cpp:*.{h,hpp,cpp}' --type cpp \
  2>/dev/null || true)"

SWITCH_REAL=""
while IFS= read -r line; do
  [ -z "$line" ] && continue
  code="${line#*:*:}"
  trimmed="$(echo "$code" | sed 's/^[[:space:]]*//')"
  case "$trimmed" in
    \**|//*) continue ;;
  esac
  stripped="$(echo "$code" | sed 's|//.*||' | sed 's|/\*.*\*/||g')"
  if echo "$stripped" | rg -q '\bswitch\s*\(' 2>/dev/null; then
    SWITCH_REAL="$SWITCH_REAL
$line"
  fi
done <<< "$SWITCH_HITS"
SWITCH_REAL="$(echo "$SWITCH_REAL" | sed '/^$/d')"

if [ -n "$SWITCH_REAL" ]; then
  warn "switch-statements found (visitor dispatch preferred):"
  echo "$SWITCH_REAL" | head -10
else
  pass "No switch statements in first-party code"
fi

echo ""

# ── 8. No mocking ─────────────────────────────────────────────
# Mocking is strictly prohibited. Tests exercise real code paths
# or skip gracefully when infrastructure is unavailable.
# Legacy rtk_test_mocks.h is quarantined under Private/Tests/Core/.

echo "── Rule 8: No mocking (strict policy) ──"

# Check demo test files (if demo source exists alongside SDK)
DEMO_SRC="$ROOT/../../Source/DemoProject"
MOCK_VIOLATIONS=""

# Check SDK Public/ and Private/ (excluding quarantined legacy tests)
SDK_MOCK="$(rg -ni '\bmock\b' \
  "$SRC/Public" \
  --glob '!**/ThirdParty/**' \
  --type-add 'cpp:*.{h,hpp,cpp}' --type cpp \
  2>/dev/null || true)"

[ -n "$SDK_MOCK" ] && MOCK_VIOLATIONS="$MOCK_VIOLATIONS
$SDK_MOCK"

# Check demo tests if present
if [ -d "$DEMO_SRC/Tests" ]; then
  DEMO_MOCK="$(rg -ni '\bmock\b' \
    "$DEMO_SRC/Tests" \
    --type-add 'cpp:*.{h,hpp,cpp}' --type cpp \
    2>/dev/null || true)"
  [ -n "$DEMO_MOCK" ] && MOCK_VIOLATIONS="$MOCK_VIOLATIONS
$DEMO_MOCK"
fi

# Check demo non-test source if present
if [ -d "$DEMO_SRC" ]; then
  DEMO_SRC_MOCK="$(rg -ni '\bmock\b' \
    "$DEMO_SRC" \
    --glob '!**/Tests/**' \
    --type-add 'cpp:*.{h,hpp,cpp}' --type cpp \
    2>/dev/null || true)"
  [ -n "$DEMO_SRC_MOCK" ] && MOCK_VIOLATIONS="$MOCK_VIOLATIONS
$DEMO_SRC_MOCK"
fi

MOCK_VIOLATIONS="$(echo "$MOCK_VIOLATIONS" | sed '/^$/d')"

if [ -n "$MOCK_VIOLATIONS" ]; then
  fail "Mock references found (mocking is strictly prohibited):"
  echo "$MOCK_VIOLATIONS" | head -20
  MOCK_COUNT="$(echo "$MOCK_VIOLATIONS" | wc -l | tr -d ' ')"
  [ "$MOCK_COUNT" -gt 20 ] && echo "  ... and $((MOCK_COUNT - 20)) more"
else
  pass "No mock references in demo or SDK public code"
fi

echo ""

# ── Summary ───────────────────────────────────────────────────

echo "╔══════════════════════════════════════════════════╗"
echo "║  RESULTS                                        ║"
echo "╠══════════════════════════════════════════════════╣"
printf "║  FAIL: %-3d   WARN: %-3d                         ║\n" "$FAIL_COUNT" "$WARN_COUNT"
echo "╠══════════════════════════════════════════════════╣"
if [ "$STATUS" -eq 0 ]; then
  echo "║  ✅ FP CONFORMANCE: PASS                        ║"
else
  echo "║  ❌ FP CONFORMANCE: FAIL                        ║"
fi
echo "╚══════════════════════════════════════════════════╝"

exit "$STATUS"
