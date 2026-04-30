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
#   bash scripts/verify-ue-parity.sh --quick  # Skip slow checks (conformance only)
#
# Run from the SDK plugin root:
#   cd Plugins/ForbocAI_SDK && bash scripts/verify-ue-parity.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
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
  TOTAL=$((TOTAL + 1))

  echo ""
  echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
  echo -e "${CYAN}[$TOTAL] $name${NC}"
  echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

  if [ -f "$script" ]; then
    if case "$script" in
      *.py) python3 "$script" 2>&1 ;;
      *) bash "$script" 2>&1 ;;
    esac; then
      echo -e "${GREEN}✓ $name — PASSED${NC}"
    else
      echo -e "${RED}✗ $name — FAILED${NC}"
      FAILURES=$((FAILURES + 1))
    fi
  else
    echo -e "${YELLOW}⚠ $name — SKIPPED (script not found: $script)${NC}"
  fi
}

echo ""
echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║          ForbocAI UE SDK — Parity Verification Suite         ║"
echo "╠═══════════════════════════════════════════════════════════════╣"
echo "║  Running all conformance and parity checks...                ║"
echo "╚═══════════════════════════════════════════════════════════════╝"

# ── Phase 1: Structural Conformance ──
run_check "UE Conformance (structural rules)" \
  "$SCRIPT_DIR/check-ue-conformance.sh"

# ── Phase 2: FP Conformance ──
run_check "FP Conformance (no loops, no classes, no mutation)" \
  "$SCRIPT_DIR/check-ue-fp-conformance.sh"

# ── Phase 3: Command Surface Guardrails ──
run_check "Thin-Wrapper Guardrails (command surface rules)" \
  "$SCRIPT_DIR/check-thin-wrapper-guardrails.sh"

# ── Phase 4: Product Boundary ──
run_check "Product Boundary (game-agnostic audit)" \
  "$SCRIPT_DIR/check-product-boundary.sh"

# ── Phase 5: API Contract Parity ──
run_check "Canonical Contract Parity (API test-game contract)" \
  "$SCRIPT_DIR/check-api-contract-parity.py"

# ── Summary ──
echo ""
echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║                    VERIFICATION SUMMARY                      ║"
echo "╠═══════════════════════════════════════════════════════════════╣"

PASSED=$((TOTAL - FAILURES))

if [ "$FAILURES" -eq 0 ]; then
  echo -e "║  ${GREEN}All $TOTAL checks passed.${NC}                                      ║"
  echo "║                                                               ║"
  echo -e "║  ${GREEN}✓ Ready to commit.${NC}                                            ║"
else
  echo -e "║  ${RED}$FAILURES of $TOTAL checks failed.${NC}                                       ║"
  echo "║                                                               ║"
  echo -e "║  ${RED}✗ Fix violations before committing.${NC}                            ║"
fi

echo "╚═══════════════════════════════════════════════════════════════╝"
echo ""

# Checklist summary
echo "Parity Verification Checklist:"
echo "  [$([ $FAILURES -eq 0 ] && echo 'x' || echo ' ')] UE conformance (structural)"
echo "  [$([ $FAILURES -eq 0 ] && echo 'x' || echo ' ')] FP conformance (immutability)"
echo "  [$([ $FAILURES -eq 0 ] && echo 'x' || echo ' ')] Thin-wrapper guardrails"
echo "  [$([ $FAILURES -eq 0 ] && echo 'x' || echo ' ')] Product boundary audit"
echo "  [$([ $FAILURES -eq 0 ] && echo 'x' || echo ' ')] Canonical-contract parity"
echo "  [ ] Focused RunGame automation (requires editor build)"
echo "  [ ] Runtime-readiness verification (requires API connectivity)"
echo ""

exit $FAILURES
