#!/usr/bin/env bash
# check-test-game-executor-boundary.sh
#
# Coverage-script guard for the UE test-game executor boundary.
#
# This check fails if any code re-introduces an in-process command executor
# under the test-game surface. The legacy `TestGameLib.h` header used to
# host `ExecuteForbocAICommand`, a shadow CLI that bypassed CLIOps; that
# entire header is retired (see ForbocAI/sdk-ue-5.7#5). All UE test-game
# command execution must flow through `TestGame::CommandSurface`
# (`TestGame/TestGameCommandSurface.h`), which delegates to the canonical
# `CLIOps::DispatchCommand`.
#
# Rules enforced:
#   1. No file may include the retired `TestGame/TestGameLib.h` header.
#   2. No file may reintroduce `TestGameLib.h` (the file itself).
#   3. Integration tests under `Source/ForbocAI_SDK/Private/Tests/` must
#      not name a function `ExecuteForbocAICommand` or shadow the
#      canonical command surface with an alternate executor symbol.
#
# Usage:
#   bash scripts/check-test-game-executor-boundary.sh
#
# Exit codes:
#   0 — all rules satisfied
#   1 — at least one rule failed
#   2 — required tool missing
#
# This script is intentionally shell-portable (POSIX-friendly bash + rg)
# so it runs on Windows (Git Bash / WSL) and macOS without extra setup.

set -euo pipefail

if ! command -v rg >/dev/null 2>&1; then
  echo "[fail] ripgrep (rg) is required but not found on PATH." >&2
  echo "       Install ripgrep before running the executor-boundary guard." >&2
  exit 2
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="$ROOT/Source/ForbocAI_SDK"
STATUS=0

normalize_crlf() {
  tr -d '\r'
}

echo "[check] Test-game executor boundary guard"

# 1) No #include of the retired TestGameLib.h header.
LIB_INCLUDES="$(rg -n '#include[[:space:]]+"[^"]*TestGame/TestGameLib\.h"' \
  "$SRC/Public" "$SRC/Private" \
  2>/dev/null | normalize_crlf || true)"
if [ -n "$LIB_INCLUDES" ]; then
  echo "[fail] Files still include the retired TestGame/TestGameLib.h:"
  echo "$LIB_INCLUDES"
  echo "       Include TestGame/TestGameRuntime.h for runtime-URL helpers" >&2
  echo "       or TestGame/TestGameGridRender.h for ASCII rendering." >&2
  echo "       All command execution must use TestGame::CommandSurface." >&2
  STATUS=1
else
  echo "[ok] No TestGame/TestGameLib.h includes"
fi

# 2) The retired header itself must not be re-added.
if [ -f "$SRC/Public/TestGame/TestGameLib.h" ]; then
  echo "[fail] TestGame/TestGameLib.h has been re-added. The legacy" >&2
  echo "       in-process executor surface is retired — split helpers" >&2
  echo "       into TestGameRuntime.h / TestGameGridRender.h instead." >&2
  STATUS=1
else
  echo "[ok] Legacy TestGameLib.h is absent"
fi

# 3) Integration tests must not reintroduce an executor symbol.
TEST_DIR="$SRC/Private/Tests"
if [ -d "$TEST_DIR" ]; then
  EXEC_DECLS="$(rg -n '\bExecuteForbocAICommand\b' \
    "$TEST_DIR" \
    2>/dev/null | normalize_crlf || true)"
  if [ -n "$EXEC_DECLS" ]; then
    echo "[fail] Integration tests reference the retired ExecuteForbocAICommand entrypoint:"
    echo "$EXEC_DECLS"
    echo "       Drive commands through TestGame::CommandSurface::Execute" >&2
    echo "       (with a CommandSurface::FCommandExecutor injector for stubs)." >&2
    STATUS=1
  else
    echo "[ok] Integration tests do not reintroduce ExecuteForbocAICommand"
  fi
fi

echo "[done] Test-game executor boundary check complete (exit $STATUS)"
exit "$STATUS"
