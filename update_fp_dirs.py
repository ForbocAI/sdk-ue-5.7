import re
with open("Plugins/ForbocAI_SDK/scripts/check-ue-fp-conformance.sh", "r") as f:
    content = f.read()

src_dirs_decl = '''if [ -n "$DEMO_ROOT" ]; then
  DEMO_SRC="$DEMO_ROOT/Source/DemoProject"
else
  DEMO_SRC="$ROOT/../../Source/DemoProject"
fi'''

new_src_dirs_decl = '''if [ -n "$DEMO_ROOT" ]; then
  DEMO_SRC="$DEMO_ROOT/Source/DemoProject"
else
  DEMO_SRC="$ROOT/../../Source/DemoProject"
fi

SRC_DIRS=("$SRC/Public" "$SRC/Private")
[ -d "$DEMO_SRC" ] && SRC_DIRS+=("$DEMO_SRC")'''

content = content.replace(src_dirs_decl, new_src_dirs_decl)

# Replace "$SRC/Public" "$SRC/Private" with "${SRC_DIRS[@]}"
content = content.replace('"\\$SRC/Public" "$SRC/Private"', '"${SRC_DIRS[@]}"')

with open("Plugins/ForbocAI_SDK/scripts/check-ue-fp-conformance.sh", "w") as f:
    f.write(content)
