# PiESC

**An independent open-source ESC project for RP2350.**

---

## 🇬🇧 English

PiESC is an open-source project exploring the design and development of Electronic Speed Controller (ESC) firmware and hardware using the RP2350 microcontroller.

The goal is to create another open-source option for ESC development, learning, experimentation, and future community collaboration.

### Project Status

🚧 **Early Development**

PiESC is currently an experimental project.

The hardware and firmware are being developed and tested step by step. Features, circuit designs, firmware architecture, and documentation may change during development.

### Hardware

The initial development platform uses the **RP2350** microcontroller.

Development hardware is purchased independently by the project creator.

PiESC is an independent project and is **not affiliated with, sponsored by, or endorsed by Raspberry Pi Ltd.**

### Firmware

The firmware is being developed with a focus on:

* 3-phase BLDC motor control
* PWM generation
* Motor commutation
* MOSFET gate-driver control
* Current sensing
* Motor protection
* Future sensorless control
* Future ESC telemetry and configuration

### Open Source

PiESC is intended to be an open-source project.

We welcome developers, electronics enthusiasts, researchers, makers, and anyone interested in motor control to explore the project and contribute ideas, testing, documentation, hardware designs, or code.

### Development

The project is being developed using:

* RP2350 / Raspberry Pi Pico 2
* PlatformIO
* Pico SDK
* C/C++
* KiCad

### A Personal Project

PiESC was started by an ordinary electronics enthusiast who wanted to learn, experiment, and build something that others could continue developing.

The project creator is not a professional software engineer or electrical engineer. Much of the development process is based on learning through experimentation, documentation, community knowledge, and AI-assisted development.

**ChatGPT is used as an AI development assistant for programming, debugging, architecture discussions, documentation, and technical exploration.**

The goal is not to claim that everything is perfect.

The goal is to build something useful, learn along the way, and make the work available for others to improve.

---

## 🇹🇭 ภาษาไทย

PiESC คือโปรเจกต์ Open Source ที่กำลังศึกษาและพัฒนา Firmware และ Hardware สำหรับ Electronic Speed Controller (ESC) โดยใช้ไมโครคอนโทรลเลอร์ RP2350

เป้าหมายของเราคือการสร้างทางเลือกอีกทางหนึ่งสำหรับการพัฒนา ESC แบบ Open Source เพื่อการเรียนรู้ การทดลอง และเปิดโอกาสให้ชุมชนเข้ามาช่วยพัฒนาต่อในอนาคต

### สถานะโปรเจกต์

🚧 **อยู่ในช่วงเริ่มต้นของการพัฒนา**

ขณะนี้ PiESC ยังเป็นโปรเจกต์สำหรับการทดลองและพัฒนา

Hardware และ Firmware กำลังถูกพัฒนาและทดสอบทีละขั้นตอน ดังนั้นคุณสมบัติ วงจร โครงสร้าง Firmware และเอกสารต่าง ๆ อาจมีการเปลี่ยนแปลงระหว่างการพัฒนา

### Hardware

แพลตฟอร์มเริ่มต้นของโปรเจกต์ใช้ไมโครคอนโทรลเลอร์ **RP2350**

Hardware ที่ใช้ในการพัฒนาถูกจัดซื้อโดยผู้ริเริ่มโปรเจกต์เอง

PiESC เป็นโปรเจกต์อิสระ และ **ไม่มีความเกี่ยวข้อง ได้รับการสนับสนุน หรือได้รับการรับรองจาก Raspberry Pi Ltd.**

### Firmware

Firmware กำลังได้รับการพัฒนาโดยมุ่งเน้นไปที่:

* การควบคุมมอเตอร์ BLDC แบบ 3 เฟส
* การสร้าง PWM
* การ Commutation ของมอเตอร์
* การควบคุม MOSFET Gate Driver
* การวัดกระแส
* ระบบป้องกันมอเตอร์และ ESC
* การควบคุมแบบ Sensorless ในอนาคต
* Telemetry และการตั้งค่า ESC ในอนาคต

### Open Source

PiESC ตั้งใจให้เป็นโปรเจกต์ Open Source

เรายินดีต้อนรับนักพัฒนา ผู้สนใจอิเล็กทรอนิกส์ นักวิจัย Maker และทุกคนที่สนใจด้านการควบคุมมอเตอร์ ให้เข้ามาศึกษา ทดลอง เสนอแนวคิด ทดสอบ ปรับปรุงเอกสาร Hardware หรือ Firmware และช่วยพัฒนาโปรเจกต์ต่อ

### เครื่องมือที่ใช้ในการพัฒนา

โปรเจกต์นี้ใช้:

* RP2350 / Raspberry Pi Pico 2
* PlatformIO
* Pico SDK
* C/C++
* KiCad

### จุดเริ่มต้นของโปรเจกต์

PiESC เริ่มต้นจากคนธรรมดาที่สนใจอิเล็กทรอนิกส์ และต้องการเรียนรู้ ทดลอง และสร้างสิ่งที่คนอื่นสามารถเข้ามาช่วยพัฒนาต่อได้

ผู้ริเริ่มโปรเจกต์ไม่ได้เป็นวิศวกรซอฟต์แวร์หรือวิศวกรไฟฟ้ามืออาชีพ การพัฒนาส่วนหนึ่งเกิดจากการเรียนรู้ผ่านการทดลอง การศึกษาข้อมูล ความรู้จากชุมชน และการใช้ AI ช่วยในการพัฒนา

**ChatGPT ถูกใช้เป็นผู้ช่วยด้าน AI สำหรับการเขียนโปรแกรม การแก้ไขข้อผิดพลาด การออกแบบโครงสร้าง การจัดทำเอกสาร และการพูดคุยด้านเทคนิค**

เราไม่ได้อ้างว่าโปรเจกต์นี้สมบูรณ์แบบ

เป้าหมายคือการสร้างสิ่งที่มีประโยชน์ เรียนรู้ไปพร้อมกับการพัฒนา และเปิดผลงานให้ผู้อื่นสามารถเข้ามาช่วยปรับปรุงต่อได้

---

**PiESC — Build it. Test it. Improve it.**

**PiESC — สร้าง ทดลอง และพัฒนาไปด้วยกัน**
