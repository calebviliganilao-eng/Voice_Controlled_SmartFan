#ifndef SMARTFAN_LOGIC_H
#define SMARTFAN_LOGIC_H
#include <stdbool.h>
#include <stdint.h>

/* Module A: IN1 = bit0 (GPIO32), IN2 = bit1 (GPIO33).
   Module B: IN1 = bit2 (GPIO25); Module B IN2 is not used. */
#define FAN_RELAY_A1 (1u << 0)
#define FAN_RELAY_A2 (1u << 1)
#define FAN_RELAY_B1 (1u << 2)

static inline int fan_normalize(int value) {
    if (value <= 0) return 0;
    if (value <= 33) return 33;
    if (value <= 66) return 66;
    return 100;
}

/* Sinric Pro's Fan dashboard / Alexa can send a discrete level 1, 2, 3,
   while some integrations send a 0..100 percentage. The old code treated
   all of 1, 2 and 3 as 33%, so the relay never changed from LOW. */
static inline int fan_from_sinric_request(int requested) {
    if (requested == 1) return 33;
    if (requested == 2) return 66;
    if (requested == 3) return 100;
    return fan_normalize(requested);
}

/* Convert the internal 0 / 33 / 66 / 100% preset into the 0..3 steps
   shown by the Sinric Pro fan speed selector. */
static inline int fan_step_from_percent(int level) {
    int normalized = fan_normalize(level);
    if (normalized == 0) return 0;
    if (normalized == 33) return 1;
    if (normalized == 66) return 2;
    return 3;
}

/* Relative adjustments can arrive as steps (+1/-1) or percentages
   (+10/-10). A negative adjustment must lower the selected step even
   though rounding the resulting percentage would keep it unchanged. */
static inline int fan_adjust_from_sinric(int old_level, int delta) {
    int step = fan_step_from_percent(old_level);
    if (delta == 0) return fan_normalize(old_level);
    long long mag = delta < 0 ? -(long long)delta : (long long)delta;
    int count = 1;
    if (mag <= 3) count = (int)mag;
    else if (mag > 66) count = 3;
    else if (mag > 33) count = 2;
    int result = step + (delta > 0 ? count : -count);
    if (result < 0) result = 0;
    if (result > 3) result = 3;
    return result == 0 ? 0 : (result == 1 ? 33 : (result == 2 ? 66 : 100));
}

static inline int fan_adjust(int old_level, int delta) {
    /* long prevents integer overflow for 32-bit int adjustments. */
    long long value = (long long)old_level + (long long)delta;
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    return fan_normalize((int)value);
}

/* All 3 relays are for a 3-speed-selector LAB DEMONSTRATION.
   With ONE two-wire DC fan, only Module A IN1 switches fan ON/OFF.
   Such a fan does NOT gain three physical speeds from 3 relays. */
static inline unsigned fan_relay_mask(int requested_level, bool two_wire_dc_fan) {
    int value = fan_normalize(requested_level);
    if (value == 0) return 0u;
    if (two_wire_dc_fan) return FAN_RELAY_A1;
    if (value == 33) return FAN_RELAY_A1;
    if (value == 66) return FAN_RELAY_A2;
    return FAN_RELAY_B1;
}

static inline bool fan_mask_is_safe(unsigned mask) {
    return (mask == 0u || mask == FAN_RELAY_A1 ||
            mask == FAN_RELAY_A2 || mask == FAN_RELAY_B1);
}
#endif
