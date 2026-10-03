#!/usr/bin/env bash
# Lints, validates and compiles the firmware. Stops at the first failure.
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

fail() {
  echo "check: $*" >&2
  exit 1
}

for tool in esphome yamllint; do
  command -v "$tool" > /dev/null 2>&1 ||
    fail "$tool not found on PATH. Activate the venv: . .venv/bin/activate"
done

[[ -f secrets.yaml ]] ||
  fail "secrets.yaml is missing. Copy the example: cp secrets.example.yaml secrets.yaml"

if [[ -n $(git ls-files --cached -- secrets.yaml) ]]; then
  fail "secrets.yaml is staged in git. Unstage it: git rm --cached secrets.yaml"
fi

echo "check: yamllint -s ."
yamllint -s .
echo "check: esphome config windmill.yaml"
esphome config windmill.yaml
echo "check: esphome compile windmill.yaml"
esphome compile windmill.yaml
echo "check: all passed"
