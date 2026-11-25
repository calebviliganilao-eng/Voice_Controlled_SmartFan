#include "unity.h"
#include "../../src/fan_logic.h"

void setUp(void) {}
void tearDown(void) {}

void test_normalization_boundaries(void) {
    TEST_ASSERT_EQUAL_INT(0, fan_normalize(-10));
    TEST_ASSERT_EQUAL_INT(0, fan_normalize(0));
    TEST_ASSERT_EQUAL_INT(33, fan_normalize(1));
    TEST_ASSERT_EQUAL_INT(33, fan_normalize(33));
    TEST_ASSERT_EQUAL_INT(66, fan_normalize(34));
    TEST_ASSERT_EQUAL_INT(66, fan_normalize(50));
    TEST_ASSERT_EQUAL_INT(66, fan_normalize(66));
    TEST_ASSERT_EQUAL_INT(100, fan_normalize(67));
    TEST_ASSERT_EQUAL_INT(100, fan_normalize(100));
    TEST_ASSERT_EQUAL_INT(100, fan_normalize(150));
}
void test_adjust_relative(void) {
    TEST_ASSERT_EQUAL_INT(0, fan_adjust(33, -40));
    TEST_ASSERT_EQUAL_INT(33, fan_adjust(33, -10));
    TEST_ASSERT_EQUAL_INT(66, fan_adjust(33, 10));
    TEST_ASSERT_EQUAL_INT(100, fan_adjust(66, 35));
    TEST_ASSERT_EQUAL_INT(100, fan_adjust(100, 100));
    TEST_ASSERT_EQUAL_INT(100, fan_adjust(100, 2147483647));
    TEST_ASSERT_EQUAL_INT(0, fan_adjust(0, -2147483647));
}
void test_three_relay_outputs(void) {
    TEST_ASSERT_EQUAL_UINT(0u, fan_relay_mask(0, false));
    TEST_ASSERT_EQUAL_UINT(FAN_RELAY_A1, fan_relay_mask(33, false));
    TEST_ASSERT_EQUAL_UINT(FAN_RELAY_A2, fan_relay_mask(50, false));
    TEST_ASSERT_EQUAL_UINT(FAN_RELAY_A2, fan_relay_mask(66, false));
    TEST_ASSERT_EQUAL_UINT(FAN_RELAY_B1, fan_relay_mask(100, false));
    for (int level = 0; level <= 100; ++level) {
        TEST_ASSERT_TRUE(fan_mask_is_safe(fan_relay_mask(level, false)));
    }
}
void test_two_wire_dc_fan_on_off(void) {
    TEST_ASSERT_EQUAL_UINT(0u, fan_relay_mask(0, true));
    TEST_ASSERT_EQUAL_UINT(FAN_RELAY_A1, fan_relay_mask(33, true));
    TEST_ASSERT_EQUAL_UINT(FAN_RELAY_A1, fan_relay_mask(66, true));
    TEST_ASSERT_EQUAL_UINT(FAN_RELAY_A1, fan_relay_mask(100, true));
}
void test_sinric_alexa_levels_1_2_3(void) {
    TEST_ASSERT_EQUAL_INT(0, fan_from_sinric_request(0));
    TEST_ASSERT_EQUAL_INT(33, fan_from_sinric_request(1));
    TEST_ASSERT_EQUAL_INT(66, fan_from_sinric_request(2));
    TEST_ASSERT_EQUAL_INT(100, fan_from_sinric_request(3));
    TEST_ASSERT_EQUAL_INT(33, fan_from_sinric_request(33));
    TEST_ASSERT_EQUAL_INT(66, fan_from_sinric_request(66));
    TEST_ASSERT_EQUAL_INT(100, fan_from_sinric_request(100));
    TEST_ASSERT_EQUAL_INT(1, fan_step_from_percent(33));
    TEST_ASSERT_EQUAL_INT(2, fan_step_from_percent(66));
    TEST_ASSERT_EQUAL_INT(3, fan_step_from_percent(100));
    TEST_ASSERT_EQUAL_UINT(FAN_RELAY_A1, fan_relay_mask(fan_from_sinric_request(1), false));
    TEST_ASSERT_EQUAL_UINT(FAN_RELAY_A2, fan_relay_mask(fan_from_sinric_request(2), false));
    TEST_ASSERT_EQUAL_UINT(FAN_RELAY_B1, fan_relay_mask(fan_from_sinric_request(3), false));
}
void test_percent_and_range_commands_select_every_relay(void) {
    const int range_values[3] = {1, 2, 3};
    const int percentage_values[3] = {33, 66, 100};
    const unsigned expected[3] = {FAN_RELAY_A1, FAN_RELAY_A2, FAN_RELAY_B1};
    for (int i = 0; i < 3; ++i) {
        int range_as_percent = fan_from_sinric_request(range_values[i]);
        int pct_as_percent = fan_from_sinric_request(percentage_values[i]);
        TEST_ASSERT_EQUAL_INT(percentage_values[i], range_as_percent);
        TEST_ASSERT_EQUAL_INT(percentage_values[i], pct_as_percent);
        TEST_ASSERT_EQUAL_INT(i+1, fan_step_from_percent(range_as_percent));
        TEST_ASSERT_EQUAL_UINT(expected[i], fan_relay_mask(range_as_percent, false));
        TEST_ASSERT_EQUAL_UINT(expected[i], fan_relay_mask(pct_as_percent, false));
    }
}
void test_alexa_increase_decrease_speed(void) {
    TEST_ASSERT_EQUAL_INT(66, fan_adjust_from_sinric(33, 1));
    TEST_ASSERT_EQUAL_INT(33, fan_adjust_from_sinric(66, -1));
    TEST_ASSERT_EQUAL_INT(66, fan_adjust_from_sinric(100, -1));
    TEST_ASSERT_EQUAL_INT(100, fan_adjust_from_sinric(33, 2));
    TEST_ASSERT_EQUAL_INT(0, fan_adjust_from_sinric(33, -1));
    TEST_ASSERT_EQUAL_INT(66, fan_adjust_from_sinric(33, 10));
    TEST_ASSERT_EQUAL_INT(0, fan_adjust_from_sinric(33, -10));
    TEST_ASSERT_EQUAL_INT(100, fan_adjust_from_sinric(100, 10));
    TEST_ASSERT_EQUAL_INT(0, fan_adjust_from_sinric(0, -10));
    TEST_ASSERT_EQUAL_INT(100, fan_adjust_from_sinric(0, 100));
    TEST_ASSERT_EQUAL_INT(0, fan_adjust_from_sinric(100, -100));
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_normalization_boundaries);
    RUN_TEST(test_adjust_relative);
    RUN_TEST(test_three_relay_outputs);
    RUN_TEST(test_two_wire_dc_fan_on_off);
    RUN_TEST(test_sinric_alexa_levels_1_2_3);
    RUN_TEST(test_alexa_increase_decrease_speed);
    RUN_TEST(test_percent_and_range_commands_select_every_relay);
    return UNITY_END();
}
