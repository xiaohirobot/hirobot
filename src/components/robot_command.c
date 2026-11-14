#include "components/robot_command.h"
#include "components/servo_angle.h"
static int servo_ready;
void robot_endpoint_init(RobotEndpoint *endpoint)
{
    memset(endpoint, 0, sizeof(*endpoint));
    rw_make(endpoint->reply, RW_REPLY, 0, 0, 0, 0);
    endpoint->reply[8] = RW_BUSY;
    rw_finish(endpoint->reply);
}
void robot_command(RobotEndpoint *endpoint, const uint8_t *request)
{
    if (!rw_valid(request) || request[3] != RW_SUBMIT) return;
    uint32_t id = rw_get(request + 4);
    uint8_t mode = request[9], status = RW_OK;
    if (endpoint->have_last && rw_get(endpoint->last_request + 4) == id) {
        if (!memcmp(endpoint->last_request, request, RW_SIZE)) {
            memcpy(endpoint->reply, endpoint->last_reply, RW_SIZE);
        } else {
            rw_make(endpoint->reply, RW_REPLY, id, mode, 0, 0);
            endpoint->reply[8] = RW_INVALID;
            rw_finish(endpoint->reply);
        }
        return;
    }
    int32_t a = (int32_t)rw_get(request + 10), b = (int32_t)rw_get(request + 14);
    int32_t x = 0, y = 0;
    if (mode == 1) {
        if ((b > 0 && a > INT32_MAX - b) || (b < 0 && a < INT32_MIN - b)) status = RW_INVALID;
        else x = a + b;
    } else if (mode == 10) {
        if (!servo_ready) servo_ready = servo_angle_init() == SERVO_OK ? 1 : -1;
        if (servo_ready < 0 || servo_angle_move(a, b) != SERVO_OK) {
            servo_stop_all(); status = RW_SERVO;
        } else {
            x = servo_angle_deg(0); y = servo_angle_deg(1);
        }
    } else status = RW_INVALID;
    rw_make(endpoint->reply, RW_REPLY, id, mode, x, y);
    endpoint->reply[8] = status;
    rw_finish(endpoint->reply);
    memcpy(endpoint->last_request, request, RW_SIZE);
    memcpy(endpoint->last_reply, endpoint->reply, RW_SIZE);
    endpoint->have_last = 1;
}
