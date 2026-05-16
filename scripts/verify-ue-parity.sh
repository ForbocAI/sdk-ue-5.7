#!/usr/bin/env bash
# verify-ue-parity.sh
# Unified UE parity verification — runs all conformance checks in one pass.
#
# This is the single script to run before any commit or PR.
# It replaces manual audits with automated enforcement.
#
# Checks (in order):
#   1. UE conformance (check-ue-conformance.sh)
#   2. FP conformance (check-ue-fp-conformance.sh)
#   3. Thin-wrapper guardrails (check-thin-wrapper-guardrails.sh)
#   4. Product boundary audit (check-product-boundary.sh)
#   5. API contract parity (check-api-contract-parity.py)
#
# Exit codes:
#   0 = all checks passed
#   1 = one or more checks failed
#
# Usage:
#   bash scripts/verify-ue-parity.sh          # Run all checks
#   bash scripts/verify-ue-parity.sh --quick  # Skip slow network-backed parity only
#
# Run from the SDK plugin root:
#   bash scripts/verify-ue-parity.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
QUICK_MODE=0
DEMO_ROOT=""
SKIPPED=0
UE_CONFORMANCE_STATUS="skipped"
FP_CONFORMANCE_STATUS="skipped"
THIN_WRAPPER_STATUS="skipped"
PRODUCT_BOUNDARY_STATUS="skipped"
CONTRACT_PARITY_STATUS="skipped"
HANDLER_CLASSIFICATION_STATUS="skipped"
TEST_QUALITY_STATUS="skipped"
RUNTIME_READINESS_STATUS="skipped"
while [[ $# -gt 0 ]]; do
  case $1 in
    --quick)
      QUICK_MODE=1
      shift
      ;;
    --demo-root)
      DEMO_ROOT="$2"
      shift 2
      ;;
    *)
      shift
      ;;
  esac
done

FAILURES=0
TOTAL=0

# Colors (if terminal supports them)
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

run_check() {
  local name="$1"
  local script="$2"
  local result_var="$3"
  local required="${4:-1}"
  local check_status="skipped"
  TOTAL=$((TOTAL + 1))

  echo ""
  echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
  echo -e "${CYAN}[$TOTAL] $name${NC}"
  echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

  if [ -f "$script" ]; then
    local args=()
    [ -n "$DEMO_ROOT" ] && args+=("--demo-root" "$DEMO_ROOT")

    if case "$script" in
      *.py) python3 "$script" "${args[@]}" 2>&1 ;;
      *) bash "$script" "${args[@]}" 2>&1 ;;
    esac; then
      echo -e "${GREEN}✓ $name — PASSED${NC}"
      check_status="passed"
    else
      echo -e "${RED}✗ $name — FAILED${NC}"
      FAILURES=$((FAILURES + 1))
      check_status="failed"
    fi
  elif [ "$required" -eq 1 ]; then
    echo -e "${RED}✗ $name — FAILED (required script not found: $script)${NC}"
    FAILURES=$((FAILURES + 1))
    check_status="failed"
  else
    echo -e "${YELLOW}⚠ $name — SKIPPED (script not found: $script)${NC}"
    SKIPPED=$((SKIPPED + 1))
  fi

  printf -v "$result_var" '%s' "$check_status"
}

normalize_script_line_endings() {
  local script="$1"
  python3 - "$script" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
raw = path.read_bytes()
normalized = raw.replace(b"\r\n", b"\n").replace(b"\r", b"\n")
if normalized != raw:
    path.write_bytes(normalized)
    print(path.name)
PY
}

normalize_shell_scripts() {
  local normalized_scripts=""
  local script
  local changed
  for script in "$SCRIPT_DIR"/*.sh; do
    [ -f "$script" ] || continue
    changed="$(normalize_script_line_endings "$script")"
    [ -n "$changed" ] && normalized_scripts="${normalized_scripts}
$changed"
  done

  normalized_scripts="$(echo "$normalized_scripts" | sed '/^$/d')"
  if [ -n "$normalized_scripts" ]; then
    echo -e "${YELLOW}⚠ Found CRLF line endings in shell scripts. Normalizing to LF...${NC}"
    while IFS= read -r script_name; do
      [ -z "$script_name" ] && continue
      echo "  Normalized: $script_name"
    done <<< "$normalized_scripts"
  fi
}

echo ""
echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║          ForbocAI UE SDK — Parity Verification Suite         ║"
echo "╠═══════════════════════════════════════════════════════════════╣"
echo "║  Running all conformance and parity checks...                ║"
echo "╚═══════════════════════════════════════════════════════════════╝"

# ── Pre-flight: Line Ending Normalization ──
normalize_shell_scripts


# ── Phase 1: Structural Conformance ──
run_check "UE Conformance (structural rules)" \
  "$SCRIPT_DIR/check-ue-conformance.sh" UE_CONFORMANCE_STATUS

# ── Phase 2: FP Conformance ──
run_check "FP Conformance (no loops, no classes, no mutation)" \
  "$SCRIPT_DIR/check-ue-fp-conformance.sh" FP_CONFORMANCE_STATUS

# ── Phase 3: Command Surface Guardrails ──
run_check "Thin-Wrapper Guardrails (command surface rules)" \
  "$SCRIPT_DIR/check-thin-wrapper-guardrails.sh" THIN_WRAPPER_STATUS

# ── Phase 4: Product Boundary ──
run_check "Product Boundary (game-agnostic audit)" \
  "$SCRIPT_DIR/check-product-boundary.sh" PRODUCT_BOUNDARY_STATUS

# ── Phase 5: API Contract Parity ──
if [ "$QUICK_MODE" -eq 1 ]; then
  TOTAL=$((TOTAL + 1))
  SKIPPED=$((SKIPPED + 1))
  echo -e "\n${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
  echo -e "${CYAN}[SKIPPED] Canonical Contract Parity (API test-game contract)${NC}"
  echo -e "${YELLOW}Skipped in --quick mode${NC}"
else
  run_check "Canonical Contract Parity (API test-game contract)" \
    "$SCRIPT_DIR/check-api-contract-parity.py" CONTRACT_PARITY_STATUS
fi

if [ "$QUICK_MODE" -eq 0 ] && [ "$CONTRACT_PARITY_STATUS" = "skipped" ]; then
  echo -e "${RED}✗ Canonical Contract Parity (API test-game contract) — FAILED${NC}"
  echo -e "${RED}Required contract-parity execution did not run.${NC}"
  FAILURES=$((FAILURES + 1))
  CONTRACT_PARITY_STATUS="failed"
fi

# ── Phase 6: Handler Classification Parity ──
run_check "Handler Classification Drift (UE/TS parity and contract adherence)" \
  "$SCRIPT_DIR/check-handler-classification.py" HANDLER_CLASSIFICATION_STATUS

# ── Phase 7: Test Quality Audit ──
run_check "Test Quality Audit (no simulated tests or no-op assertions)" \
  "$SCRIPT_DIR/check-test-quality.sh" TEST_QUALITY_STATUS

# ── Phase 8: Runtime-Readiness ──
if [ "$QUICK_MODE" -eq 1 ]; then
  TOTAL=$((TOTAL + 1))
  SKIPPED=$((SKIPPED + 1))
  echo -e "\n${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
  echo -e "${CYAN}[SKIPPED] Runtime-readiness verification (requires API connectivity)${NC}"
  echo -e "${YELLOW}Skipped in --quick mode${NC}"
  RUNTIME_READINESS_STATUS="skipped"
else
  run_check "Runtime-readiness verification (requires API connectivity)" \
    "$SCRIPT_DIR/check-runtime-readiness.sh" RUNTIME_READINESS_STATUS
fi

# ── Summary ──
echo ""
echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║                    VERIFICATION SUMMARY                      ║"
echo "╠═══════════════════════════════════════════════════════════════╣"

PASSED=$((TOTAL - FAILURES - SKIPPED))

if [ "$FAILURES" -eq 0 ]; then
  if [ "$SKIPPED" -eq 0 ]; then
    echo -e "║  ${GREEN}$PASSED of $TOTAL checks passed.${NC}                                 ║"
  else
    echo -e "║  ${GREEN}$PASSED passed, $SKIPPED skipped, $TOTAL total.${NC}                      ║"
  fi
  echo "║                                                               ║"
  echo -e "║  ${GREEN}✓ Ready to commit.${NC}                                            ║"
else
  echo -e "║  ${RED}$FAILURES failed, $PASSED passed, $SKIPPED skipped.${NC}                     ║"
  echo "║                                                               ║"
  echo -e "║  ${RED}✗ Fix violations before committing.${NC}                            ║"
fi

echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""

# Checklist summary
mark_for_status() {
  case "$1" in
    passed) echo "x" ;;
    *) echo " " ;;
  esac
}

echo "Parity Verification Checklist:"
echo "  [$(mark_for_status "$UE_CONFORMANCE_STATUS")] UE conformance (structural)"
echo "  [$(mark_for_status "$FP_CONFORMANCE_STATUS")] FP conformance (immutability)"
echo "  [$(mark_for_status "$THIN_WRAPPER_STATUS")] Thin-wrapper guardrails"
echo "  [$(mark_for_status "$PRODUCT_BOUNDARY_STATUS")] Product boundary audit"
if [ "$QUICK_MODE" -eq 1 ]; then
  echo "  [ ] Canonical-contract parity (skipped by --quick)"
else
  echo "  [$(mark_for_status "$CONTRACT_PARITY_STATUS")] Canonical-contract parity"
fi
echo "  [$(mark_for_status "$HANDLER_CLASSIFICATION_STATUS")] Handler classification drift"
echo "  [$(mark_for_status "$TEST_QUALITY_STATUS")] Test quality (real coverage)"
echo "  [ ] Protocol codec parity"
echo "  [ ] Focused RunGame automation (requires editor build)"
echo "  [ ] Runtime-readiness verification (requires API connectivity)"
echo ""

exit $FAILURES
RL not set)"
else
  echo "  [$(mark_for_status "$RUNTIME_READINESS_STATUS")] Runtime-readiness verification (requires API connectivity)"
fi
echo ""

exit $FAILURES
