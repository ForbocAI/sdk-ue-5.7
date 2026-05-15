import re
with open("Plugins/ForbocAI_SDK/scripts/check-product-boundary.sh", "r") as f:
    content = f.read()

setup = '''PLUGIN_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$PLUGIN_ROOT/Source/ForbocAI_SDK"
PUBLIC="$SRC/Public"

VIOLATIONS=0'''

new_setup = '''PLUGIN_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$PLUGIN_ROOT/Source/ForbocAI_SDK"
PUBLIC="$SRC/Public"

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
  DEMO_SRC="$PLUGIN_ROOT/../../Source/DemoProject"
fi

SRC_DIRS=("$PUBLIC")
[ -d "$DEMO_SRC" ] && SRC_DIRS+=("$DEMO_SRC")

VIOLATIONS=0'''

content = content.replace(setup, new_setup)
content = content.replace('"$PUBLIC"', '"${SRC_DIRS[@]}"')
content = content.replace('GENERIC_DIRS=("${SRC_DIRS[@]}"/CLI', 'GENERIC_DIRS=("$PUBLIC/CLI"')
content = content.replace('GENERIC_DIRS=("${SRC_DIRS[@]}"/Protocol', 'GENERIC_DIRS=("$PUBLIC/Protocol"')
content = content.replace('GENERIC_DIRS=("${SRC_DIRS[@]}"/Core', 'GENERIC_DIRS=("$PUBLIC/Core"')
content = content.replace('GENERIC_DIRS=("$PUBLIC/CLI" "$PUBLIC/Protocol" "$PUBLIC/Core")', 'GENERIC_DIRS=("$PUBLIC/CLI" "$PUBLIC/Protocol" "$PUBLIC/Core")\n[ -d "$DEMO_SRC" ] && GENERIC_DIRS+=("$DEMO_SRC")')

with open("Plugins/ForbocAI_SDK/scripts/check-product-boundary.sh", "w") as f:
    f.write(content)

