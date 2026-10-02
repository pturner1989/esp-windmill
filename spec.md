# Windmill Module — Standalone Build Spec

Sep 22, 2026 · @Peter Turner

Matchmaker MM02 windmill, motorised sails and four addressable pixels, on its own ESP node. Built and finished as a self-contained module that later either folds into the main hub or stays an independent node.

## Scope and the migration rule

This module is the mill, its sails, its drive, four pixels and its own ESP, finished and working on the bench before the landscape exists. It has one input: **5V USB into the C3's own USB-C port**, from a USB charger. Nothing else crosses its boundary.

That single rule is what makes both your migration paths work later:

| Path | What changes | What doesn't |
| --- | --- | --- |
| Keep the ESP | Plug its USB cable into any 5V USB source, or a 12V-to-USB module if a 12V bus ever exists | Everything inside the mill |
| Fold it into the main hub | Unplug two connectors in the base box, run 7 wires to the hub | The mill, the drive, the pixels, the mounting |

So the module is built around an internal split at the base of the mill. Above the split: the mill, sails, motor and pixel string, terminating in a **7-pin JST-XH** carrying four stepper coils, pixel data, 5V and GND. Below the split, in the base box: the ULN2003, the data-level fix (diode drop or level shifter) and the ESP.

When the mill joins the main system you either leave the box alone and change what feeds the USB cable, or you pull the ESP and extend that 7-pin connector to the hub. Neither touches anything you glued.

### Why USB-C in (decided 2026-10-02)

The original plan was 12V on an XT30 with a buck converter in the box, so the mill could join a 12V bus with a re-plug. That plan is dropped. The mill now takes 5V from a USB charger straight into the C3 SuperMini's USB-C port, and the board's 5V pin feeds the rest of the base box. That removes the XT30, polyfuse, SS34 and buck.

Because USB is the only power source, there is no back-feed risk. To flash over USB, unplug the charger and plug in the laptop. Normal updates go over the network.

What this costs, and how to manage it:

- **All current goes through the C3 board** (about 450mA with the brightness cap). That is within what the board and its USB-C socket carry, but do not add loads beyond this module.
- **Stepper noise sits on the C3's own supply.** The 470uF at the ULN2003 is required, not optional.
- **The cable pulls on a small socket.** Clamp the cable to the base box (cable tie to an anchor, or a P-clip) so a tug never reaches the board.
- **Charger:** 5V, 2A recommended (1A minimum). If a USB-C to USB-C cable gives no power, the board lacks the CC resistors; use a USB-A to USB-C cable.
- **The 5V pin may sit around 4.7V** if the board has a diode after the USB socket. Everything in this module works at that voltage.

## Dimensions and clearances

From the kit listing: 305mm high excluding sails, 355mm sail span.

| Dimension | Value | Source |
| --- | --- | --- |
| Height to cap | 305mm | Kit spec |
| Sail span | 355mm | Kit spec |
| Sail radius | 177.5mm | Derived |
| Hub height above base | \~275mm | Estimated, measure on build |
| Top of sail tip | \~455mm | Derived |
| Lowest sail tip | \~100mm above base | Derived |
| Tower base diameter | 100-120mm | Estimate, measure from formers |

### Height budget

| Item | mm |
| --- | --- |
| Base box | 58 |
| Ply lid, datum | 12 |
| Mill to sail tip | 455 |
| Air above sails | 12 |
| **Total above sill** | **537** |

**Measure your clear glass height before cutting anything.** If it's under about 540mm the kit won't stand as designed and you'd be looking at shortening the tower, which on a matchstick kit means re-spacing formers and is best decided before you start rather than after.

### Swept circle

The sails sweep a 355mm circle whose lowest point sits about 100mm above the mill's base. Directly beside the tower that's the tightest clearance; the arc rises as you move outward. Practical rule for the landscape later: **keep everything outside a 200mm radius of the mill centre**, which gives a comfortable margin over the arc and leaves room to get a hand in.

### Depth

A 100-120mm base on a 150mm board leaves 15-25mm each side. The mill will span both lanes, which is fine and intended, but it means the ground detail works around it rather than past it. If the base former comes in over 120mm, say so and the cross-section needs revisiting.

## Modifying the kit

The kit assumes a static display model. Five changes, all of which must happen before the relevant part is closed up.

### 1. Open the formers

The pre-cut card formers stack inside the tower. Before assembly, drill or cut a **12mm central passage** through every one so the motor wires and pixel string can run up the inside. Do all of them at once, stacked and clamped, so the holes line up.

### 2. Reinforce the base

Cut a disc of 4mm ply to match the bottom former and glue it underneath. This is what the mounting bolts go through, and what stops a 455mm structure pivoting on card.

### 3. Make the cap removable

This is the important one. The motor lives in the cap, and if the cap is glued down you will never service it.

- Build the cap as a separate sub-assembly on its own former ring.
- Locate it on three small dowels or a shallow rebate so it drops back in the same position.
- Retain it with two 3mm neodymium magnets, or a friction fit if the card is stiff enough.
- Test removal and replacement about twenty times before you finish it. It'll be done often.

### 4. Ventilate the cap

The stepper runs continuously and will sit around 40-45°C. Enclosed in matchsticks that's warm rather than dangerous, but it dries glue joints over a season. Real mills have louvred cap vents, so cut two on each side, 15 x 4mm, backed with black mesh. They read as correct detail and they matter.

### 5. Replace the sail hub

The kit's hub will be card and glued to the stocks. Replace it with a **25mm disc of 3mm ply or acrylic**, drilled 4mm at centre, with four slots cut for the sail stocks. Build the sails onto that instead. Matchstick sails are light, so the disc carries them easily, but card will not survive rotation.

### While you're at it

Paint the whole tower interior matt black before the formers go in, then white in a 40mm band at each pixel position. Once the tower is closed there is no access at all.

## Mechanical drive

Motor in the cap, driving the sail shaft directly. The alternative, a shaft down the tower to a motor in the box, needs a right-angle drive at the top and is harder to build and harder to service.

### The stack, front to back

| Part | Detail |
| --- | --- |
| Sail hub | 3mm ply or acrylic disc, 25mm, bored 4mm |
| Shaft | 4mm brass tube or rod, \~45mm long |
| Bearing | 684ZZ, 4 x 9 x 4mm, in a ply carrier |
| Carrier | 3mm ply disc set into the cap front, bearing press-fit |
| Coupler | 5mm to 4mm aluminium shaft coupler |
| Motor | 28BYJ-48, 5V, axis horizontal |
| Mount | 2mm ply bracket, motor's own two M3 holes |

The 28BYJ-48 is 28mm across and 19mm deep. With the cap interior around 55mm that fits, but it's the tightest part of the build, so mock it up in card before committing.

### Speed

In full-step mode the 28BYJ-48 gives 2048 steps per output revolution. Useful settings:

| Steps/sec | RPM | Reads as |
| --- | --- | --- |
| 100 | 2.9 | Very light breeze, almost too slow |
| 170 | 5.0 | Good default |
| 240 | 7.0 | Brisk |
| 400+ | 11+ | Torque drops, risk of missed steps |

Start at 170 and adjust by eye. Scale motion always looks best slower than you expect — the sails should look heavy.

### Balancing

At 177mm radius, small imbalances become visible wobble and put a bending load on a 4mm shaft.

- Weigh each finished sail before fitting. Aim within 0.2g of each other.
- Trim the heavy ones rather than adding weight where you can.
- Assemble the sails onto the hub, then balance the whole assembly on a pin through the bore before it goes on the shaft. It should sit still in any position.
- Where you must add mass, a dab of epoxy near the inner end of the light stock is better than anything at the tip.

### Torque and stall

The 28BYJ-48 has far more torque than these sails need, so the risk isn't stalling — it's the reverse. A missed step or a jam won't be felt, and the motor will happily keep trying while something binds. Run it continuously for an hour on the bench and check the bearing is cool and the shaft still spins freely by hand afterwards.

### Sleep behaviour

Set `sleep_when_done: false` so the coils stay energised and hold the sails still when stopped, otherwise they drift in a draught. That costs about 200mA and the warmth noted above. If it runs too hot in the finished cap, switch to `true` and accept a little drift.

## Lighting

Four SK6812 bullet pixels on a single data line. A 12mm bullet string at 100mm pitch suits a 305mm tower almost exactly.

| # | Position | Height | Purpose |
| --- | --- | --- | --- |
| 0 | Ground floor, mill door | \~40mm | Warm glow at the doorway, strongest from both lanes |
| 1 | Stone floor window | \~140mm | Mid-tower, the one most visible at distance |
| 2 | Bin floor window | \~240mm | High, reads against the sky |
| 3 | External lamp at the door | \~55mm | Mounted outside on a bracket, lights the threshold |

Pixel 3 is the one that earns its place. A single exterior lamp casting onto the ground gives the mill a sense of being occupied, and it's the only light that works when the tower windows are dark.

### Light-tightness

The tower is a chimney. Without dividers, all four pixels merge into one glow and the windows stop reading as windows.

- Card disc at each former, painted matt black on both faces, with only the 12mm wire passage open.
- Stagger the passages so no two line up vertically. A straight 12mm shaft from bottom to top will leak light all the way up.
- Black inside, white only in a 40mm band behind each window.
- Point each pixel at the white band, not at the window.

### Diffusion

Glaze every window with tracing paper or a smear of white PVA on the inside of clear acetate. A bare bullet pixel behind a 16mm opening is a visible dot and reads as an LED instantly, which is the single most common thing that ruins a lit model.

### Brightness

Cap at 60% in software. Four pixels at full white is about 240mA and more light than a matchstick mill should ever emit. The colour you want is deep amber, roughly 255/120/40, not white.

### Routing

The string runs up the central passage alongside the motor wires. Twist the pixel data with its ground, and keep it separated from the four stepper leads where you can — stepper switching on an adjacent unshielded run is the likeliest cause of random pixel flicker in a build like this.

## Electronics

An ESP32-C3 SuperMini is the right board here: 22 x 18mm, about £3, plenty of GPIO for five signals, and native USB-C for flashing. A full ESP32 devkit also works if you'd rather keep one board type across the project.

### Pin allocation

| Pin | To | Notes |
| --- | --- | --- |
| GPIO0 | ULN2003 IN1 |  |
| GPIO1 | ULN2003 IN2 |  |
| GPIO2 | ULN2003 IN3 | Has a boot-strap function, fine as output |
| GPIO3 | ULN2003 IN4 |  |
| GPIO4 | 74AHCT125 input | Pixel data |
| GPIO5 | Button, optional | To GND, internal pullup |
| 5V | USB 5V out | Feeds ULN2003 and pixels |
| GND | Common |  |

### Power chain

| Stage | Part | Output |
| --- | --- | --- |
| Input | USB-C on the C3 SuperMini, from a 5V USB charger | 2A charger recommended |
| Distribution | C3 5V pin to the base box 5V rail | About 4.7-5.0V depending on the board |
| Bulk | 470uF across 5V at the ULN2003 | Absorbs stepper switching |
| Pixel | 1000uF across 5V at the string entry |  |

Peak draw is around 700mA if the pixels ran at full white: roughly 250mA stepper, 240mA pixels, 100mA ESP, plus margin. With the 60% software cap the pixels draw far less, so expect about 450mA. A 5V 2A USB charger is ample, on the bench and in the finished mill.

### The 3.3V data problem

SK6812 pixels want a data high above about 0.7 x VDD, which at 5V is 3.5V. The ESP32-C3 puts out 3.3V. It often works and then intermittently doesn't, usually once everything is glued shut.

Two fixes, either acceptable:

1. **74AHCT125 level shifter.** One gate, 3.3V in, 5V out. Correct, costs about £1, matches the main build.
2. **Diode drop.** A 1N4001 in series with the pixels' 5V feed brings them to about 4.3V, which makes 3.3V a valid high. Fewer parts, well proven for short strings, and fine for four pixels.

Use the shifter if you're building the board anyway. Either way, put a 330R resistor in series with the data line at the string end.

### Layout in the base box

Keep the ULN2003 apart from the ESP, with the ESP's antenna end facing the room rather than buried behind metal. The C3 SuperMini's antenna is its weak point and a 500mm steel-ballasted box is not a friendly place for it. If WiFi is marginal, that's the first thing to move.

## Wiring diagram

```mermaid
flowchart TD
  PSU[5V USB charger] -->|USB-C, cable clamped to box| ESP[ESP32-C3 SuperMini]

  ESP -->|5V pin| RAIL[5V rail]
  RAIL --> ULN[ULN2003 driver<br/>+ 470uF]
  RAIL --> SHIFT[Data-level fix<br/>diode drop or 74AHCT125]

  ESP -->|GPIO0-3| ULN
  ESP -->|GPIO4| SHIFT
  BTN[Button, optional] -->|GPIO5| ESP

  ULN --> J7
  SHIFT --> J7
  RAIL --> J7[7-pin JST-XH<br/>base of mill]

  J7 -->|4 coils| MOT[28BYJ-48<br/>in the cap]
  J7 -->|data, 5V, GND| PIX[4 x SK6812<br/>up the tower]

  MOT --> SHAFT[Coupler, 4mm shaft,<br/>684ZZ bearing, sail hub]
```

Everything above the JST-XH is inside the mill and gets glued in. Everything below it is in the base box and stays serviceable. That connector is the boundary the whole module is designed around, so make it a proper crimped JST-XH with a latch rather than a header strip.

## ESPHome configuration

Complete config for the standalone node. Flash over USB once, then OTA.

```yaml
substitutions:
  name: village-windmill
  friendly: Windmill

esphome:
  name: ${name}
  friendly_name: ${friendly}
  on_boot:
    priority: -100
    then:
      - light.turn_off: mill_lights
      - stepper.report_position:
          id: sails
          position: 0
      - stepper.set_target:
          id: sails
          target: 0

esp32:
  board: esp32-c3-devkitm-1
  framework:
    type: esp-idf

logger:
  baud_rate: 0

api:
  encryption:
    key: !secret api_key
ota:
  - platform: esphome
    password: !secret ota_password

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
  ap:
    ssid: "Windmill Fallback"
    password: !secret ap_password

captive_portal:

# ---------- Sails ----------
stepper:
  - platform: uln2003
    id: sails
    pin_a: GPIO0
    pin_b: GPIO1
    pin_c: GPIO2
    pin_d: GPIO3
    max_speed: 170 steps/s
    step_mode: FULL_STEP
    sleep_when_done: false

globals:
  - id: sails_running
    type: bool
    restore_value: true
    initial_value: "false"

number:
  - platform: template
    name: "Sail Speed"
    id: sail_speed
    min_value: 60
    max_value: 320
    step: 10
    initial_value: 170
    unit_of_measurement: "steps/s"
    optimistic: true
    restore_value: true
    on_value:
      - stepper.set_speed:
          id: sails
          speed: !lambda "return x;"

switch:
  - platform: template
    name: "Sails Turning"
    id: sails_turn
    lambda: 'return id(sails_running);'
    restore_mode: ALWAYS_OFF
    turn_on_action:
      - lambda: 'id(sails_running) = true;'
      - stepper.set_speed:
          id: sails
          speed: !lambda "return id(sail_speed).state;"
      - stepper.set_target:
          id: sails
          target: 2000000000
    turn_off_action:
      - lambda: |-
          id(sails_running) = false;
          id(sails).set_target(id(sails).current_position);

# ---------- Lights ----------
light:
  - platform: esp32_rmt_led_strip
    id: mill_pixels
    name: "Mill Pixels"
    internal: true
    pin: GPIO4
    num_leds: 4
    rmt_channel: 0
    chipset: SK6812
    rgb_order: GRB
    restore_mode: ALWAYS_OFF

  - platform: partition
    name: "Mill Interior"
    id: mill_interior
    default_transition_length: 3s
    segments:
      - id: mill_pixels
        from: 0
        to: 2
    effects:
      - flicker:
          name: Lamplight
          alpha: 92%
          intensity: 18%

  - platform: partition
    name: "Mill Door Lamp"
    id: mill_lamp
    default_transition_length: 3s
    segments:
      - id: mill_pixels
        from: 3
        to: 3

  - platform: partition
    name: "Mill Lights"
    id: mill_lights
    default_transition_length: 3s
    segments:
      - id: mill_pixels
        from: 0
        to: 3

# ---------- Local control ----------
binary_sensor:
  - platform: gpio
    name: "Mill Button"
    pin:
      number: GPIO5
      mode:
        input: true
        pullup: true
      inverted: true
    on_click:
      - min_length: 50ms
        max_length: 500ms
        then:
          - script.execute: toggle_mill
      - min_length: 1s
        max_length: 5s
        then:
          - script.execute: mill_off

script:
  - id: mill_on
    then:
      - light.turn_on:
          id: mill_interior
          brightness: 60%
          red: 100%
          green: 47%
          blue: 16%
          effect: Lamplight
      - light.turn_on:
          id: mill_lamp
          brightness: 50%
          red: 100%
          green: 55%
          blue: 22%
      - switch.turn_on: sails_turn

  - id: mill_off
    then:
      - switch.turn_off: sails_turn
      - light.turn_off: mill_lights

  - id: toggle_mill
    then:
      - if:
          condition:
            light.is_on: mill_interior
          then:
            - script.execute: mill_off
          else:
            - script.execute: mill_on
```

### Notes

- `logger: baud_rate: 0` disables serial logging. On the C3 the USB serial shares pins with normal operation and leaving it on can cause odd behaviour once unplugged.
- The sails run by setting the target to 2 billion steps. At 170 steps/s that lasts about four months of continuous running, so in practice it never reaches it. Turning off sets the target to the current position, which stops it immediately without a deceleration ramp.
- `internal: true` on the pixel strip hides the raw 4-pixel entity from Home Assistant so only the three meaningful groups appear.
- Both lights are off at boot and the sails are stopped. After a power cut you want a dark, still mill, not a motor running unattended.

## Mounting

455mm of matchstick on a 150mm-wide board is top-heavy, and it's the thing most likely to be knocked from the room side. The mounting has to be structural, not scenic.

### The stack

| Layer | Part |
| --- | --- |
| Mill | Base former + 4mm ply disc glued under it |
| Landscape | Papier-mâché, with a let-in ply pad flush to the surface |
| Pad | 6mm ply or hardwood disc, 130mm, bedded on the baseboard |
| Baseboard | 12mm ply lid |
| Box | 58mm frame, cross-brace directly under the pad |

Three M4 bolts on a 90mm circle, up through the ply lid into threaded inserts in the mill's base disc. Bolt heads accessible from inside the box.

**The mâché must not be structural.** Let the ply pad in flush before the landform goes on, and mâché up to it. If the mill bolts to paper it will loosen.

### Ballast

A 2kg steel plate in the bottom of the box directly under the mill. Cheap, invisible, and it changes the board from something that tips to something that doesn't. Worth doing before you find out the other way.

### Cable route

A 20mm brass or plastic tube through the ply pad and the ply lid, standing 2mm proud of the finished landscape, inside the mill's footprint. The 7-pin JST-XH lives in the box below it. Fit the tube before any mâché — a hole covered by papier-mâché is a hole you will never find again.

### Access

The mill lifts off by removing three bolts from inside the box and unplugging one connector. Nothing else is attached. Keep it that way: don't glue the mill's base into the landscape, and leave a 1mm shadow gap around it filled with snow or loose ground cover rather than adhesive.

### Standing it before the board exists

For the bench phase, make a temporary base: an offcut of 18mm ply, 300 x 200mm, with the same three-bolt pattern and a hole for the cable. Sits on the workbench, takes the same wiring, lets you run the mill for hours while the rest of the project doesn't exist yet.

## Build and test order

The governing rule: **everything electrical must work on the bench before anything is glued shut.** There is no access afterwards.

### Phase 1, electronics on the bench

- [ ] Flash the C3, confirm WiFi, API and the fallback AP
- [ ] Wire the ULN2003 and a bare 28BYJ-48, confirm rotation both directions
- [ ] Sweep the speed number from 60 to 320, find where it starts missing steps
- [ ] Wire four pixels on the bench, confirm all four address correctly
- [ ] Run the stepper and pixels together for an hour, watch for pixel flicker
- [ ] Confirm the 5V rail stays above 4.5V with both running
- [ ] Confirm boot state is dark and stopped after a power cut

If pixel flicker appears only when the stepper runs, that's the data line picking up switching noise. Fix it now with separation and a 330R, not later.

### Phase 2, the drive mock-up

- [ ] Card mock-up of the cap interior, check the 28BYJ-48 actually fits
- [ ] Assemble bearing, shaft and coupler on a scrap ply plate
- [ ] Run for an hour, check the bearing stays cool and free

### Phase 3, kit assembly

- [ ] Open all formers, 12mm passages, staggered
- [ ] Paint interiors black, white bands at pixel positions
- [ ] Ply disc under the base former, threaded inserts fitted
- [ ] Build the tower, feeding the pixel string and motor cable as you go
- [ ] Build the cap as a removable sub-assembly with the motor in it
- [ ] Cut the cap vents
- [ ] Build sails on the new ply hub, weigh and balance before fitting

### Phase 4, integration

- [ ] Temporary ply base, mill bolted down, JST-XH connected
- [ ] Full run: sails turning, all four pixels, for three hours
- [ ] Check cap temperature by hand after the run
- [ ] Check sail balance by eye at three different speeds
- [ ] Leave it running an evening in the room it will live in, and look at it from across the room and from outside the window

That last test is the one that tells you whether the brightness, colour and sail speed are right. Everything else can be measured; those three can only be judged.

## Migration to the main system

### Option A, keep the ESP

Leave the mill on its own USB charger. If a 12V bus is ever built, a 12V-to-USB module in a junction box can feed the same cable. That is the entire job.

The windmill stays a separate node in Home Assistant, appearing as its own device alongside the main hub. Scenes coordinate the two from HA. This is the better option unless you have a specific reason to consolidate: the mill keeps working while you rebuild everything else, it keeps working if the hub fails, and its stepper timing isn't competing with a hundred pixels for the same processor.

### Option B, fold into the main hub

Inside the base box:

1. Unplug the 7-pin JST-XH from the module board.
2. Extend those seven conductors to the nearest junction box on a single cable.
3. At the hub, the four coil wires go to a spare ULN2003, the data line to a spare RMT channel, 5V and GND to the bus.
4. Move the stepper and light blocks from this config into the hub's config, renaming ids to avoid collisions.
5. Remove the C3 and the data-level fix from the box, or leave them in place unpowered. The hub then supplies 5V to the 7-pin connector.

The one thing to watch: stepper coil signals over a run of a metre or more are more susceptible to noise than you'd expect. Use twisted pairs and keep the run away from pixel data. If the sails start stuttering after the move, that's why.

### Which to choose

|  | Option A | Option B |
| --- | --- | --- |
| Work involved | One plug | Rewire and reconfigure |
| Fails independently | Yes | No |
| Hub GPIO used | None | 5 pins |
| Extra device in HA | Yes | No |
| Cost | £3 ESP stays in use | £3 recovered |

Option A, unless the extra device in Home Assistant genuinely bothers you. The £3 isn't worth the rework or the loss of independence.

## Bill of materials

Approximate UK prices from memory, so budget rather than quote. Around £55 all in, including the kit.

| Qty | Item | Approx £ |
| --- | --- | --- |
| 1 | Matchmaker MM02 windmill kit | 15 |
| 1 | ESP32-C3 SuperMini | 3 |
| 1 | 28BYJ-48 stepper + ULN2003 board | 4 |
| 1 | 74AHCT125 | 1 |
| 4 | SK6812 12mm bullet pixels, 100mm pitch | 4 |
| 1 | 684ZZ bearing, 4 x 9 x 4mm | 3 |
| 1 | Shaft coupler, 5mm to 4mm | 3 |
| 1 | 4mm brass tube, 300mm | 3 |
| 1 | JST-XH 7-pin kit with crimps | 4 |
| 1 | 2kg steel ballast plate | 6 |
| 1 | 5V 2A USB charger and USB-A to USB-C cable | 6 |
| - | 4mm and 6mm ply offcuts, M4 inserts and bolts | 5 |
| - | Capacitors (470uF, 1000uF, 16V), 330R, cable clamp, wire | 3 |

### Worth buying spares of

- **A second 28BYJ-48 and ULN2003.** They come in packs anyway, they're £2 each, and discovering a dead one after the cap is built is a bad afternoon.
- **Extra pixels.** Bullet strings are sold in tens. You will kill one soldering.
- **JST-XH crimps.** You will ruin several learning the tool.

### Not on this list

Junction boxes and the base box itself are part of the main build rather than this module.

## Open questions

### Measure before you buy anything

- **Clear glass height above the bottom frame.** The mill needs about 537mm from the sill including the box. This is the one that can stop the whole design.
- **Cap interior width.** Decides whether the 28BYJ-48 fits or whether you need an N20 gearmotor with a worm reduction instead.
- **Base former diameter.** Over 120mm and the board cross-section needs revisiting.
- **Whether the kit's sails are separate stocks or a single moulded assembly.** Changes how the replacement hub is made.

### Decide as you build

- **Does the cap actually need to be removable, or is a hatch in the tower easier?** If the cap is too small for the motor, a service hatch in the rear of the tower at hub height is the fallback, hidden from side A.
- **Sail speed.** Set it by eye in the room. Anything I suggest is a starting point.
- **Whether the mill gets a fifth pixel later.** The string is easier to extend now than to rebuild. If you might want the sails lit, run a spare conductor up the tower while it's open.

### Known risks

| Risk | Likelihood | Mitigation |
| --- | --- | --- |
| Motor doesn't fit the cap | Medium | Card mock-up before building |
| Sail wobble from imbalance | High | Balance on a pin before fitting |
| Pixel flicker when stepper runs | Medium | Separation, 330R, test in phase 1 |
| Cap runs warm over a season | Medium | Vents, scheduled hours, not unattended |
| C3 WiFi weak in the box | Low | Reposition, or swap to a full ESP32 |
| USB cable tug damages the C3 socket | Medium | Clamp the cable to the base box |
| C3 resets when the stepper starts | Low | 470uF at the ULN2003; 2A charger, not a laptop port |
