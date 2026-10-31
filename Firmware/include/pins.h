#pragma once
/* แผนที่ขา GPIO ของ PiESC_1CH_Test (อ่านจาก netlist PCB ล่าสุด 2026-10-20)
 * RP2350B QFN-80: ขาตัวถัง -> GPIO  (ขา 77-80 = GPIO0-3, ขา 1-3 = GPIO4-6 ...) */

/* เกตไดรเวอร์ U5 (JSM6288Q) */
#define PIN_HIN_A   0   /* ขา 77 */
#define PIN_LIN_A   1   /* ขา 78 */
#define PIN_HIN_B   2   /* ขา 79 */
#define PIN_LIN_B   3   /* ขา 80 */
#define PIN_HIN_C   4   /* ขา 1  */
#define PIN_LIN_C   5   /* ขา 2  */

#define PIN_LED     25  /* ขา 26, LED บนบอร์ด */

#define PIN_UART_TX 18  /* ขา 18 ไป FC (อาจไม่ใช่ขา UART TX ฮาร์ดแวร์ ตรวจในตารางฟังก์ชัน) */
#define PIN_M1_IN   28  /* ขา 36 สัญญาณจาก FC */

/* ADC (ต้องตั้ง PICO_RP2350A=0) */
#define PIN_ISENSE  40  /* ขา 49, ADC0 */
#define PIN_VBAT    41  /* ขา 52, ADC1 */
#define PIN_BEMF_A  42  /* ขา 53, ADC2 */
#define PIN_BEMF_B  43  /* ขา 54, ADC3 */
#define PIN_BEMF_C  44  /* ขา 55, ADC4 */
