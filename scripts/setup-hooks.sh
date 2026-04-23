#!/bin/sh
# Run once after cloning to activate git hooks and (optionally) link the demo repo.
git config core.hooksPath .githooks
chmod +x .githooks/post-commit .githooks/post-merge
echo "Hooks enabled."

if [ -n "$1" ]; then
  git config forboc.demoPath "$1"
  echo "Demo path set to: $1"
else
  echo ""
  echo "To auto-sync demo-ue-5.7 after each SDK commit/pull, run:"
  echo "  git config forboc.demoPath /path/to/demo-ue-5.7"
fi
