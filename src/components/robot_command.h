#ifndef ROBOT_COMMAND_H
#define ROBOT_COMMAND_H
#include "components/robot_wire.h"
/* One endpoint per transport; shared physical servo state, separate replies/IDs. */
typedef struct {
    uint8_t reply[RW_SIZE], last_request[RW_SIZE], last_reply[RW_SIZE];
    int have_last;
} RobotEndpoint;
void robot_endpoint_init(RobotEndpoint *endpoint);
void robot_command(RobotEndpoint *endpoint, const uint8_t *request);
#endif
