#!/usr/bin/env bash
set -euo pipefail

# Resolve the project root to the directory containing this script,
# regardless of where it is invoked from.
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# UE engine root. Override with UE_ROOT=/path/to/UE_5.7 to point at
# a non-default install. The default matches the macOS shared-install
# convention; Windows/Linux developers should set UE_ROOT explicitly.
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.7}"
UAT_PATH="$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"

PLUGIN_PATH="$PROJECT_ROOT/Plugins/ForbocAI_SDK/ForbocAI_SDK.uplugin"

if [[ ! -f "$PLUGIN_PATH" ]]; then
    echo "Plugin descriptor not found at: $PLUGIN_PATH" >&2
    exit 1
fi

PLUGIN_VERSION="$(awk -F'"' '/"VersionName"[[:space:]]*:/ { print $4; exit }' "$PLUGIN_PATH")"
if [[ -z "$PLUGIN_VERSION" ]]; then
    echo "Unable to read VersionName from: $PLUGIN_PATH" >&2
    exit 1
fi

OUTPUT_DIR="${OUTPUT_DIR:-$PROJECT_ROOT/dist_ForbocAI_SDK_v$PLUGIN_VERSION}"

if [[ ! -f "$UAT_PATH" ]]; then
    echo "RunUAT.sh not found at: $UAT_PATH" >&2
    echo "Set UE_ROOT to your UE 5.7 engine install (the directory containing Engine/)." >&2
    exit 1
fi

echo "Building ForbocAI SDK Plugin..."
echo "  PROJECT_ROOT: $PROJECT_ROOT"
echo "  UE_ROOT:      $UE_ROOT"
echo "  VERSION:      $PLUGIN_VERSION"
echo "  OUTPUT_DIR:   $OUTPUT_DIR"

"$UAT_PATH" BuildPlugin -Plugin="$PLUGIN_PATH" -Package="$OUTPUT_DIR" -Rocket

echo "Build complete. Output in $OUTPUT_DIR"
