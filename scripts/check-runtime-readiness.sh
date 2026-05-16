#!/usr/bin/env bash
set -euo pipefail

if [ -z "${FORBOC_RUNTIME_URL:-}" ]; then
  echo "[skip] FORBOC_RUNTIME_URL is not set. Skipping runtime-readiness check."
  exit 0
fi

URL="${FORBOC_RUNTIME_URL%/}/status"
echo "Checking API status at $URL..."

STATUS_CODE=$(curl -s -o /dev/null -w "%{http_code}" "$URL" || true)

if [ "$STATUS_CODE" -eq 200 ]; then
  echo "[ok] Runtime API is reachable and healthy."
  exit 0
else
  echo "[fail] Runtime API check failed (HTTP $STATUS_CODE)."
  exit 1
fi
