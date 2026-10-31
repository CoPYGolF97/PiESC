# Contributing to PiESC

Thank you for your interest! PiESC is an early-stage, hobbyist, open-source ESC project built around the RP2350. The hardware and firmware are **incomplete and untested on a motor**, so help of any size is welcome: reviews, ideas, tests, documentation, code.

## Ways to help
- **Review** the schematics and PCB layouts and point out mistakes (open an Issue).
- **Write or improve firmware**: DShot input, sensorless control, protection, telemetry.
- **Test** a board if you build one. Please share what worked and what did not.
- **Improve documentation**: pinouts, bring-up guides, translations.

See the Issues tab for specific tasks, especially those labelled `help wanted` and `good first issue`.

## Before you start
- For anything bigger than a small fix, **open an Issue first** so we can agree on the approach.
- This hardware handles high current from lithium batteries. **Never test unattended**; use a current-limited supply first.
- Please do not copy code or designs from projects whose license is incompatible with GPL-3.0.

## Pull requests
1. Fork the repository and create a branch.
2. Keep changes focused, and describe what you changed and how you tested it.
3. For firmware, say which board/MCU you tested on (Pico 2 dev board, 1CH Test, 4-in-1).
4. For hardware, include the KiCad version and the result of the DRC.

## License
By contributing, you agree that your contribution is licensed under the project license (see `LICENSE`).

---

## ภาษาไทย (สรุป)
PiESC เป็นโปรเจกต์ ESC โอเพนซอร์สงานอดิเรกที่ยังอยู่ช่วงเริ่มต้น ฮาร์ดแวร์และเฟิร์มแวร์ยังไม่สมบูรณ์และยังไม่ได้ทดสอบกับมอเตอร์ ยินดีรับความช่วยเหลือทุกขนาด: ตรวจแบบ เขียนเฟิร์มแวร์ ทดสอบ ปรับปรุงเอกสาร ดูงานเฉพาะเจาะจงได้ที่แท็บ Issues (ป้าย `help wanted`, `good first issue`) ถ้าจะทำงานใหญ่ ให้เปิด Issue คุยกันก่อน และห้ามทดสอบกระแสสูงโดยไม่มีคนเฝ้า ผู้ร่วมพัฒนาตกลงให้ผลงานอยู่ภายใต้ใบอนุญาตของโปรเจกต์ (ไฟล์ `LICENSE`)
