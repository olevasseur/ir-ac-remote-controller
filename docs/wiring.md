# Wiring

## Parts

| Qty | Part |
|---|---|
| 1 | ESP32 dev board (ESP-WROOM-32, 30-pin DevKit) |
| 1 | 5 mm 940 nm IR LED |
| 1 | 2N2222A NPN transistor |
| 1 | 1 kΩ resistor (base) |
| 1 | 100 Ω resistor (LED series) |
| 1 | Breadboard, jumper wires |
| 1 | 5 V USB supply or power bank |

## Circuit

The transistor is a low-side switch. GPIO18 drives its base through 1 kΩ. The IR LED and its 100 Ω resistor hang between 5 V (VIN) and the collector.

```
 ESP32 VIN (5 V) ---[100 Ω]---|>|--- collector
                           IR LED     |
                    (anode)  (cathode)|
                                    2N2222A
 ESP32 GPIO18 ---[1 kΩ]------------ base
                                      |
 ESP32 GND ------------------------ emitter
```

| From | To |
|---|---|
| ESP32 VIN (5 V) | 100 Ω resistor |
| 100 Ω resistor | IR LED anode (long leg) |
| IR LED cathode (short leg) | 2N2222A collector |
| ESP32 GPIO18 (D18) | 1 kΩ resistor → 2N2222A base |
| 2N2222A emitter | ESP32 GND |

## Transistor pinout

Check your part's datasheet: pinouts vary between makers and packages. For the 2N2222A used here, it was:

- Hold it flat face down, with the legs pointing away from you.
- **Left** leg is the collector (to the LED cathode).
- **Middle** leg is the base (to the 1 kΩ resistor).
- **Right** leg is the emitter (to GND).

If the LED doesn't flash, swap the collector and emitter wires. That fixed it on the first build.

## Checks

- You can see the LED flash through a phone camera when you tap a button. Some cameras filter IR; front cameras often don't.
- Use 100 Ω. A smaller resistor gives more range but more LED current: keep sends short, and stop if the LED, transistor or resistor gets warm.
- Never connect the IR LED straight to a GPIO pin.
