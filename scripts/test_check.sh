#!/usr/bin/env bash
# Tests for scripts/check.sh and the repository's secrets and node settings.
# Run it with the venv active: the tool-version and node-config tests use the
# real esphome and yamllint. The refusal tests run check.sh in a temporary git
# repository with stub tools that record each call.
set -euo pipefail

repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

passed=0
failed=0

pass() { echo "PASS: $1"; passed=$((passed + 1)); }
fail() { echo "FAIL: $1"; echo "      $2"; failed=$((failed + 1)); }

# The system PATH without any directory that holds a real esphome or yamllint.
clean_path=""
IFS=: read -r -a path_dirs <<< "$PATH"
for dir in "${path_dirs[@]}"; do
  [[ -x "$dir/esphome" || -x "$dir/yamllint" ]] && continue
  clean_path="${clean_path:+$clean_path:}$dir"
done

# make_stubs DIR TOOL... writes a stub for each TOOL that logs its arguments.
make_stubs() {
  local dir=$1 tool
  shift
  mkdir -p "$dir"
  for tool in "$@"; do
    printf '#!/usr/bin/env bash\necho "%s $*" >> "%s"\n' "$tool" "$work/calls" > "$dir/$tool"
    chmod +x "$dir/$tool"
  done
}

# new_repo [with-secrets] creates a fresh git repository that holds the
# check script and the repository files it reads. Prints its path.
new_repo() {
  local dir
  dir=$(mktemp -d "$work/repo.XXXX")
  mkdir -p "$dir/scripts"
  cp -p "$repo/scripts/check.sh" "$dir/scripts/"
  cp "$repo/windmill.yaml" "$repo/secrets.example.yaml" "$repo/.gitignore" "$dir/"
  cp -r "$repo/packages" "$dir/"
  git -C "$dir" init -q
  if [[ ${1:-} == with-secrets ]]; then
    cp "$dir/secrets.example.yaml" "$dir/secrets.yaml"
  fi
  echo "$dir"
}

# run_check REPO STUB_TOOLS... runs check.sh with only the named stub tools on
# PATH. Sets $status, $output and $calls.
run_check() {
  local dir=$1 bin
  shift
  bin=$(mktemp -d "$work/bin.XXXX")
  make_stubs "$bin" "$@"
  : > "$work/calls"
  status=0
  output=$(PATH="$bin:$clean_path" "$dir/scripts/check.sh" 2>&1) || status=$?
  calls=$(cat "$work/calls")
}

expect_refusal() {
  local name=$1 pattern=$2
  if [[ $status -eq 0 ]]; then
    fail "$name" "exit status 0, expected non-zero. Output: $output"
  elif ! grep -Eqi -- "$pattern" <<< "$output"; then
    fail "$name" "message does not match /$pattern/. Output: $output"
  elif [[ -n $calls ]]; then
    fail "$name" "tools ran: $calls"
  else
    pass "$name"
  fi
}

test_tool_versions() {
  local esphome_version yamllint_version
  esphome_version=$(esphome version 2>&1 || true)
  yamllint_version=$(yamllint --version 2>&1 || true)
  if [[ $esphome_version == "Version: 2026.9.1" && $yamllint_version == "yamllint 1.38.0" ]]; then
    pass "pinned tool versions are installed"
  else
    fail "pinned tool versions are installed" \
      "esphome: '$esphome_version', yamllint: '$yamllint_version' (is the venv active?)"
  fi
}

test_full_pass_with_stubs() {
  local dir expected
  dir=$(new_repo with-secrets)
  run_check "$dir" esphome yamllint
  expected=$'yamllint -s .\nesphome config windmill.yaml\nesphome compile windmill.yaml'
  if [[ $status -ne 0 ]]; then
    fail "check runs lint, config and compile" "exit status $status. Output: $output"
  elif [[ $calls != "$expected" ]]; then
    fail "check runs lint, config and compile" "calls were: $calls"
  elif ! grep -qi "bench not included" <<< "$output"; then
    fail "check runs lint, config and compile" "no 'bench not included' note. Output: $output"
  else
    pass "check runs lint, config and compile"
  fi
}

test_refuses_missing_secrets() {
  run_check "$(new_repo)" esphome yamllint
  expect_refusal "check refuses a missing secrets file" "secrets\.yaml.*missing"
}

test_refuses_staged_secrets() {
  local dir
  dir=$(new_repo with-secrets)
  git -C "$dir" add -f secrets.yaml
  run_check "$dir" esphome yamllint
  expect_refusal "check refuses a staged secrets file" "secrets\.yaml.*staged"
}

test_refuses_missing_tools() {
  local dir
  dir=$(new_repo with-secrets)
  run_check "$dir" yamllint
  expect_refusal "check refuses when esphome is missing" "esphome.*not found"
  run_check "$dir" esphome
  expect_refusal "check refuses when yamllint is missing" "yamllint.*not found"
  run_check "$dir"
  expect_refusal "check refuses when both tools are missing" "(esphome|yamllint).*not found"
}

test_secrets_kept_out_of_git() {
  local history
  history=$(git -C "$repo" log --all --oneline -- secrets.yaml)
  if ! git -C "$repo" check-ignore -q secrets.yaml; then
    fail "secrets.yaml is ignored by git" "git check-ignore does not match secrets.yaml"
  elif [[ -n $history ]]; then
    fail "secrets.yaml is in no commit" "commits: $history"
  else
    pass "secrets.yaml is ignored and in no commit"
  fi
}

test_example_secrets_are_placeholders() {
  local bad
  bad=$(grep -Ev '^\s*(#|$)' "$repo/secrets.example.yaml" |
    grep -Ev '^(wifi_ssid|wifi_password|ap_password|ota_password): "example-[a-z0-9-]{3,}"$' |
    grep -Ev '^api_key: "[A-Za-z0-9+/]{43}="$' || true)
  local key api_key
  for key in wifi_ssid wifi_password ap_password ota_password api_key; do
    grep -q "^$key: " "$repo/secrets.example.yaml" || bad+=$'\n'"missing key $key"
  done
  # ESPHome rejects an all-zero key, so the placeholder key decodes to readable text.
  api_key=$(sed -n 's/^api_key: "\(.*\)"$/\1/p' "$repo/secrets.example.yaml")
  if [[ $(base64 -d <<< "$api_key" 2> /dev/null) != "example-api-key-placeholder-only" ]]; then
    bad+=$'\n'"api_key does not decode to the placeholder text"
  fi
  if [[ -n $bad ]]; then
    fail "example secrets hold only placeholders" "unexpected lines: $bad"
  else
    pass "example secrets hold only placeholders"
  fi
}

# section NAME prints the top-level block NAME from the validated config.
section() {
  awk -v key="$1:" '$0 == key { on = 1; next } /^[^ ]/ { on = 0 } on' <<< "$config"
}

# expect_setting NAME BLOCK PATTERN checks that BLOCK contains PATTERN.
expect_setting() {
  if grep -Eq -- "$3" <<< "$2"; then
    pass "$1"
  else
    fail "$1" "no line matches /$3/ in: $2"
  fi
}

# list_item BLOCK PATTERN prints the list entry in BLOCK that holds a line
# matching PATTERN.
list_item() {
  awk -v pat="$2" '
    /^  - / { if (hit) exit; item = "" }
    { item = item $0 "\n" }
    $0 ~ pat { hit = 1 }
    END { if (hit) printf "%s", item }' <<< "$1"
}

# pin_number BLOCK PIN prints the GPIO number of PIN (for example pin_a) in BLOCK.
pin_number() {
  awk -v key="$2:" '
    $1 == key { on = 1; next }
    on && $1 == "number:" { print $2; exit }
    on && /^    [^ ]/ { exit }' <<< "$1"
}

# load_config validates windmill.yaml and keeps the output in $config. It
# uses the example secrets when secrets.yaml is missing.
load_config() {
  local secrets_backup=""
  if [[ ! -f "$repo/secrets.yaml" ]]; then
    secrets_backup=none
    cp "$repo/secrets.example.yaml" "$repo/secrets.yaml"
  fi
  config_status=0
  config=$(cd "$repo" && esphome config windmill.yaml 2> /dev/null) || config_status=$?
  # ESPHome wraps the access point name in escaped terminal codes that hide it.
  config=${config//\\033\[8m/}
  config=${config//\\033\[28m/}
  [[ $secrets_backup == none ]] && rm -f "$repo/secrets.yaml"
  if [[ $config_status -ne 0 ]]; then
    fail "node config validates" "esphome config exited $config_status"
  fi
}

test_node_settings() {
  [[ $config_status -ne 0 ]] && return
  local logger esp32 api ota wifi
  logger=$(section logger)
  esp32=$(section esp32)
  api=$(section api)
  ota=$(section ota)
  wifi=$(section wifi)
  expect_setting "serial logging is off" "$logger" '^  baud_rate: 0$'
  expect_setting "log level is INFO" "$logger" '^  level: INFO$'
  expect_setting "ESP-IDF log level is NONE" "$esp32" '^    log_level: NONE$'
  expect_setting "API key comes from secrets" "$api" "key: !secret '?api_key'?$"
  expect_setting "update password comes from secrets" "$ota" "password: !secret '?ota_password'?$"
  expect_setting "access point password comes from secrets" "$wifi" "^    password: !secret '?ap_password'?$"
  expect_setting "access point is Windmill Fallback" "$wifi" "^    ssid: '?Windmill Fallback'?$"
}

test_sails_settings() {
  [[ $config_status -ne 0 ]] && return
  local substitutions stepper turn package pair letter number actual
  substitutions=$(section substitutions)
  stepper=$(list_item "$(section stepper)" '^    id: mill_sails$')
  turn=$(list_item "$(section switch)" "^    name: '?Sails Turning'?$")
  package=$(cat "$repo/packages/mill_sails.yaml" 2> /dev/null || true)
  for pair in a:0 b:1 c:3 d:4; do
    letter=${pair%:*}
    number=${pair#*:}
    expect_setting "node sets sail pin $letter to GPIO$number" "$substitutions" \
      "^  sails_pin_$letter: '?GPIO$number'?$"
    expect_setting "sails package takes pin $letter from its substitution" "$package" \
      "^ +pin_$letter: '?\\\$\\{sails_pin_$letter\\}'?$"
    actual=$(pin_number "$stepper" "pin_$letter")
    if [[ $actual == "$number" ]]; then
      pass "sails stepper pin $letter is GPIO$number"
    else
      fail "sails stepper pin $letter is GPIO$number" "pin number is '$actual' in: $stepper"
    fi
  done
  expect_setting "sails stepper is a ULN2003" "$stepper" '^  - platform: uln2003$'
  expect_setting "sails stepper does not sleep when done" "$stepper" '^    sleep_when_done: false$'
  expect_setting "sails stepper turns at 170 steps/s" "$stepper" '^    max_speed: 170(\.0)?( steps/s)?$'
  expect_setting "Sails Turning is optimistic" "$turn" '^    optimistic: true$'
  expect_setting "Sails Turning boots off" "$turn" '^    restore_mode: ALWAYS_OFF$'
}

# package_violations DIR prints each line of DIR/*.yaml, with comments
# removed, that holds a node-level key or a literal GPIO number.
package_violations() {
  local file
  for file in "$1"/*.yaml; do
    [[ -f $file ]] || continue
    sed -E 's/(^|[[:space:]])#.*$//' "$file" |
      grep -nE '^[[:space:]]*(- +)?(esphome|esp32|wifi|api|ota|logger):|GPIO[0-9]|\b(pin[a-z_]*|number): *[0-9]' |
      sed "s|^|${file##*/}:|" || true
  done
}

test_packages_hold_no_node_config() {
  local fixture violations
  fixture=$(mktemp -d "$work/packages.XXXX")
  printf 'wifi:\n  ssid: x\nstepper:\n  - pin_a: GPIO0  # GPIO9\n    pin_b: 4\n# api:\n' > "$fixture/mill_bad.yaml"
  violations=$(package_violations "$fixture")
  if [[ $(grep -c . <<< "$violations") -eq 3 ]]; then
    pass "package search finds node keys and literal pins"
  else
    fail "package search finds node keys and literal pins" "found: $violations"
  fi
  if ! compgen -G "$repo/packages/*.yaml" > /dev/null; then
    fail "packages hold no node config or literal pins" "no package in $repo/packages"
    return
  fi
  violations=$(package_violations "$repo/packages")
  if [[ -z $violations ]]; then
    pass "packages hold no node config or literal pins"
  else
    fail "packages hold no node config or literal pins" "found: $violations"
  fi
}

test_tool_versions
test_full_pass_with_stubs
test_refuses_missing_secrets
test_refuses_staged_secrets
test_refuses_missing_tools
test_secrets_kept_out_of_git
test_example_secrets_are_placeholders
load_config
test_node_settings
test_sails_settings
test_packages_hold_no_node_config

echo "$passed passed, $failed failed"
[[ $failed -eq 0 ]]
