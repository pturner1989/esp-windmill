#!/usr/bin/env bash
# Tests for scripts/check.sh, the repository's secrets, the node settings and
# the packages. Run it with the venv active: the tool-version and node-config
# tests use the real esphome and yamllint, and the lambda test uses the venv's
# PyYAML. The refusal tests run check.sh in a temporary git repository with
# stub tools that record each call.
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

# section NAME [TEXT] prints the top-level block NAME from TEXT, or from the
# validated config when TEXT is not given.
section() {
  awk -v key="$1:" '$0 == key { on = 1; next } /^[^ ]/ { on = 0 } on' <<< "${2-$config}"
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

# item_key ITEM KEY prints the lines nested under KEY in the list entry ITEM.
item_key() {
  awk -v key="$2:" '
    NF { match($0, /[^ ]/) }
    on && NF && RSTART <= 5 { exit }
    on
    $0 == "    " key { on = 1 }' <<< "$1"
}

# flat TEXT prints TEXT without comment lines on one line, with each run of
# spaces and line breaks as one space.
flat() {
  sed '/^ *#/d' <<< "$1" | tr -s ' \n' ' '
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
  expect_setting "WiFi loss never restarts the device" "$wifi" '^  reboot_timeout: 0s$'
  expect_setting "HA loss never restarts the device" "$api" '^  reboot_timeout: 0s$'
  expect_setting "access point starts after the default 90 s" "$wifi" '^    ap_timeout: 90s$'
  expect_same "node leaves the access point timeout at its default" "" \
    "$(grep -n 'ap_timeout' "$repo/windmill.yaml" || true)"
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

test_sail_speed_settings() {
  [[ $config_status -ne 0 ]] && return
  local speed
  speed=$(list_item "$(section number)" "^    name: '?Sail Speed'?$")
  expect_setting "Sail Speed has id mill_sail_speed" "$speed" '^    id: mill_sail_speed$'
  expect_setting "Sail Speed minimum is 60" "$speed" '^    min_value: 60(\.0)?$'
  expect_setting "Sail Speed maximum is 320" "$speed" '^    max_value: 320(\.0)?$'
  expect_setting "Sail Speed step is 10" "$speed" '^    step: 10(\.0)?$'
  expect_setting "Sail Speed starts at 170" "$speed" '^    initial_value: 170(\.0)?$'
  expect_setting "Sail Speed is in steps/s" "$speed" "^    unit_of_measurement: '?steps/s'?$"
  expect_setting "Sail Speed restores its value" "$speed" '^    restore_value: true$'
  expect_setting "Sail Speed is not optimistic" "$speed" '^    optimistic: false$'
}

test_sail_speed_rounding() {
  local package speed turn set_action turn_on round_x round_any
  package=$(cat "$repo/packages/mill_sails.yaml" 2> /dev/null || true)
  speed=$(list_item "$(section number "$package")" "^    name: '?Sail Speed'?$")
  turn=$(list_item "$(section switch "$package")" "^    name: '?Sails Turning'?$")
  set_action=$(flat "$(item_key "$speed" set_action)")
  turn_on=$(flat "$(item_key "$turn" turn_on_action)")
  round_x='floor\(\(x \+ 5\) / 10\) \* 10'
  round_any='floor\(\([a-z_]+ \+ 5\) / 10\) \* 10'
  expect_setting "Sail Speed set action rounds half up to a multiple of 10" "$set_action" "$round_x"
  expect_setting "Sail Speed set action sets the rounded value one loop pass later" "$set_action" \
    "- delay: 0ms - number\.set: id: mill_sail_speed value: [^-]*$round_x"
  expect_setting "Sail Speed set action passes the speed to the stepper" "$set_action" \
    '- stepper\.set_speed: id: mill_sails '
  expect_setting "Sails Turning turn-on rounds Sail Speed half up" "$turn_on" "$round_any"
  expect_setting "Sails Turning turn-on clamps the speed to 60-320" "$turn_on" \
    'clamp\(.+, 60(\.0f?)?, 320(\.0f?)?\)'
  expect_setting "Sails Turning turn-on uses 170 for an unknown speed" "$turn_on" \
    'isnan\([a-z_]+\) \? 170(\.0f?)? :'
  expect_setting "Sails Turning turn-on reads Sail Speed" "$turn_on" 'id\(mill_sail_speed\)\.state'
  expect_setting "Sails Turning sets the speed, then re-arms" "$turn_on" \
    '^ ?- stepper\.set_speed: id: mill_sails .*- script\.execute: mill_sails_rearm'
}

test_sails_rearm() {
  local package rearm repeat direction resolved
  package=$(cat "$repo/packages/mill_sails.yaml" 2> /dev/null || true)
  rearm=$(flat "$(item_key "$(list_item "$(section script "$package")" '^  - id: mill_sails_rearm$')" then)")
  expect_setting "re-arm script re-bases to 0, then aims 10,000,000 steps in the chosen direction" "$rearm" \
    '^ ?- stepper\.report_position: id: mill_sails position: 0 - stepper\.set_target: id: mill_sails target: !lambda "return \$\{sails_forward_direction\} \* \(id\(mill_sails_reverse\) \? -1 : 1\) \* 10000000;" ?$'
  [[ $config_status -ne 0 ]] && return
  direction=$(sed -En "s/^  sails_forward_direction: '?(-?1)'?$/\1/p" <<< "$(section substitutions)")
  resolved=$(flat "$(list_item "$(section script)" '^  - id: mill_sails_rearm$')")
  expect_setting "re-arm target resolves to the forward direction times the reverse sign times 10,000,000" \
    "$resolved" "target: !lambda \|- return ${direction:-unset} \* \(id\(mill_sails_reverse\) \? -1 : 1\) \* 10000000;"
  repeat=$(flat "$(list_item "$(section interval)" '^  - interval: 10min$')")
  expect_setting "an interval runs every 10 minutes" "$repeat" '^ ?- interval: 10min '
  expect_setting "the interval re-arms the sails only while Sails Turning is on" "$repeat" \
    '^ ?- interval: 10min then: - if: condition: switch\.is_on: id: mill_sails_turn then: - script\.execute: id: mill_sails_rearm( startup_delay: [0-9a-z]+)? ?$'
}

test_sails_reverse() {
  local package header word missing="" global reverse calls expected action found
  package=$(cat "$repo/packages/mill_sails.yaml" 2> /dev/null || true)
  header=$(awk '!/^#/ { exit } 1' <<< "$package")
  for word in mill_sails_reverse mill_sails_reverse_switch; do
    grep -qw -- "$word" <<< "$header" || missing+=" $word"
  done
  if [[ -n $header && -z $missing ]]; then
    pass "sails package comment lists the direction ids"
  else
    fail "sails package comment lists the direction ids" "missing:${missing:- the comment}"
  fi
  [[ $config_status -ne 0 ]] && return
  global=$(list_item "$(section globals)" '^  - id: mill_sails_reverse$')
  expect_setting "direction global is a bool" "$global" '^    type: bool$'
  expect_setting "direction global starts false" "$global" "^    initial_value: '?false'?$"
  expect_setting "direction global is not restored" "$global" '^    restore_value: false$'
  reverse=$(list_item "$(section switch)" "^    name: '?Reverse Rotation'?$")
  expect_setting "Reverse Rotation has id mill_sails_reverse_switch" "$reverse" '^    id: mill_sails_reverse_switch$'
  expect_setting "Reverse Rotation is optimistic" "$reverse" '^    optimistic: true$'
  expect_setting "Reverse Rotation boots off" "$reverse" '^    restore_mode: ALWAYS_OFF$'
  calls=$(action_calls - <<< "$config")
  expected=""
  # The validated config puts each action list under "then".
  for action in turn_on_action:true turn_off_action:false; do
    expected+="${expected:+$'\n'}${action%:*}/then globals.set id=mill_sails_reverse;value=${action#*:}"
    expected+=$'\n'"${action%:*}/then/if/condition switch.is_on id=mill_sails_turn"
    expected+=$'\n'"${action%:*}/then/if/then script.execute id=mill_sails_rearm"
  done
  expect_same "Reverse Rotation sets the direction global, then re-arms only while Sails Turning is on" \
    "$expected" "$(awk -F '\t' '$1 == "switch/mill_sails_reverse_switch" { print $2 " " $3 " " $4 }' <<< "$calls")"
  found=$(awk -F '\t' '$1 != "switch/mill_sails_reverse_switch" &&
    (($3 == "globals.set" && $4 ~ /(^|;)id=mill_sails_reverse(;|$)/) ||
     ($3 ~ /^switch\.(turn_on|turn_off|toggle)$/ && $4 ~ /(^|;)id=mill_sails_reverse_switch(;|$)/))' <<< "$calls")
  expect_same "nothing but Reverse Rotation changes the direction" "" "$found"
}

# long_lambdas FILE... prints the first line of each lambda in FILE that spans
# more than two source lines. A lambda is a value tagged !lambda or the value
# of a "lambda" key.
long_lambdas() {
  python3 - "$@" << 'PY'
import sys

import yaml


def source_lines(node, text):
    lines = text[node.start_mark.index:node.end_mark.index].strip().splitlines()
    if node.style in ("|", ">"):
        lines = lines[1:]
    return [line.strip() for line in lines if line.strip()]


def walk(node, key, text, found):
    if isinstance(node, yaml.MappingNode):
        for k, v in node.value:
            walk(v, k.value, text, found)
    elif isinstance(node, yaml.SequenceNode):
        for v in node.value:
            walk(v, key, text, found)
    elif node.tag == "!lambda" or key == "lambda":
        lines = source_lines(node, text)
        if len(lines) > 2:
            found.append(lines[0])


for path in sys.argv[1:]:
    with open(path) as f:
        text = f.read()
    found = []
    walk(yaml.compose(text, Loader=yaml.SafeLoader), None, text, found)
    for first in found:
        print(f"{path.rsplit('/', 1)[-1]}: {first}")
PY
}

test_lambdas_are_short() {
  local fixture found
  fixture=$(mktemp -d "$work/lambdas.XXXX")
  cat > "$fixture/mill_bad.yaml" << 'YAML'
number:
  - set_action:
      - lambda: |-
          int a = 1;
          int b = 2;
          return a + b;
      - lambda: "return 1;"
    speed: !lambda |-
      float s = 1;
      return s;
    target: !lambda >-
      int a = 1;
      int b = 2;
      return a + b;
YAML
  found=$(long_lambdas "$fixture/mill_bad.yaml")
  if [[ $(grep -c . <<< "$found") -eq 2 ]]; then
    pass "lambda search finds lambdas longer than two lines"
  else
    fail "lambda search finds lambdas longer than two lines" "found: $found"
  fi
  found=$(long_lambdas "$repo"/packages/*.yaml)
  if [[ -z $found ]]; then
    pass "no lambda in a package is longer than two lines ($(cd "$repo/packages" && echo *.yaml))"
  else
    fail "no lambda in a package is longer than two lines" "found: $found"
  fi
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
  # The search reads packages/*.yaml, so it covers every package the node includes.
  local included file outside=""
  included=$(sed -nE 's/^  [a-z_]+: !include (.+)$/\1/p' "$repo/windmill.yaml")
  for file in $included; do
    [[ $file == packages/*.yaml && $file != */*/* && -f $repo/$file ]] || outside+=" $file"
  done
  if [[ -z $included ]]; then
    fail "package search covers every included package" "windmill.yaml includes no package"
  elif [[ -n $outside ]]; then
    fail "package search covers every included package" "not in packages/:$outside"
  else
    pass "package search covers every included package ($(tr '\n' ' ' <<< "$included" | sed 's/ $//'))"
  fi
}

# strip_writes FILE... prints each addressable_set, each rmt_channel, each
# light action or condition that names the strip mill_pixels, and each lambda
# that names it, in FILE. Comments are not read.
strip_writes() {
  python3 - "$@" << 'PY'
import re
import sys

import yaml


def target(node):
    if isinstance(node, yaml.ScalarNode):
        return node.value
    if isinstance(node, yaml.MappingNode):
        for k, v in node.value:
            if k.value == "id" and isinstance(v, yaml.ScalarNode):
                return v.value
    return None


def walk(node, found):
    if isinstance(node, yaml.MappingNode):
        for k, v in node.value:
            key = str(k.value)
            if "addressable_set" in key or key == "rmt_channel":
                found.append(key)
            elif key.startswith("light.") and target(v) == "mill_pixels":
                found.append(f"{key}: mill_pixels")
            walk(v, found)
    elif isinstance(node, yaml.SequenceNode):
        for v in node.value:
            walk(v, found)
    elif re.search(r"\bid\(\s*mill_pixels\s*\)", node.value):
        found.append(node.value.strip().splitlines()[0])


for path in sys.argv[1:]:
    with open(path) as f:
        found = []
        walk(yaml.compose(f, Loader=yaml.SafeLoader), found)
    for line in found:
        print(f"{path.rsplit('/', 1)[-1]}: {line}")
PY
}

test_no_strip_writes() {
  local fixture found
  fixture=$(mktemp -d "$work/strip.XXXX")
  cat > "$fixture/mill_bad.yaml" << 'YAML'
light:
  - platform: partition
    segments:
      - id: mill_pixels
        from: 0
        to: 0
    rmt_channel: 0
script:
  - id: mill_bad
    then:
      - light.turn_on: mill_pixels
      - light.turn_off:
          id: mill_pixels
      - light.addressable_set:
          id: mill_door_glow
      - light.turn_on: mill_door_glow
      - lambda: "id(mill_pixels).turn_on();"
# - light.control: mill_pixels
YAML
  found=$(strip_writes "$fixture/mill_bad.yaml")
  if [[ $(grep -c . <<< "$found") -eq 5 ]]; then
    pass "strip search finds strip writes, addressable_set and rmt_channel"
  else
    fail "strip search finds strip writes, addressable_set and rmt_channel" "found: $found"
  fi
  found=$(strip_writes "$repo/windmill.yaml" "$repo"/packages/*.yaml)
  if [[ -z $found ]]; then
    pass "no action writes the strip, and no addressable_set or rmt_channel exists"
  else
    fail "no action writes the strip, and no addressable_set or rmt_channel exists" "found: $found"
  fi
}

# expect_cap_and_off NAME ITEM checks that the light ITEM carries a 60% colour
# correction on each of its four channels (red, green, blue and white) and
# boots off.
expect_cap_and_off() {
  expect_setting "$1 is capped at 60% on every channel, white included" \
    "$(flat "$(item_key "$2" color_correct)")" '^ ?- 0\.6 - 0\.6 - 0\.6 - 0\.6 ?$'
  expect_setting "$1 boots off" "$2" '^    restore_mode: ALWAYS_OFF$'
}

test_lights_settings() {
  local package header word missing="" lights strip entry id index name light
  package=$(cat "$repo/packages/mill_lights.yaml" 2> /dev/null || true)
  expect_setting "node includes the lights package" "$(section packages "$(cat "$repo/windmill.yaml")")" \
    '^  lights: !include packages/mill_lights\.yaml$'
  expect_setting "lights package takes the pixel pin from its substitution" "$package" \
    "^ +pin: '?\\\$\\{pixel_pin\\}'?$"
  header=$(awk '!/^#/ { exit } 1' <<< "$package")
  for word in pixel_pin mill_pixels mill_door_glow mill_stone_window mill_bin_window mill_door_lamp; do
    grep -qw -- "$word" <<< "$header" || missing+=" $word"
  done
  if [[ -n $header && -z $missing ]]; then
    pass "lights package starts with a comment that lists its substitution and ids"
  else
    fail "lights package starts with a comment that lists its substitution and ids" "missing:${missing:- the comment}"
  fi
  [[ $config_status -ne 0 ]] && return
  lights=$(section light)
  strip=$(list_item "$lights" '^    id: mill_pixels$')
  expect_setting "node sets the pixel pin to GPIO6" "$(section substitutions)" "^  pixel_pin: '?GPIO6'?$"
  if [[ $(pin_number "$strip" pin) == 6 ]]; then
    pass "pixel strip pin is GPIO6"
  else
    fail "pixel strip pin is GPIO6" "pin number is '$(pin_number "$strip" pin)' in: $strip"
  fi
  expect_setting "pixel strip is an RMT LED strip" "$strip" '^  - platform: esp32_rmt_led_strip$'
  expect_setting "pixel strip is internal" "$strip" '^    internal: true$'
  expect_setting "pixel strip has 4 pixels" "$strip" '^    num_leds: 4$'
  expect_setting "pixel strip chipset is SK6812" "$strip" '^    chipset: SK6812$'
  expect_setting "pixel strip is RGBW in GRBW channel order" "$strip" '^    channel_colors: GRBW$'
  expect_setting "pixel strip sends at most one frame each 20 ms" "$strip" '^    max_refresh_rate: 20ms$'
  expect_cap_and_off "pixel strip" "$strip"
  for entry in "mill_door_lamp:0:Mill Door Lamp" "mill_door_glow:1:Mill Door Glow" \
    "mill_stone_window:2:Mill Stone Floor Window" "mill_bin_window:3:Mill Bin Floor Window"; do
    IFS=: read -r id index name <<< "$entry"
    light=$(list_item "$lights" "^    id: $id$")
    expect_setting "light $id is a partition" "$light" '^  - platform: partition$'
    expect_setting "light $id is named $name" "$light" "^    name: '?$name'?$"
    expect_setting "$name lights only pixel $index" "$(flat "$(item_key "$light" segments)")" \
      "^ ?- id: mill_pixels from: $index to: $index reversed: false ?$"
    expect_setting "$name fades over 3 s by default" "$light" '^    default_transition_length: 3s$'
    expect_cap_and_off "$name" "$light"
  done
}

# effect_list ITEM prints one line for each effect of the light ITEM in the
# validated config: type|name|update_interval|intensity, with "-" for an
# option that is not set.
effect_list() {
  item_key "$1" effects | awk '
    function flush() { if (type != "") print type "|" name "|" interval "|" intensity }
    /^      - / {
      flush()
      type = $2
      sub(/:.*$/, "", type)
      name = interval = intensity = "-"
      next
    }
    { value = $0; sub(/^ *[a-z_]+: /, "", value); gsub(/\047/, "", value) }
    $1 == "name:" { name = value }
    $1 == "update_interval:" { interval = value }
    $1 == "intensity:" { intensity = value }
    END { flush() }'
}

# plain_flickers TEXT prints each line of TEXT that starts a plain flicker
# effect. An addressable_flicker effect does not match.
plain_flickers() {
  grep -E '^ *- flicker:' <<< "$1" || true
}

test_lamplight_effects() {
  local fixture found lights entry id name light effects type effect interval intensity
  fixture=$'    effects:\n      - addressable_flicker:\n          name: A\n      - flicker:\n          name: B\n      - flicker: {}\n'
  found=$(plain_flickers "$fixture")
  if [[ $(grep -c . <<< "$found") -eq 2 ]]; then
    pass "flicker search finds plain flicker effects only"
  else
    fail "flicker search finds plain flicker effects only" "found: $found"
  fi
  [[ $config_status -ne 0 ]] && return
  lights=$(section light)
  for entry in "mill_door_glow:Mill Door Glow" "mill_stone_window:Mill Stone Floor Window" \
    "mill_bin_window:Mill Bin Floor Window"; do
    IFS=: read -r id name <<< "$entry"
    light=$(list_item "$lights" "^    id: $id$")
    effects=$(effect_list "$light")
    if [[ $(grep -c . <<< "$effects") -ne 1 ]]; then
      fail "$name has exactly one effect" "effects: ${effects:-none}"
      continue
    fi
    pass "$name has exactly one effect"
    IFS='|' read -r type effect interval intensity <<< "$effects"
    if [[ $type == addressable_flicker ]]; then
      pass "$name effect is an addressable flicker"
    else
      fail "$name effect is an addressable flicker" "effect type is '$type'"
    fi
    if [[ $effect == Lamplight ]]; then
      pass "$name effect is named Lamplight"
    else
      fail "$name effect is named Lamplight" "effect name is '$effect'"
    fi
    if [[ $interval == 50ms ]]; then
      pass "$name Lamplight updates every 50 ms"
    else
      fail "$name Lamplight updates every 50 ms" "update_interval is '$interval'"
    fi
    if [[ $intensity =~ ^[0-9]*\.?[0-9]+$ ]] && awk -v v="$intensity" 'BEGIN { exit !(v > 0) }'; then
      pass "$name Lamplight intensity is above 0%"
    else
      fail "$name Lamplight intensity is above 0%" "intensity is '$intensity'"
    fi
  done
  light=$(list_item "$lights" '^    id: mill_door_lamp$')
  effects=$(item_key "$light" effects)
  if [[ -n $light && -z $effects ]]; then
    pass "Mill Door Lamp has no effects"
  else
    fail "Mill Door Lamp has no effects" "effects: ${effects:-no light mill_door_lamp}"
  fi
  found=$(plain_flickers "$lights")
  if [[ -z $found ]]; then
    pass "no light uses the plain flicker effect"
  else
    fail "no light uses the plain flicker effect" "found: $found"
  fi
}

# action_calls FILE... prints one line for each action or condition in FILE,
# or in standard input when FILE is "-":
# owner<TAB>path<TAB>key<TAB>arguments.
# The owner is the top-level key and the id of its list entry, for example
# script/mill_toggle. The path is the chain of keys from that entry down to
# the call, for example then/if/condition/or. A call is a key with a dot in it
# (light.turn_on) or a delay or lambda key. A plain value prints as it is; a
# mapping prints its plain values as key=value pairs joined by ";". Comments
# are not read.
action_calls() {
  # The script comes from -c, so that standard input stays free for "-".
  python3 -c "$(cat << 'PY'
import sys

import yaml


def scalar(node):
    return isinstance(node, yaml.ScalarNode)


def arguments(node):
    if scalar(node):
        return node.value.strip().splitlines()[0] if node.value.strip() else ""
    if isinstance(node, yaml.MappingNode):
        return ";".join(f"{k.value}={v.value}" for k, v in node.value if scalar(v))
    return ""


def walk(node, owner, path, out):
    if isinstance(node, yaml.MappingNode):
        for k, v in node.value:
            key = str(k.value)
            if "." in key or key in ("delay", "lambda"):
                out.append(f"{owner}\t{'/'.join(path)}\t{key}\t{arguments(v)}")
            walk(v, owner, path + [key], out)
    elif isinstance(node, yaml.SequenceNode):
        for v in node.value:
            walk(v, owner, path, out)


for path in sys.argv[1:]:
    text = sys.stdin.read() if path == "-" else open(path).read()
    out = []
    root = yaml.compose(text, Loader=yaml.SafeLoader)
    for k, v in root.value if isinstance(root, yaml.MappingNode) else []:
        entries = v.value if isinstance(v, yaml.SequenceNode) else [v]
        for entry in entries:
            owner = k.value
            if isinstance(entry, yaml.MappingNode):
                ids = [i.value for n, i in entry.value if n.value == "id" and scalar(i)]
                owner += "/" + (ids[0] if ids else "-")
                walk(entry, owner, [], out)
    print("\n".join(out))
PY
)" "$@"
}

# calls_of OWNER [PATH] prints "key arguments" for each call of OWNER in
# $controls_calls, in order. With PATH, only the calls at that path.
calls_of() {
  awk -F '\t' -v owner="$1" -v path="${2-}" \
    '$1 == owner && (path == "" || $2 == path) { print $3 " " $4 }' <<< "$controls_calls"
}

# expect_same NAME EXPECTED ACTUAL checks that the two texts are equal.
expect_same() {
  if [[ $2 == "$3" ]]; then
    pass "$1"
  else
    fail "$1" "expected: $(tr '\n' '|' <<< "$2") actual: $(tr '\n' '|' <<< "$3")"
  fi
}

test_action_search() {
  local found
  found=$(action_calls - << 'YAML'
script:
  - id: mill_a
    then:
      - if:
          condition:
            or:
              - light.is_on: mill_door_lamp
          then:
            - delay: 3s
            # - light.turn_on: mill_pixels
            - light.turn_on:
                id: mill_door_glow
                red: 100%
YAML
)
  expect_same "action search lists each call with its owner, path and arguments" \
    $'script/mill_a\tthen/if/condition/or\tlight.is_on\tmill_door_lamp\nscript/mill_a\tthen/if/then\tdelay\t3s\nscript/mill_a\tthen/if/then\tlight.turn_on\tid=mill_door_glow;red=100%' \
    "$found"
}

partitions="mill_door_glow mill_stone_window mill_bin_window mill_door_lamp"
interior="mill_door_glow mill_stone_window mill_bin_window"

test_controls_button() {
  local package header word missing="" button pin on_keys clicks
  package=$(cat "$repo/packages/mill_controls.yaml" 2> /dev/null || true)
  expect_setting "node includes the controls package" "$(section packages "$(cat "$repo/windmill.yaml")")" \
    '^  controls: !include packages/mill_controls\.yaml$'
  expect_setting "controls package takes the button pin from its substitution" "$package" \
    "^ +number: '?\\\$\\{button_pin\\}'?$"
  header=$(awk '!/^#/ { exit } 1' <<< "$package")
  for word in button_pin mill_button mill_toggle mill_on mill_off; do
    grep -qw -- "$word" <<< "$header" || missing+=" $word"
  done
  if [[ -n $header && -z $missing ]]; then
    pass "controls package starts with a comment that lists its substitution and ids"
  else
    fail "controls package starts with a comment that lists its substitution and ids" "missing:${missing:- the comment}"
  fi
  [[ $config_status -ne 0 ]] && return
  expect_setting "node sets the button pin to GPIO5" "$(section substitutions)" "^  button_pin: '?GPIO5'?$"
  button=$(list_item "$(section binary_sensor)" "^    name: '?Mill Button'?$")
  expect_setting "Mill Button has id mill_button" "$button" '^    id: mill_button$'
  expect_setting "Mill Button is a GPIO input" "$button" '^  - platform: gpio$'
  if [[ $(pin_number "$button" pin) == 5 ]]; then
    pass "Mill Button pin is GPIO5"
  else
    fail "Mill Button pin is GPIO5" "pin number is '$(pin_number "$button" pin)' in: $button"
  fi
  pin=$(flat "$(item_key "$button" pin)")
  expect_setting "Mill Button pin is an input with pull-up" "$pin" ' mode: input: true pullup: true '
  expect_setting "Mill Button pin is inverted" "$pin" ' inverted: true '
  expect_setting "Mill Button has only a 20 ms debounce filter" "$(flat "$(item_key "$button" filters)")" \
    '^ ?- delayed_on_off: 20ms ?$'
  on_keys=$( (grep -E '^    on_[a-z_]+:' <<< "$button" || true) | tr -d ' ' | tr '\n' ' ')
  expect_same "Mill Button has a click handler and no other handler" "on_click: " "$on_keys"
  clicks=$(item_key "$button" on_click)
  if [[ $(grep -c '^      - ' <<< "$clicks") -eq 1 ]]; then
    pass "Mill Button has exactly one click handler"
  else
    fail "Mill Button has exactly one click handler" "on_click: $clicks"
  fi
  expect_setting "Mill Button click is 50-500 ms and runs the toggle" "$(flat "$clicks")" \
    '^ ?- min_length: 50ms max_length: 500ms then: - script\.execute: id: mill_toggle ?$'
}

test_controls_scripts() {
  [[ $config_status -ne 0 ]] && return
  local expected id level logs bad mill_on before after pairs
  controls_calls=$(action_calls - <<< "$config")
  expected="switch.is_on id=mill_sails_turn"
  for id in $partitions; do expected+=$'\n'"light.is_on id=$id"; done
  expect_same "toggle checks the sails and all four lights" "$expected" \
    "$(calls_of script/mill_toggle then/if/condition/or)"
  expect_setting "toggle turns the mill off when anything is on" \
    "$(calls_of script/mill_toggle then/if/then | tr '\n' '|')" \
    '^script\.execute id=mill_off\|logger\.log format=[^|;]*mill off;'
  expect_setting "toggle turns the mill on when everything is off" \
    "$(calls_of script/mill_toggle then/if/else | tr '\n' '|')" \
    '^script\.execute id=mill_on\|logger\.log format=[^|;]*mill on;'
  logs=$(grep -P '\tlogger\.log\t' <<< "$controls_calls" || true)
  bad=$(grep -vE ';level=INFO;tag=mill\.controls(;|$)' <<< "$logs" || true)
  if [[ $(grep -c . <<< "$logs") -eq 2 && -z $bad ]]; then
    pass "both log calls use level INFO and tag mill.controls"
  else
    fail "both log calls use level INFO and tag mill.controls" "log calls: $logs"
  fi
  expected=$'script.stop id=mill_on\nswitch.turn_off id=mill_sails_turn'
  for id in $partitions; do expected+=$'\n'"light.turn_off id=$id;state=false"; done
  expect_same "mill off stops mill on first, then stops the sails and turns off the four lights" \
    "$expected" "$(calls_of script/mill_off)"
  expect_setting "mill on restarts when run again" \
    "$(list_item "$(section script)" '^  - id: mill_on$')" '^    mode: restart$'
  mill_on=$(calls_of script/mill_on)
  before=$(sed '/^delay /,$d' <<< "$mill_on")
  after=$(sed -n '/^delay /,$p' <<< "$mill_on")
  expected="switch.turn_on id=mill_sails_turn"
  for id in $partitions; do
    [[ $id == mill_door_lamp ]] && level=0.85 || level=1.0
    expected+=$'\n'"light.turn_on id=$id;color_mode=RGB_WHITE;brightness=$level;color_brightness=1.0;red=1.0;green=0.76;blue=0.52;white=0.0;state=true"
  done
  expect_same "mill on starts the sails, then fades the lights on to amber with white off and no effect" \
    "$expected" "$before"
  pairs=$(sed -n '/^light\.turn_on .*effect=/{x;G;s/\n/ => /;p};h' <<< "$after")
  expected=""
  for id in $interior; do
    expected+="${expected:+$'\n'}lambda return id($id).remote_values.is_on(); => light.turn_on id=$id;effect=Lamplight;state=true"
  done
  expect_setting "mill on waits 3 s for the fade" "$after" '^delay 3s$'
  expect_same "after the fade, mill on starts Lamplight on each interior light that is still on" \
    "$expected" "$pairs"
}

test_controls_boundaries() {
  local package text found ids id outside=""
  package="$repo/packages/mill_controls.yaml"
  if [[ ! -f $package ]]; then
    fail "controls package exists" "no file $package"
    return
  fi
  text=$(sed -E 's/(^|[[:space:]])#.*$//' "$package")
  found=$(grep -n 'mill_sails_reverse' "$package" || true)
  expect_same "controls package does not name the direction global" "" "$found"
  found=$(grep -nE 'mill_sails_reverse_switch|Reverse Rotation' "$package" || true)
  expect_same "controls package does not name the Reverse Rotation switch" "" "$found"
  found=$(grep -n 'homeassistant' <<< "$text" || true)
  expect_same "controls package makes no call to Home Assistant" "" "$found"
  found=$( (strip_writes "$package"; grep -n 'mill_pixels' <<< "$text") || true)
  expect_same "controls package does not write or name the strip" "" "$found"
  ids=$(action_calls "$package" | awk -F '\t' '$3 ~ /^light\.(turn_on|control|toggle)$/ { print $4 }' |
    sed -E 's/^(.*;)?id=([^;]*).*$/\2/')
  for id in $ids; do
    grep -qw -- "$id" <<< "$partitions" || outside+=" $id"
  done
  if [[ -n $ids && -z $outside ]]; then
    pass "every light the controls package turns on is a capped partition light"
  else
    fail "every light the controls package turns on is a capped partition light" \
      "lights outside the four partitions:${outside:- none, but no light is turned on}"
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
test_sail_speed_settings
test_sail_speed_rounding
test_sails_rearm
test_sails_reverse
test_lambdas_are_short
test_packages_hold_no_node_config
test_no_strip_writes
test_lights_settings
test_lamplight_effects
test_action_search
test_controls_button
test_controls_scripts
test_controls_boundaries

echo "$passed passed, $failed failed"
[[ $failed -eq 0 ]]
