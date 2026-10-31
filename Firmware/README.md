# PiESC firmware (experimental)

Tested on a Raspberry Pi Pico 2 dev board only. Not yet tested on a motor.

Done: LED blink, USB serial, 3 complementary PWM pairs at 20 kHz with dead-time, open-loop 6-step commutation.
Not done: DShot input, back-EMF sensing, current sensing, protection, telemetry.

Build: PlatformIO with the Pico SDK (see platformio.ini).
