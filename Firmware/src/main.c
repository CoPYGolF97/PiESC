// PiESC_fw  ขั้นที่ 6: คอมมิวเทต 6 สเต็ป (open-loop ช้าๆ, ไม่ต่อมอเตอร์)
// เฟส A=GPIO0/1, B=GPIO2/3, C=GPIO4/5  (HIN/LIN)
// สถานะของเฟส: PWM_HI (HIN ทำ PWM + LIN เติมเต็มพร้อม dead-time),
//              LOW_ON (LIN เปิดค้าง HIN ปิด), FLOAT (ปิดทั้งคู่)
// ห้ามต่อ MOSFET/แบตเตอรี่จริงจนกว่าจะวัด dead-time ด้วยออสซิลโลสโคปแล้ว
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

#define LED_PIN    25
#define PWM_HZ     20000
#define DUTY_PCT   25
#define DEAD_NS    500
#define STEP_MS    500         // เปลี่ยนสเต็ปทุกกี่ ms (ช้าไว้เพื่อดูด้วยมิเตอร์)

typedef enum { PH_FLOAT, PH_LOW, PH_PWM } phase_state_t;

static uint32_t g_period;       // = wrap + 1
static uint32_t g_dead;         // dead-time เป็นจำนวน tick
static uint     g_slice[3];

static void pwm_init_phase(int ph) {
    uint pin_hi = ph * 2;
    gpio_set_function(pin_hi,     GPIO_FUNC_PWM);
    gpio_set_function(pin_hi + 1, GPIO_FUNC_PWM);
    uint s = pwm_gpio_to_slice_num(pin_hi);
    g_slice[ph] = s;
    pwm_set_phase_correct(s, true);
    pwm_set_wrap(s, g_period - 1);
    pwm_set_output_polarity(s, false, true);   // ช่อง B (LIN) กลับขั้ว
}

static void set_phase(int ph, phase_state_t st, uint32_t duty_pct) {
    uint s = g_slice[ph];
    uint32_t a, b;
    switch (st) {
    case PH_PWM:
        a = g_period * duty_pct / 100;
        b = a + g_dead;
        if (b > g_period) b = g_period;
        break;
    case PH_LOW:    a = 0; b = 0;        break;   // HIN ปิด, LIN เปิดค้าง
    default:        a = 0; b = g_period; break;   // ปิดทั้งคู่ (ลอย)
    }
    pwm_set_chan_level(s, PWM_CHAN_A, a);
    pwm_set_chan_level(s, PWM_CHAN_B, b);
}

// ลำดับ 6 สเต็ป: {A, B, C}
static const phase_state_t STEPS[6][3] = {
    { PH_PWM,   PH_LOW,   PH_FLOAT },  // 0: A+ B-
    { PH_PWM,   PH_FLOAT, PH_LOW   },  // 1: A+ C-
    { PH_FLOAT, PH_PWM,   PH_LOW   },  // 2: B+ C-
    { PH_LOW,   PH_PWM,   PH_FLOAT },  // 3: B+ A-
    { PH_LOW,   PH_FLOAT, PH_PWM   },  // 4: C+ A-
    { PH_FLOAT, PH_LOW,   PH_PWM   },  // 5: C+ B-
};

int main(void) {
    stdio_init_all();
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    sleep_ms(2000);

    uint32_t clk = clock_get_hz(clk_sys);
    g_period = clk / (2u * PWM_HZ);
    g_dead   = (uint32_t)((uint64_t)clk * DEAD_NS / 1000000000ull);

    for (int p = 0; p < 3; p++) {
        pwm_init_phase(p);
        set_phase(p, PH_FLOAT, 0);       // เริ่มต้นปลอดภัย: ทุกอย่างปิด
        pwm_set_enabled(g_slice[p], true);
    }

    int step = 0;
    while (true) {
        for (int p = 0; p < 3; p++) set_phase(p, STEPS[step][p], DUTY_PCT);
        gpio_xor_mask(1u << LED_PIN);
        printf("step %d\n", step);
        step = (step + 1) % 6;
        sleep_ms(STEP_MS);
    }
}