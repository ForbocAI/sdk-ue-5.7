import re
with open("Plugins/ForbocAI_SDK/scripts/verify-ue-parity.sh", "r") as f:
    content = f.read()

old_args = '''QUICK_MODE=0
for arg in "$@"; do
  if [ "$arg" = "--quick" ]; then
    QUICK_MODE=1
  fi
done'''

new_args = '''QUICK_MODE=0
DEMO_ROOT=""
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
done'''
content = content.replace(old_args, new_args)

old_run_check = '''  if [ -f "$script" ]; then
    if case "$script" in
      *.py) python3 "$script" 2>&1 ;;
      *) bash "$script" 2>&1 ;;
    esac; then'''

new_run_check = '''  if [ -f "$script" ]; then
    local args=()
    [ -n "$DEMO_ROOT" ] && args+=("--demo-root" "$DEMO_ROOT")
    
    if case "$script" in
      *.py) python3 "$script" "${args[@]}" 2>&1 ;;
      *) bash "$script" "${args[@]}" 2>&1 ;;
    esac; then'''
content = content.replace(old_run_check, new_run_check)

with open("Plugins/ForbocAI_SDK/scripts/verify-ue-parity.sh", "w") as f:
    f.write(content)

