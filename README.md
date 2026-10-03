# esp-windmill

ESPHome firmware for the motorised Matchmaker MM02 windmill. One ESP32-C3 node
(`village-windmill`) joins Home Assistant as "Windmill". `spec.md` holds the build spec.

## Setup

Create the Python venv with the pinned tools:

```bash
python3 -m venv .venv && . .venv/bin/activate && pip install -r requirements.txt
```

Copy the example secrets:

```bash
cp secrets.example.yaml secrets.yaml
```

The example values let the firmware validate and compile, but they connect to nothing.
Before you flash a device, put your real values in `secrets.yaml`:

- `wifi_ssid` and `wifi_password`: your WiFi network.
- `ap_password`: the password for the "Windmill Fallback" access point (8 or more characters).
- `ota_password`: the password for network updates.
- `api_key`: the Home Assistant encryption key, a fresh 32-byte base64 value. Make one with
  `openssl rand -base64 32`, or with
  `python3 -c "import os,base64;print(base64.b64encode(os.urandom(32)).decode())"`.

Git ignores `secrets.yaml`. Never add it to a commit.

## Checks

With the venv active, run:

```bash
scripts/check.sh
```

The script lints all YAML (`yamllint -s .`), validates the config (`esphome config windmill.yaml`)
and compiles the firmware (`esphome compile windmill.yaml`). It stops at the first failure.
It refuses to start if `esphome` or `yamllint` is not on the path, if `secrets.yaml` is missing,
or if `secrets.yaml` is staged in git. The first compile downloads the ESP-IDF toolchain
(several GB), so it takes several minutes.

`scripts/test_check.sh` tests the check script, the secrets handling, the node and sails settings,
and that no package holds node-level config or a literal GPIO number.
Run it with the venv active.

## First USB flash and OTA

First flash, over USB. Unplug the USB charger and connect the C3 to the laptop:

```bash
esphome run windmill.yaml --device /dev/ttyACM0
```

If Home Assistant still lists the old device from the web installer, remove it before you adopt
the new one. Home Assistant then discovers "Windmill" and asks for the encryption key:
enter the `api_key` from `secrets.yaml`.

After the first flash, update over the network:

```bash
esphome run windmill.yaml --device village-windmill.local
```

If `village-windmill.local` does not resolve on this laptop (mDNS discovery is blocked here),
use the device's IP address instead: `--device <ip>`. Find the current IP in the router or in HA.

`esphome run` compiles and then uploads. `esphome upload` sends the last build in the
build directory, so compile the config you want (or use `esphome run`) before an upload.

Serial logging is off. Read the device log over the network:

```bash
esphome logs windmill.yaml
```

If the device cannot join the configured WiFi, it starts the access point "Windmill Fallback"
(password `ap_password`) after about 90 s. Join it with a phone and enter a working network on the
setup page. The device keeps that network across network updates until a full flash erase. To go
back to the network in `secrets.yaml`, enter it again on the setup page, or erase the flash and
flash over USB.

## Forward direction

The sails must turn anticlockwise when you look from the sail side. The substitution
`sails_forward_direction` in `windmill.yaml` sets which way the stepper turns. Check it on the bench:

1. Put a mark on the motor coupler.
2. Turn on "Sails Turning" in HA.
3. Look at the coupler from the sail side.
4. If the mark turns clockwise, set `sails_forward_direction: "-1"` in the `substitutions` of
   `windmill.yaml` (or back to `"1"` if it is already `"-1"`). Then update over the network
   (see "First USB flash and OTA").
