import re
with open("Plugins/ForbocAI_SDK/scripts/check-product-boundary.sh", "r") as f:
    content = f.read()

rule7 = '''# ── Rule 7: No Simulated Coverage Claims ──
echo "[Rule 7] No simulated coverage claims..."
SIMULATED_TERMS="simulated coverage|simulated mode|mock coverage|simulated test"
HITS=$(rg -ci "$SIMULATED_TERMS" "${SRC_DIRS[@]}" 2>/dev/null || true)
if [ -n "$HITS" ]; then
  echo "  ✗ Simulated coverage claims found:"
  rg -ni "$SIMULATED_TERMS" "${SRC_DIRS[@]}" 2>/dev/null || true
  VIOLATIONS=$((VIOLATIONS + 1))
else
  echo "  ✓ No simulated coverage claims."
fi
echo ""

# ── Summary ──'''

content = content.replace("# ── Summary ──", rule7)

with open("Plugins/ForbocAI_SDK/scripts/check-product-boundary.sh", "w") as f:
    f.write(content)

