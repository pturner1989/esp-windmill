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

`scripts/test_check.sh` tests the check script, the secrets handling, the node, sails and lights
settings, that no action writes the pixel strip directly, and that no package holds node-level config
or a literal GPIO number.
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

## Sail speed

"Sail Speed" in HA sets how fast the sails turn, in rpm: 1 to 9 in steps of 0.5. It starts at 5 rpm
and keeps its value across a restart, but a restart never starts the sails. A value between two steps
is rounded, and HA shows the rounded value. The sails never jump to a speed: "Sails Turning" on spins
them up over about 3 s, off (or a short press of the Mill Button) spins them down over about 3 s, and a
new "Sail Speed" while they turn ramps them to it at the same rate. "Sails Turning" shows off while
the sails spin down.

## Sail direction

The "Reverse Rotation" switch in HA sets the sail direction. When it is off, the sails turn
forward; when it is on, they turn the other way. Switch it while the sails turn and they slow to a
stop, then speed up the other way, and "Sails Turning" stays on. Switch it while they are stopped and it only sets the direction
for the next start. A short press of the Mill Button never changes it. A long press (hold 1 s to 5 s,
then release) toggles it while the sails turn, so they slow to a stop and reverse; while the sails are stopped, a
long press does nothing. While "Disco Mode" is on, a long press leaves disco instead (see "Disco"). The switch is off after every restart, so the sails always start forward.
`sails_forward_direction` (see "Forward direction") still sets which way is forward. Keep
"Reverse Rotation" off when you check the forward direction.

## HA light groups

The firmware gives HA four lights, one for each pixel: "Mill Door Lamp" (pixel 0),
"Mill Door Glow" (pixel 1), "Mill Stone Floor Window" (pixel 2) and "Mill Bin Floor Window" (pixel 3).
Create two light groups in HA to control them together. HA lists each light with the device name
in front, for example "Windmill Mill Door Glow".

1. In HA, go to Settings → Devices & services → Helpers.
2. Select Create helper → Group → Light group.
3. Name it "Mill Interior" and add Mill Door Glow, Mill Stone Floor Window and Mill Bin Floor Window
   (pixels 1–3). Submit.
4. Repeat steps 2 and 3 for a group named "Mill Lights" with all four lights.

The groups exist only in HA. The mill button and the firmware scripts act on the four lights
directly, not on these groups.

## Disco

Turn on the "Disco Mode" switch in HA to start disco. All four lights turn on at full brightness with
the "Disco" effect: each pixel flashes in turn and fades, the firing order changes every bar and the
colours change every 16 beats. Turn the switch off to end disco: within 4 s all four lights show the
lamplight look (warm amber, with Lamplight on the three interior lights), also a light that was off
before disco. Disco never touches the sails, so turning sails keep turning and stopped sails stay
stopped. The switch is off after every restart.

The Mill Button works in disco, also with no WiFi or HA. A short press (50 ms to 500 ms) turns the
mill off as it does outside disco: the sails spin down, all four lights fade off, and "Disco Mode" turns
off. A long press (hold 1 s to 5 s, then release) leaves disco: all four lights show the lamplight look
within 4 s, and the sails keep their speed and direction, with no reversal. Other presses do nothing.
The button never starts disco. Outside disco, the button acts as before.

Set the tempo with "Disco BPM" (60 to 180 in steps of 0.1) and the flashes per beat with "Disco Rate":
½× flashes each pixel every second beat, 1× every beat and 2× every beat and half beat. A change takes
effect at once and keeps the beat. 2× is allowed only at 90 BPM or below, so no pixel flashes more than
three times a second: above 90 BPM the mill refuses 2× and HA shows 1×, and a tempo above 90 BPM drops
2× to 1×. Both controls go back to 120 BPM and 1× after every restart.
