import re
with open("Plugins/ForbocAI_SDK/scripts/check-ue-fp-conformance.sh", "r") as f:
    content = f.read()

old_setup = '''ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="$ROOT/Source/ForbocAI_SDK"
STATUS=0'''

new_setup = '''ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="$ROOT/Source/ForbocAI_SDK"
STATUS=0
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
  DEMO_SRC="$DEMO_ROOT/Source/DemoProject"
else
  DEMO_SRC="$ROOT/../../Source/DemoProject"
fi'''

content = content.replace(old_setup, new_setup)

old_demo_src_decl = '''# Check demo test files (if demo source exists alongside SDK)
DEMO_SRC="$ROOT/../../Source/DemoProject"
MOCK_VIOLATIONS=""'''

new_demo_src_decl = '''# Check demo test files (if demo source exists alongside SDK)
MOCK_VIOLATIONS=""'''

content = content.replace(old_demo_src_decl, new_demo_src_decl)

with open("Plugins/ForbocAI_SDK/scripts/check-ue-fp-conformance.sh", "w") as f:
    f.write(content)
