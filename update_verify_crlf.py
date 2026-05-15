import re

with open("Plugins/ForbocAI_SDK/scripts/verify-ue-parity.sh", "r") as f:
    content = f.read()

preflight = '''echo ""
echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║          ForbocAI UE SDK — Parity Verification Suite         ║"
echo "╠═══════════════════════════════════════════════════════════════╣"
echo "║  Running all conformance and parity checks...                ║"
echo "╚═══════════════════════════════════════════════════════════════╝"

# ── Pre-flight: Line Ending Normalization ──
CRLF_FILES=$(find "$SCRIPT_DIR" -type f -name "*.sh" -exec grep -l $'\\r' {} + 2>/dev/null || true)
if [ -n "$CRLF_FILES" ]; then
  echo -e "${YELLOW}⚠ Found CRLF line endings in shell scripts. Normalizing to LF...${NC}"
  for f in $CRLF_FILES; do
    sed -i 's/\\r$//' "$f"
    echo "  Normalized: $(basename "$f")"
  done
fi
'''

old_banner = '''echo ""
echo "╔═══════════════════════════════════════════════════════════════╗"
echo "║          ForbocAI UE SDK — Parity Verification Suite         ║"
echo "╠═══════════════════════════════════════════════════════════════╣"
echo "║  Running all conformance and parity checks...                ║"
echo "╚═══════════════════════════════════════════════════════════════╝"'''

content = content.replace(old_banner, preflight)

with open("Plugins/ForbocAI_SDK/scripts/verify-ue-parity.sh", "w") as f:
    f.write(content)
