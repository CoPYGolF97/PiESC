// PiESC-FW test rig v7 - open-loop 6-step, complementary PWM, throttle from keyboard / DShot / PWM.
// One file. Pins: A: HIN=GP0 LIN=GP1   B: HIN=GP2 LIN=GP3   C: HIN=GP4 LIN=GP5
// Throttle input from the flight controller: GP6 (DShot150/300/600 or 1-2 ms PWM). FC GND -> Pico GND.
//
// The motor is stepped from a timer interrupt, so printing to USB or reading the FC can never delay a step.
//
// Keys (serial monitor):
//   m = input mode: KEYS -> DSHOT -> PWM -> KEYS ...   (starts in KEYS, nothing spins on power-up)
//   x = EMERGENCY STOP (all off, disarm)               ? = status
//   r = reverse direction (only while stopped)
//   KEYS mode only:  s = start   + / - = duty +-1%   f / g = faster / slower step time
//                    n = one step pulse   h = hold one step (motor disconnected only)
//   d = drive type: 6-STEP <-> SINE (only while stopped). SINE = sinusoidal open loop, smoother at low speed.
//   a = read the three phase voltages while stopped (BEMF divider check)
//   o = "scope": while running in 6-STEP, waits for the next step, then records the floating phase
//       for one step at 2 us per sample (ADC free running) and prints it. Paste the output to Claude.
//
// DSHOT / PWM mode: the motor arms after the throttle has been 0 (DShot) / <=1050 us (PWM) for 0.5 s.
//   Throttle then sets step time and duty together. Signal lost for 100 ms -> motor stops and disarms.
//   DShot commands 7 / 8 (sent repeatedly by Betaflight's motor direction buttons) set direction while stopped.

#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"
#include "hardware/pio.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/flash.h"

// ---- settings you may change ----------------------------------------------
#define PWM_FREQ_HZ        20000
#define DEAD_NS            500       // dead time between HIN off and LIN on (and back)
#define MAX_DUTY_PM        250       // hard cap, per mille (250 = 25 %)
#define DEFAULT_DUTY_PM    40        // KEYS mode start duty (4 %)
#define START_STEP_US      3500      // first step time at start (v7.9: skip the slow stepping region)
#define START_BOOST_PM    100       // duty at the moment of start (10 %), then it slews down
#define DEFAULT_TARGET_US  4000      // KEYS mode target step time
#define MIN_TARGET_US      750       // fastest step time f-key allows (500 did not work in open loop)
#define MAX_TARGET_US      20000
#define HOLD_FIRST_US      250000    // hold the first step so the rotor can swing in and settle (v7.8: was 80 ms)
#define ACCEL_SPS2         1500.0f   // speed-up: step rate may rise this many steps/s per second (v7.8, constant acceleration)
#define DECEL_SPS2         2500.0f   // slow-down: step rate may fall this fast
#define ACCEL_BOOST_PM     20        // duty above the speed-matched duty while speeding up (2 %)
#define SLEW_US            1000      // duty moves 0.1 % per this many us toward the wanted value
#define PULSE_MS           250       // length of one 'n' pulse

#define IN_PIN             6         // throttle input from the FC
#define DSHOT_RATE         300       // 150, 300 or 600
#define THR_MAP_SLOW_US    2000      // step time at the lowest running throttle (v7.9: idle about 710 rpm, below that open loop is jerky)
#define THR_MAP_FAST_US    1000      // step time at full throttle (open loop limit, about 950 rpm)
#define THR_DUTY_LO_PM     40        // duty at the lowest running throttle (4 %)
#define THR_DUTY_HI_PM     100       // duty at full throttle (10 %)
#define START_U_PM         30        // throttle (per mille) needed to start
#define STOP_U_PM          15        // throttle (per mille) below which the motor stops
#define ARM_MS             500       // throttle must stay at zero this long to arm
#define SIGNAL_TIMEOUT_MS  100       // no valid frame for this long = failsafe

// sine drive (open loop, no rotor position feedback)
#define SINE_ALIGN_PM      100       // amplitude while the rotor is aligned before the sine starts turning (10 %)
#define SINE_START_PM      150       // v7.16: starting kick (15 %) when the field starts turning, slews down 0.1 %/ms like v7.12 did
#define SINE_PARK_US       20000     // v7.15: soft stop in SINE slows the field down to this step time (~8 Hz) ...
#define SINE_PARK_HOLD_MS  150       // ... and holds it still this long, so the rotor stops where the field is
#define SINE_UPDATE_US     100       // sine update period (10 kHz)
#define SINE_START_HZ      5.0f      // electrical frequency at start
#define SINE_ACCEL_HZ_PER_S 300.0f   // how fast the electrical frequency may change
#define SINE_FAST_US       500       // v7.12: "step time" at full throttle in SINE (500 = about 2860 rpm; v7.11 had 600)
#define SINE_M_LO_PM       50        // amplitude at the lowest running throttle (5 %)
#define SINE_M_HI_PM       230       // amplitude at full throttle (23 %), kept in step with the speed (v7.11 had 19 %)
#define SINE_TABLE_N       1024

#define DIV_TOP_OHM        10000.0f  // phase divider: phase -> 10k -> node -> 2.2k -> GND
#define DIV_BOT_OHM        2200.0f
// ---------------------------------------------------------------------------

#define HIN(p) ((p) * 2)
#define LIN(p) ((p) * 2 + 1)

// {high phase, low phase}, A=0 B=1 C=2
static const int STEPS[6][2] = {{0, 1}, {0, 2}, {1, 2}, {1, 0}, {2, 0}, {2, 1}};

enum Mode { M_KEYS = 0, M_DSHOT = 1, M_PWM = 2 };
static const char *MODE_NAME[3] = {"KEYS", "DSHOT", "PWM"};

static uint32_t wrap_val;
static uint32_t dead_ticks;

// shared between main loop and interrupts
static volatile uint32_t duty_pm = DEFAULT_DUTY_PM;   // duty actually applied
static volatile uint32_t duty_cmd = DEFAULT_DUTY_PM;  // duty asked for
static volatile uint32_t target_us = DEFAULT_TARGET_US;
static volatile uint32_t period_us = START_STEP_US;
static volatile int dir = 1;
static volatile int idx = 0;
static volatile bool running = false;
static volatile bool want_run = false;
static volatile bool sine_on = true;      // drive type: false = 6-step, true = sine (default SINE since v7.13)
static repeating_timer_t sine_timer;
static int16_t sine_tab[SINE_TABLE_N];
static volatile uint32_t sine_hold = 0;   // sine update ticks left in the alignment hold
static uint32_t theta = 0;                 // electrical angle, 2^32 = one turn
static float f_cur = SINE_START_HZ;        // electrical frequency now
static alarm_id_t step_alarm = 0;
static repeating_timer_t slew_timer;

// main loop only
static Mode mode = M_KEYS;
static bool armed = false;
static bool arm_timing = false;
static uint32_t arm_since_ms = 0;
static uint32_t ds_cmd_count = 0, ds_cmd_last = 0;
static bool stopping = false;   // throttle dropped: slowing down first, then cutting
static bool rotor_parked = false;   // true after a soft stop in SINE: rotor sits where the field stopped
static uint32_t park_ms = 0;

// ---------------------------------------------------------------------------
// Saved settings (direction + drive type) in the last 4 KB sector of the flash, so they survive power off.
// Written only while the motor is stopped (flash erase stops the CPU for tens of milliseconds).
#define SETTINGS_MAGIC 0x50694554u    // "PiET" (layout with input mode)
#define SETTINGS_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)

static bool settings_read(uint32_t *d, uint32_t *t, uint32_t *m) {
    const uint32_t *p = (const uint32_t *)(XIP_BASE + SETTINGS_OFFSET);
    if (p[0] != SETTINGS_MAGIC) return false;
    if (p[4] != (p[0] ^ p[1] ^ p[2] ^ p[3] ^ 0xA5A5A5A5u)) return false;
    if ((p[1] != 1 && p[1] != 2) || p[2] > 1 || p[3] > 2) return false;
    *d = p[1]; *t = p[2]; *m = p[3];
    return true;
}

static uint32_t boot_mode = M_DSHOT;   // input mode after power-up (default DSHOT; needs 0.5 s zero throttle to arm)

static void settings_load() {
    uint32_t d, t, m;
    if (settings_read(&d, &t, &m)) { dir = (d == 1) ? 1 : -1; sine_on = (t == 1); boot_mode = m; }
}

static void settings_save() {
    if (running) return;
    uint32_t d = (dir > 0) ? 1 : 2, t = sine_on ? 1 : 0, m = (uint32_t)mode;
    uint32_t od, ot, om;
    if (settings_read(&od, &ot, &om) && od == d && ot == t && om == m) return;   // nothing changed
    static uint32_t page[FLASH_PAGE_SIZE / 4] __attribute__((aligned(4)));
    for (unsigned i = 0; i < FLASH_PAGE_SIZE / 4; i++) page[i] = 0xFFFFFFFFu;
    page[0] = SETTINGS_MAGIC; page[1] = d; page[2] = t; page[3] = m; page[4] = SETTINGS_MAGIC ^ d ^ t ^ m ^ 0xA5A5A5A5u;
    uint32_t s = save_and_disable_interrupts();
    flash_range_erase(SETTINGS_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(SETTINGS_OFFSET, (const uint8_t *)page, FLASH_PAGE_SIZE);
    restore_interrupts(s);
    printf("Settings saved: dir=%s drive=%s mode=%s\n", dir > 0 ? "fwd" : "rev", sine_on ? "SINE" : "6STEP", MODE_NAME[mode]);
}

// ---------------------------------------------------------------------------
// PWM drive (complementary, one PWM slice per phase: channel A = HIN, channel B = LIN inverted)
static uint32_t level_for(uint32_t pm) { return (wrap_val * pm) / 1000; }

static void set_pair(int p, uint32_t a, uint32_t b) {
    uint slice = pwm_gpio_to_slice_num(HIN(p));
    pwm_set_chan_level(slice, PWM_CHAN_A, a);
    pwm_set_chan_level(slice, PWM_CHAN_B, b);
}
static void phase_off(int p)  { set_pair(p, 0, wrap_val + 1); }   // HIN low, LIN low
static void phase_low(int p)  { set_pair(p, 0, 0); }             // HIN low, LIN high all the time
static void phase_high(int p) {                                  // HIN pulses, LIN fills the gaps
    uint32_t a = level_for(duty_pm);
    set_pair(p, a, a + dead_ticks);
}
static void all_off() {
    for (int p = 0; p < 3; p++) phase_off(p);
}
// v7.8: no all-off gap between steps. In 6-step order a phase never goes straight from high-side to low-side:
// it always passes through "floating", so no leg can shoot through. The old 100 us gap cut the torque
// at every step (about 10 % of the time at 1000 us) and was a likely source of the buzzing noise.
static void apply_step(int i) {
    int fl = 3 - STEPS[i][0] - STEPS[i][1];
    phase_off(fl);
    phase_low(STEPS[i][1]);
    phase_high(STEPS[i][0]);
}

// ---------------------------------------------------------------------------
// Motor engine: runs from timer interrupts
static int64_t step_cb(alarm_id_t, void *) {
    if (!want_run) {
        all_off();
        running = false;
        return 0;
    }
    idx = (idx + dir + 6) % 6;
    apply_step(idx);
    // constant acceleration of the step RATE (steps/s) toward the target rate
    uint32_t p = period_us, t = target_us;
    if (p != t) {
        float f = 1000000.0f / (float)p, tf = 1000000.0f / (float)t;
        float df = ((tf > f) ? ACCEL_SPS2 : DECEL_SPS2) * (float)p * 1e-6f;
        if (tf > f) { f += df; if (f > tf) f = tf; }
        else        { f -= df; if (f < tf) f = tf; }
        p = (uint32_t)(1000000.0f / f + 0.5f);
        if (p == period_us) p = (tf > (1000000.0f / (float)period_us)) ? p - 1 : p + 1;   // always make progress
        period_us = p;
    }
    return -(int64_t)p;   // next step p microseconds after this one was due
}

// v7.11: duty / amplitude follows the ACTUAL speed (volts per hertz) in both drive types. The throttle only sets the
// target speed. When the throttle drops, the speed falls slowly and the voltage falls with it, so the motor keeps enough
// torque to stay in step (v7.10 cut the voltage straight to the new throttle value and the motor could lose step).
static uint32_t duty_goal() {
    if (mode == M_KEYS) return duty_cmd;
    if (sine_on && sine_hold > 0) return SINE_ALIGN_PM;
    uint32_t p = period_us;
    uint32_t slow = THR_MAP_SLOW_US;
    uint32_t fast = sine_on ? SINE_FAST_US : THR_MAP_FAST_US;
    uint32_t lo = sine_on ? SINE_M_LO_PM : THR_DUTY_LO_PM;
    uint32_t hi = sine_on ? SINE_M_HI_PM : THR_DUTY_HI_PM;
    uint32_t span = slow - fast;
    uint32_t q = (p >= slow) ? 0 : (p <= fast ? span : slow - p);
    uint32_t v = lo + (hi - lo) * q / span;
    if (p > target_us) v += ACCEL_BOOST_PM;       // speeding up: a little extra torque
    if (v > MAX_DUTY_PM) v = MAX_DUTY_PM;
    return v;
}

static bool slew_cb(repeating_timer_t *) {
    if (running) {
        uint32_t g = duty_goal();
        if (duty_pm != g) {
            duty_pm += (duty_pm < g) ? 1 : -1;
            if (!sine_on) phase_high(STEPS[idx][0]);
        }
    }
    return true;
}

// Sine drive: three duty cycles 120 degrees apart, complementary PWM with dead time.
// Amplitude = duty_pm (per mille of full scale), electrical frequency follows target_us (1 / (6 * step time)).
static bool sine_cb(repeating_timer_t *) {
    if (!want_run) return false;
    bool holding = sine_hold > 0;               // v7.14: field stands still while the rotor aligns
    if (holding) { sine_hold--; if (sine_hold == 0) duty_pm = SINE_START_PM; }   // alignment done: starting kick
    float f_t = 1000000.0f / (6.0f * (float)target_us);
    float stp = holding ? 0.0f : SINE_ACCEL_HZ_PER_S * (SINE_UPDATE_US * 1e-6f);
    if (f_cur < f_t) { f_cur += stp; if (f_cur > f_t) f_cur = f_t; }
    else if (f_cur > f_t) { f_cur -= stp; if (f_cur < f_t) f_cur = f_t; }
    if (f_cur < 1.0f) f_cur = 1.0f;
    period_us = (uint32_t)(1000000.0f / (6.0f * f_cur));   // equivalent 6-step time, for status and soft stop
    uint32_t inc = (uint32_t)(f_cur * (4294967296.0f * SINE_UPDATE_US * 1e-6f));
    if (!holding) { if (dir > 0) theta += inc; else theta -= inc; }   // r key reverses the rotating field
    int32_t m = (int32_t)duty_pm;
    for (int p = 0; p < 3; p++) {
        uint32_t ph = theta + (uint32_t)p * 0x55555555u;
        int32_t sv = sine_tab[ph >> 22];
        int64_t off = (int64_t)(wrap_val / 2) * m * sv / (1000LL * 32768LL);
        int32_t a = (int32_t)(wrap_val / 2) + (int32_t)off;
        if (a < 0) a = 0;
        set_pair(p, (uint32_t)a, (uint32_t)a + dead_ticks);
    }
    return true;
}

static void start_motor() {
    if (running) return;
    if (sine_on) {
        f_cur = SINE_START_HZ;
        period_us = (uint32_t)(1000000.0f / (6.0f * f_cur));
        if (rotor_parked) {                          // rotor sits where the field stopped: start at once, no jolt
            sine_hold = 0;
            duty_pm = SINE_START_PM;
        } else {                                     // position unknown (first start, x, failsafe): align first
            sine_hold = HOLD_FIRST_US / SINE_UPDATE_US;
            duty_pm = 0;                             // amplitude ramps up gently (slew)
        }
        rotor_parked = false;
        want_run = true;
        running = true;
        add_repeating_timer_us(-SINE_UPDATE_US, sine_cb, NULL, &sine_timer);
        return;
    }
    period_us = START_STEP_US;
    duty_pm = (duty_cmd < START_BOOST_PM) ? START_BOOST_PM : duty_cmd;   // kick, then slew down to duty_cmd
    apply_step(idx);
    want_run = true;
    running = true;
    step_alarm = add_alarm_in_us(HOLD_FIRST_US, step_cb, NULL, true);
}

static void stop_motor() {
    uint32_t s = save_and_disable_interrupts();
    want_run = false;
    cancel_alarm(step_alarm);
    cancel_repeating_timer(&sine_timer);
    all_off();
    running = false;
    rotor_parked = false;      // any other way of stopping: position of the rotor is not known
    restore_interrupts(s);
}

// ---------------------------------------------------------------------------
// Throttle input on IN_PIN
// DShot receiver in PIO, hand-assembled so no pioasm step is needed:
//   wait until the line has been low ~23 us (gap between frames), then for each of 16 bits:
//   wait for rising edge, wait 1.9 us, sample the pin (1 = long pulse), wait for falling edge.
//   Sample is autopushed as one 16-bit word per frame.
static const uint16_t dshot_instr[11] = {
    0xE03F,  // 0  set x, 31
    0x00C0,  // 1  jmp pin, 0            (line high -> restart the gap count)
    0x0F41,  // 2  jmp x--, 1   [15]
    0xE04F,  // 3  set y, 15
    0x20A0,  // 4  wait 1 pin 0
    0xBF42,  // 5  nop [31]
    0xAC42,  // 6  nop [12]
    0x4001,  // 7  in pins, 1
    0x2020,  // 8  wait 0 pin 0
    0x0084,  // 9  jmp y--, 4
    0x0000,  // 10 jmp 0
};
static const pio_program_t dshot_prog = {dshot_instr, 11, -1};
static PIO dsh_pio = pio0;
static int dsh_sm = -1;
static uint dsh_off = 0;
static pio_sm_config dsh_cfg;

static volatile uint32_t ds_thr = 0;
static volatile uint32_t ds_last_ms = 0;
static volatile bool ds_seen = false;
static volatile uint32_t ds_ok = 0, ds_bad = 0;

static void dshot_setup() {
    dsh_off = pio_add_program(dsh_pio, &dshot_prog);
    dsh_sm = pio_claim_unused_sm(dsh_pio, true);
    dsh_cfg = pio_get_default_sm_config();
    sm_config_set_wrap(&dsh_cfg, dsh_off, dsh_off + 10);
    sm_config_set_in_pins(&dsh_cfg, IN_PIN);
    sm_config_set_jmp_pin(&dsh_cfg, IN_PIN);
    sm_config_set_in_shift(&dsh_cfg, false, true, 16);   // shift left, autopush at 16 bits
    sm_config_set_fifo_join(&dsh_cfg, PIO_FIFO_JOIN_RX);
    // 24 MHz state machine clock at DShot300 (one bit = 80 cycles); scales with the rate
    float hz = 24000000.0f * (float)DSHOT_RATE / 300.0f;
    sm_config_set_clkdiv(&dsh_cfg, (float)clock_get_hz(clk_sys) / hz);
}

static void dshot_start() {
    pio_gpio_init(dsh_pio, IN_PIN);
    pio_sm_set_consecutive_pindirs(dsh_pio, dsh_sm, IN_PIN, 1, false);
    gpio_pull_down(IN_PIN);
    pio_sm_init(dsh_pio, dsh_sm, dsh_off, &dsh_cfg);
    pio_sm_set_enabled(dsh_pio, dsh_sm, true);
    ds_seen = false;
}

static void dshot_poll() {
    while (!pio_sm_is_rx_fifo_empty(dsh_pio, dsh_sm)) {
        uint32_t w = pio_sm_get(dsh_pio, dsh_sm) & 0xFFFF;
        uint32_t v = w >> 4;                                   // 11 bit throttle + telemetry bit
        uint32_t crc = (v ^ (v >> 4) ^ (v >> 8)) & 0xF;
        if (crc == (w & 0xF)) {
            ds_thr = v >> 1;
            ds_last_ms = to_ms_since_boot(get_absolute_time());
            ds_seen = true;
            ds_ok++;
        } else {
            ds_bad++;
        }
    }
}

// 1-2 ms PWM input: pulse width measured with GPIO edge interrupts
static volatile uint32_t pw_rise = 0, pw_width = 0, pw_stamp = 0;
static volatile bool pw_seen = false;
static volatile uint32_t pw_ok = 0, pw_bad = 0;

static void gpio_cb(uint gpio, uint32_t events) {
    if (gpio != IN_PIN) return;
    uint32_t now = time_us_32();
    if (events & GPIO_IRQ_EDGE_RISE) pw_rise = now;
    if (events & GPIO_IRQ_EDGE_FALL) {
        uint32_t w = now - pw_rise;
        if (w >= 800 && w <= 2200) {
            pw_width = w;
            pw_stamp = now;
            pw_seen = true;
            pw_ok++;
        } else {
            pw_bad++;
        }
    }
}

static void set_mode(Mode m) {
    stopping = false;
    stop_motor();
    armed = false;
    arm_timing = false;
    pio_sm_set_enabled(dsh_pio, dsh_sm, false);
    gpio_set_irq_enabled(IN_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
    gpio_init(IN_PIN);
    gpio_set_dir(IN_PIN, GPIO_IN);
    gpio_pull_down(IN_PIN);
    if (m == M_DSHOT) {
        dshot_start();
    } else if (m == M_PWM) {
        pw_seen = false;
        gpio_set_irq_enabled_with_callback(IN_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_cb);
    }
    mode = m;
}

// throttle (per mille 0..1000) -> step time and duty
static void apply_map(uint32_t u) {
    uint32_t fast = sine_on ? SINE_FAST_US : THR_MAP_FAST_US;
    target_us = THR_MAP_SLOW_US - (THR_MAP_SLOW_US - fast) * u / 1000;
    uint32_t d = sine_on ? SINE_M_LO_PM + (SINE_M_HI_PM - SINE_M_LO_PM) * u / 1000
                         : THR_DUTY_LO_PM + (THR_DUTY_HI_PM - THR_DUTY_LO_PM) * u / 1000;
    if (d > MAX_DUTY_PM) d = MAX_DUTY_PM;
    duty_cmd = d;
}

static void control_update() {
    if (mode == M_KEYS) return;
    bool fresh;
    bool at_zero;
    uint32_t u;
    uint32_t now_ms;
    if (mode == M_DSHOT) {
        dshot_poll();                                   // read frames first, then take the time
        now_ms = to_ms_since_boot(get_absolute_time());
        fresh = ds_seen && (int32_t)(now_ms - ds_last_ms) < (int32_t)SIGNAL_TIMEOUT_MS;
        uint32_t v = ds_thr;
        at_zero = v < 48;
        u = v < 48 ? 0 : (v - 48) * 1000 / (2047 - 48);
        // direction commands: 7 = normal, 8 = reversed, must repeat
        if (fresh && !running && (v == 7 || v == 8)) {
            if (v == ds_cmd_last) ds_cmd_count++; else { ds_cmd_last = v; ds_cmd_count = 1; }
            if (ds_cmd_count == 6) {   // Betaflight repeats the command, 6 identical frames in a row = accept
                dir = (v == 7) ? 1 : -1;
                printf("DShot command %lu: direction %s\n", (unsigned long)v, dir > 0 ? "normal" : "reversed");
                settings_save();
            }
        } else {
            ds_cmd_count = 0; ds_cmd_last = 0;
        }
    } else {
        now_ms = to_ms_since_boot(get_absolute_time());
        int32_t age = (int32_t)(time_us_32() - pw_stamp);
        fresh = pw_seen && age < (int32_t)(SIGNAL_TIMEOUT_MS * 1000);
        uint32_t w = pw_width;
        at_zero = w <= 1050;
        u = w <= 1000 ? 0 : (w >= 2000 ? 1000 : w - 1000);
    }

    if (!fresh) {
        stopping = false;
        if (running) { stop_motor(); printf("SIGNAL LOST -> stop\n"); }
        if (armed) { armed = false; printf("DISARMED\n"); }
        arm_timing = false;
        return;
    }
    if (!armed) {
        if (at_zero) {
            if (!arm_timing) { arm_timing = true; arm_since_ms = now_ms; }
            else if (now_ms - arm_since_ms >= ARM_MS) { armed = true; printf("ARMED\n"); }
        } else {
            arm_timing = false;
        }
        return;
    }
    if (!running) {
        if (u >= START_U_PM) { apply_map(u); start_motor(); }
    } else {
        if (u < STOP_U_PM) {
            // soft stop: first slow down to the lowest running step rate and duty, then cut
            if (sine_on) {
                // park: slow the field right down and hold it still for a moment, so the rotor ends up where the
                // field is; the next start then needs no alignment and no jolt
                if (!stopping) { stopping = true; park_ms = 0; apply_map(0); target_us = SINE_PARK_US; }
                else if (period_us + 500 >= SINE_PARK_US) {
                    if (park_ms == 0) park_ms = now_ms | 1;
                    else if ((int32_t)(now_ms - park_ms) >= (int32_t)SINE_PARK_HOLD_MS) {
                        stop_motor(); rotor_parked = true; stopping = false; park_ms = 0;
                    }
                }
            } else {
                if (!stopping) { stopping = true; apply_map(0); }
                else if (period_us + 100 >= THR_MAP_SLOW_US) { stop_motor(); stopping = false; }
            }
        } else {
            stopping = false;
            park_ms = 0;
            apply_map(u);
        }
    }
}

// ---------------------------------------------------------------------------
static uint32_t raw_to_mv(uint32_t raw);
#define SCOPE_N 480
static uint16_t scope_buf[SCOPE_N];

static void scope_capture() {
    if (!running || sine_on) { printf("SCOPE: run in 6STEP first (running and drive=6STEP)\n"); return; }
    int i0 = idx;
    absolute_time_t to = make_timeout_time_ms(100);
    while (idx == i0 && !time_reached(to)) tight_loop_contents();
    if (idx == i0) { printf("SCOPE: no step change seen\n"); return; }
    int st = idx;                                     // the new step just started
    int ch = 3 - STEPS[st][0] - STEPS[st][1];         // floating phase of this step
    adc_select_input(ch);
    adc_fifo_setup(true, false, 0, false, false);
    adc_fifo_drain();
    adc_run(true);
    for (int k = 0; k < SCOPE_N; k++) scope_buf[k] = adc_fifo_get_blocking();
    adc_run(false);
    adc_fifo_drain();
    printf("SCOPE step=%d floating=%c n=%d dt=2us duty=%lu per-mille period=%luus  (mV at the motor phase)\n",
           st, 'A' + ch, SCOPE_N, (unsigned long)duty_pm, (unsigned long)period_us);
    for (int k = 0; k < SCOPE_N; k++) {
        printf("%lu%c", (unsigned long)raw_to_mv(scope_buf[k]), (k % 24 == 23) ? '\n' : ' ');
    }
    printf("SCOPE end\n");
}

static uint32_t raw_to_mv(uint32_t raw) {
    return (uint32_t)((uint64_t)raw * 3300u * (uint32_t)(DIV_TOP_OHM + DIV_BOT_OHM) / 4095u / (uint32_t)DIV_BOT_OHM);
}

static void show() {
    uint32_t dp = duty_pm, dc = duty_cmd;
    printf("%s mode=%s drive=%s armed=%d step=%d dir=%s duty=%u.%u%% (cmd %u.%u%%) period=%luus target=%luus",
           running ? "RUN " : "STOP", MODE_NAME[mode], sine_on ? "SINE" : "6STEP", armed ? 1 : 0, (int)idx, dir > 0 ? "fwd" : "rev",
           (unsigned)(dp / 10), (unsigned)(dp % 10), (unsigned)(dc / 10), (unsigned)(dc % 10),
           (unsigned long)period_us, (unsigned long)target_us);
    if (mode == M_DSHOT)
        printf("  dshot thr=%lu ok=%lu bad=%lu", (unsigned long)ds_thr, (unsigned long)ds_ok, (unsigned long)ds_bad);
    if (mode == M_PWM)
        printf("  pwm width=%luus ok=%lu bad=%lu", (unsigned long)pw_width, (unsigned long)pw_ok, (unsigned long)pw_bad);
    printf("\n");
}

int main() {
    stdio_init_all();

    uint32_t clk = clock_get_hz(clk_sys);
    wrap_val = clk / (2 * PWM_FREQ_HZ) - 1;           // phase-correct counts up and down
    dead_ticks = (clk / 1000000) * DEAD_NS / 1000;
    for (int p = 0; p < 3; p++) {
        gpio_set_function(HIN(p), GPIO_FUNC_PWM);
        gpio_set_function(LIN(p), GPIO_FUNC_PWM);
        uint slice = pwm_gpio_to_slice_num(HIN(p));
        pwm_set_wrap(slice, wrap_val);
        pwm_set_phase_correct(slice, true);
        pwm_set_output_polarity(slice, false, true);  // A normal, B inverted
        phase_off(p);
        pwm_set_enabled(slice, true);
    }
    adc_init();
    adc_gpio_init(26);  // A
    adc_gpio_init(27);  // B
    adc_gpio_init(28);  // C

    for (int i = 0; i < SINE_TABLE_N; i++)
        sine_tab[i] = (int16_t)lrintf(32767.0f * sinf(6.28318530718f * (float)i / (float)SINE_TABLE_N));

    dshot_setup();
    settings_load();
    set_mode((Mode)boot_mode);
    add_repeating_timer_us(-SLEW_US, slew_cb, NULL, &slew_timer);

    sleep_ms(2000);
    printf("\nPiESC-FW test v7.16. m=input mode (KEYS/DSHOT/PWM) x=STOP ?=status\n");
    show();

    absolute_time_t next_print = make_timeout_time_ms(1000);

    while (true) {
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) {
            switch (c) {
                case 'm':
                    set_mode((Mode)((mode + 1) % 3));
                    settings_save();
                    break;
                case 'x':
                    stopping = false;
                    stop_motor();
                    armed = false;
                    arm_timing = false;
                    break;
                case 'r':
                    if (!running) { dir = -dir; settings_save(); }
                    break;
                case 's':
                    if (mode == M_KEYS) start_motor();
                    break;
                case 'd':
                    if (!running) { sine_on = !sine_on; settings_save(); }
                    break;
                case '+':
                    if (mode == M_KEYS) {
                        if (duty_cmd + 10 <= MAX_DUTY_PM) duty_cmd += 10;
                        if (!running) duty_pm = duty_cmd;
                    }
                    break;
                case '-':
                    if (mode == M_KEYS) {
                        if (duty_cmd >= 10) duty_cmd -= 10;
                        if (!running) duty_pm = duty_cmd;
                    }
                    break;
                case 'f':
                    if (mode == M_KEYS && target_us >= MIN_TARGET_US + 250) target_us -= 250;
                    break;
                case 'g':
                    if (mode == M_KEYS && target_us + 250 <= MAX_TARGET_US) target_us += 250;
                    break;
                case 'n':
                    if (mode == M_KEYS && !running) {
                        idx = (idx + dir + 6) % 6;
                        apply_step(idx);
                        sleep_ms(PULSE_MS);
                        all_off();
                        printf("PULSE step=%d\n", (int)idx);
                    }
                    break;
                case 'h':
                    if (mode == M_KEYS && !running) {
                        idx = (idx + dir + 6) % 6;
                        apply_step(idx);
                        printf("HOLD step=%d  high=%c low=%c  (press x to release)\n", (int)idx,
                               'A' + STEPS[idx][0], 'A' + STEPS[idx][1]);
                    }
                    break;
                case 'o':
                    scope_capture();
                    break;
                case 'a':
                    if (!running) {
                        all_off();
                        uint32_t mv[3], rw[3];
                        for (int ch = 0; ch < 3; ch++) {
                            adc_select_input(ch);
                            uint32_t sum = 0;
                            for (int k = 0; k < 64; k++) sum += adc_read();
                            rw[ch] = sum / 64;
                            mv[ch] = raw_to_mv(rw[ch]);
                        }
                        printf("IDLE (all off) mV: A=%lu B=%lu C=%lu  raw %lu %lu %lu\n",
                               (unsigned long)mv[0], (unsigned long)mv[1], (unsigned long)mv[2],
                               (unsigned long)rw[0], (unsigned long)rw[1], (unsigned long)rw[2]);
                    }
                    break;
                default:
                    break;
            }
            show();
        }

        control_update();

        if (time_reached(next_print)) {
            if (running || mode != M_KEYS) show();
            next_print = make_timeout_time_ms(1000);
        }
    }
}
