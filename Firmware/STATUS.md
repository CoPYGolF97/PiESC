# Status (2026-11-24, firmware v7.16)

## Tested
- Perfboard rig: Pico 2 + FD6288T + IRLZ44N, 12 V (1.5 A adapter), EMAX RS2205 with a 3-blade propeller.
- SINE drive: smooth start, soft stop, fast throttle up/down without stumbling, direction reversal, saved settings.
- DShot300 from a Betaflight FC (F722) on GP6.

## Known issues
- Rarely the motor stalls briefly (hum) and recovers when the throttle is moved. Not diagnosed yet.
- Open loop only: no rotor position feedback, so no recovery from lost sync.
- No overcurrent / undervoltage / temperature protection.
- Gate driver gets warm in SINE (all six MOSFETs switch at 20 kHz); MOSFETs stay cool.
- Top speed in open loop is about 2900 rpm (step time 500 us); real ESCs run much faster.
- Back-EMF from the perfboard rig was not usable (noise); to be done on a real PCB.

## Next
- Input protocol auto-detect (DShot150/300/600 + PWM).
- Configurator (serial protocol, then web page): protocol, direction per motor, saved settings.
- Closed loop (BEMF) and protection on a real PCB; 2 motors per MCU; 4-in-1 with two MCUs.
