#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PUBLIC_DIR="$SCRIPT_DIR/../Source/ForbocAI_SDK/Public"
THUNK_DETAIL="$PUBLIC_DIR/Core/ThunkDetail.h"

echo "Checking protocol codec parity..."
python3 "$SCRIPT_DIR/check-codec-parity.py" "$THUNK_DETAIL"
