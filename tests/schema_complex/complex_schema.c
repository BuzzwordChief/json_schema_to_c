#include "complex_schema.parser.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>


#define BUF_SIZE 4096

static const uint64_t TS_LARGE = 1710000000000000000ULL;


static json_write_state_t make_write_state(char *buf, size_t capacity)
{
    json_write_state_t state = {
        .buf = buf,
        .capacity = capacity,
        .pos = 0,
    };
    return state;
}


static void test_cmd_identify(void)
{
    const char *json = "{\"type\":\"cmd.identify\",\"ts\":\"1710000000000000000\"}";
    ahrs_t parsed = {};
    ahrs_t again = {};
    char buf[BUF_SIZE];
    json_write_state_t state;

    assert(!json_parse_ahrs(json, &parsed));
    assert(parsed.variant == AHRS_CMD_IDENTIFY);
    assert(parsed.u.cmd_identify.type == AHRS_CMD_IDENTIFY_TYPE_CMD_IDENTIFY);
    assert(parsed.u.cmd_identify.ts == TS_LARGE);

    state = make_write_state(buf, sizeof(buf));
    assert(!json_write_ahrs_cmd_identify(&state, &parsed.u.cmd_identify));
    assert(!strcmp(buf, json));

    assert(!json_parse_ahrs(buf, &again));
    assert(again.variant == AHRS_CMD_IDENTIFY);
    assert(again.u.cmd_identify.ts == TS_LARGE);
}


static void test_identify_response(void)
{
    const char *json =
        "{\"type\":\"identify\",\"serial\":\"SN-42\",\"hw_version\":\"1.0\","
        "\"project\":\"AHRS-01\",\"fw_version\":\"2.3.1\",\"ts\":\"1000\"}";
    ahrs_t parsed = {};
    ahrs_t again = {};
    char buf[BUF_SIZE];

    assert(!json_parse_ahrs(json, &parsed));
    assert(parsed.variant == AHRS_IDENTIFY);
    assert(!strcmp(parsed.u.identify.serial, "SN-42"));
    assert(!strcmp(parsed.u.identify.hw_version, "1.0"));
    assert(!strcmp(parsed.u.identify.project, "AHRS-01"));
    assert(!strcmp(parsed.u.identify.fw_version, "2.3.1"));
    assert(parsed.u.identify.ts == 1000);

    json_write_state_t state = make_write_state(buf, sizeof(buf));
    assert(!json_write_ahrs_identify(&state, &parsed.u.identify));
    assert(!json_parse_ahrs(buf, &again));
    assert(again.variant == AHRS_IDENTIFY);
    assert(!strcmp(again.u.identify.serial, "SN-42"));
    assert(!strcmp(again.u.identify.fw_version, "2.3.1"));
}


static void test_error_message(void)
{
    const char *json =
        "{\"type\":\"error\",\"code\":7,\"message\":\"rejected\","
        "\"context\":\"cmd.reset\",\"ts\":\"2000\"}";
    ahrs_t parsed = {};
    char buf[BUF_SIZE];

    assert(!json_parse_ahrs(json, &parsed));
    assert(parsed.variant == AHRS_ERROR);
    assert(parsed.u.error.code == 7);
    assert(!strcmp(parsed.u.error.message, "rejected"));
    assert(!strcmp(parsed.u.error.context, "cmd.reset"));

    json_write_state_t state = make_write_state(buf, sizeof(buf));
    assert(!json_write_ahrs_error(&state, &parsed.u.error));
    assert(!json_parse_ahrs(buf, &parsed));
    assert(parsed.u.error.code == 7);
}


static void test_status_required_fields(void)
{
    const char *json =
        "{\"type\":\"status\",\"state\":\"READY\",\"ptp_state\":\"LOCKED\",\"ts\":\"3000\"}";
    ahrs_t parsed = {};

    assert(!json_parse_ahrs(json, &parsed));
    assert(parsed.variant == AHRS_STATUS);
    assert(parsed.u.status.state == AHRS_STATUS_STATE_READY);
    assert(parsed.u.status.ptp_state == AHRS_STATUS_PTP_STATE_LOCKED);
    assert(parsed.u.status.ts == 3000);
}


static void test_status_nullable_fields(void)
{
    const char *json_with_null =
        "{\"type\":\"status\",\"state\":\"NAVIGATING\",\"ptp_state\":\"HOLDOVER\","
        "\"launch_id\":null,\"t_launch\":null,\"ts\":\"4000\"}";
    const char *json_with_values =
        "{\"type\":\"status\",\"state\":\"READY\",\"ptp_state\":\"LOCKED\","
        "\"launch_id\":\"launch-7\",\"t_launch\":\"5000\",\"ts\":\"6000\"}";
    ahrs_t parsed = {};
    char buf[BUF_SIZE];

    assert(!json_parse_ahrs(json_with_null, &parsed));
    assert(parsed.variant == AHRS_STATUS);
    assert(parsed.u.status.launch_id.is_null);
    assert(parsed.u.status.t_launch.is_null);

    assert(!json_parse_ahrs(json_with_values, &parsed));
    assert(!parsed.u.status.launch_id.is_null);
    assert(!strcmp(parsed.u.status.launch_id.value, "launch-7"));
    assert(!parsed.u.status.t_launch.is_null);
    assert(parsed.u.status.t_launch.value == 5000);

    json_write_state_t state = make_write_state(buf, sizeof(buf));
    assert(!json_write_ahrs_status(&state, &parsed.u.status));
    assert(!json_parse_ahrs(buf, &parsed));
    assert(!parsed.u.status.launch_id.is_null);
    assert(parsed.u.status.t_launch.value == 5000);
}


static void test_cmd_cal_reference(void)
{
    const char *json =
        "{\"type\":\"cmd.cal.reference\",\"params\":{"
        "\"lat_deg\":52.5,\"lon_deg\":13.4,\"heeling_deg\":-1.5,\"inclination_deg\":0.25"
        "},\"ts\":\"7000\"}";
    ahrs_t parsed = {};
    char buf[BUF_SIZE];

    assert(!json_parse_ahrs(json, &parsed));
    assert(parsed.variant == AHRS_CMD_CAL_REFERENCE);
    assert(parsed.u.cmd_cal_reference.params.lat_deg == 52.5);
    assert(parsed.u.cmd_cal_reference.params.lon_deg == 13.4);
    assert(parsed.u.cmd_cal_reference.params.heeling_deg == -1.5);
    assert(parsed.u.cmd_cal_reference.params.inclination_deg == 0.25);

    json_write_state_t state = make_write_state(buf, sizeof(buf));
    assert(!json_write_ahrs_cmd_cal_reference(&state, &parsed.u.cmd_cal_reference));
    assert(!json_parse_ahrs(buf, &parsed));
    assert(parsed.u.cmd_cal_reference.params.lat_deg == 52.5);
}


static void test_cmd_test_start_and_reset(void)
{
    const char *start_json =
        "{\"type\":\"cmd.test.start\",\"params\":{\"test_id\":3},\"ts\":\"8000\"}";
    const char *reset_json =
        "{\"type\":\"cmd.reset\",\"params\":{\"confirm\":\"RESET\"},\"ts\":\"9000\"}";
    ahrs_t parsed = {};

    assert(!json_parse_ahrs(start_json, &parsed));
    assert(parsed.variant == AHRS_CMD_TEST_START);
    assert(parsed.u.cmd_test_start.params.test_id == 3);

    assert(!json_parse_ahrs(reset_json, &parsed));
    assert(parsed.variant == AHRS_CMD_RESET);
    assert(parsed.u.cmd_reset.params.confirm == AHRS_CMD_RESET_PARAMS_CONFIRM_RESET);
}


static void test_aiding_water(void)
{
    const char *json =
        "{\"type\":\"aiding.water\",\"velocity_mps\":{\"x\":1.2,\"y\":0.0,\"z\":-0.1},"
        "\"valid\":{\"x\":true,\"y\":false,\"z\":false},"
        "\"t_meas\":\"12345\",\"ts\":\"10000\"}";
    ahrs_t parsed = {};
    char buf[BUF_SIZE];

    assert(!json_parse_ahrs(json, &parsed));
    assert(parsed.variant == AHRS_AIDING_WATER);
    assert(parsed.u.aiding_water.velocity_mps.x == 1.2);
    assert(parsed.u.aiding_water.velocity_mps.z == -0.1);
    assert(parsed.u.aiding_water.valid.x == true);
    assert(parsed.u.aiding_water.valid.y == false);
    assert(parsed.u.aiding_water.t_meas == 12345);

    json_write_state_t state = make_write_state(buf, sizeof(buf));
    assert(!json_write_ahrs_aiding_water(&state, &parsed.u.aiding_water));
    assert(!json_parse_ahrs(buf, &parsed));
    assert(parsed.u.aiding_water.velocity_mps.x == 1.2);
}


static void test_nav_data(void)
{
    const char *json =
        "{\"type\":\"nav.data\",\"seq\":42,\"state\":\"NAVIGATING\","
        "\"imu_counter\":100,\"t_since_launch_s\":12.5,"
        "\"course_deg\":90.0,\"heeling_deg\":1.0,\"inclination_deg\":2.0,"
        "\"heeling_rate_dps\":0.1,\"inclination_rate_dps\":0.2,\"course_rate_dps\":0.3,"
        "\"accel_mps2\":{\"x\":0.0,\"y\":0.0,\"z\":9.81},"
        "\"quat\":{\"w\":1.0,\"x\":0.0,\"y\":0.0,\"z\":0.0},"
        "\"status\":{\"solution_valid\":true,\"aligned\":true,\"ptp_locked\":true,"
        "\"aiding_fresh\":false,\"aiding_stale\":false,\"degraded\":false},"
        "\"ts\":\"11000\"}";
    ahrs_t parsed = {};
    char buf[BUF_SIZE];

    assert(!json_parse_ahrs(json, &parsed));
    assert(parsed.variant == AHRS_NAV_DATA);
    assert(parsed.u.nav_data.seq == 42);
    assert(parsed.u.nav_data.state == AHRS_NAV_DATA_STATE_NAVIGATING);
    assert(parsed.u.nav_data.imu_counter == 100);
    assert(parsed.u.nav_data.quat.w == 1.0);
    assert(parsed.u.nav_data.status.solution_valid == true);
    assert(parsed.u.nav_data.status.ptp_locked == true);

    json_write_state_t state = make_write_state(buf, sizeof(buf));
    assert(!json_write_ahrs_nav_data(&state, &parsed.u.nav_data));
    assert(!json_parse_ahrs(buf, &parsed));
    assert(parsed.u.nav_data.seq == 42);
    assert(parsed.u.nav_data.accel_mps2.z == 9.81);
}


static void test_test_report_with_values_array(void)
{
    const char *json =
        "{\"type\":\"test.report\",\"test_id\":2,\"status\":\"PASS\","
        "\"progress_pct\":100,\"values\":[1.5,2.5],\"message\":\"ok\",\"ts\":\"12000\"}";
    ahrs_t parsed = {};

    assert(!json_parse_ahrs(json, &parsed));
    assert(parsed.variant == AHRS_TEST_REPORT);
    assert(parsed.u.test_report.test_id == 2);
    assert(parsed.u.test_report.status == AHRS_TEST_REPORT_STATUS_PASS);
    assert(parsed.u.test_report.values.n == 2);
    assert(parsed.u.test_report.values.items[0] == 1.5);
    assert(parsed.u.test_report.values.items[1] == 2.5);
    assert(!strcmp(parsed.u.test_report.message, "ok"));
}


static void test_launch_event(void)
{
    const char *json =
        "{\"type\":\"launch.event\",\"launch_id\":\"L-1\",\"t_launch\":\"99000\","
        "\"imu_counter_at_launch\":50,\"source\":\"scheduled\",\"ts\":\"13000\"}";
    ahrs_t parsed = {};

    assert(!json_parse_ahrs(json, &parsed));
    assert(parsed.variant == AHRS_LAUNCH_EVENT);
    assert(!strcmp(parsed.u.launch_event.launch_id, "L-1"));
    assert(parsed.u.launch_event.t_launch == 99000);
    assert(parsed.u.launch_event.imu_counter_at_launch == 50);
    assert(parsed.u.launch_event.source == AHRS_LAUNCH_EVENT_SOURCE_SCHEDULED);
}


static void test_parse_rejects_invalid(void)
{
    ahrs_t parsed = {};

    assert(json_parse_ahrs("{\"type\":\"cmd.identify\"}", &parsed));
    assert(json_parse_ahrs("{\"type\":\"not.a.message\",\"ts\":\"1\"}", &parsed));
    assert(json_parse_ahrs("not json", &parsed));
    assert(json_parse_ahrs("{}", &parsed));
}


static void test_write_buffer_overflow(void)
{
    ahrs_identify_t identify = {
        .type = AHRS_IDENTIFY_TYPE_IDENTIFY,
        .ts = 1,
    };
    char buf[16];
    json_write_state_t state = make_write_state(buf, sizeof(buf));

    memcpy(identify.serial, "SN", 3);
    memcpy(identify.hw_version, "1", 2);
    memcpy(identify.project, "P", 2);
    memcpy(identify.fw_version, "1", 2);

    assert(json_write_ahrs_identify(&state, &identify));
}


int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    test_cmd_identify();
    test_identify_response();
    test_error_message();
    test_status_required_fields();
    test_status_nullable_fields();
    test_cmd_cal_reference();
    test_cmd_test_start_and_reset();
    test_aiding_water();
    test_nav_data();
    test_test_report_with_values_array();
    test_launch_event();
    test_parse_rejects_invalid();
    test_write_buffer_overflow();

    return 0;
}