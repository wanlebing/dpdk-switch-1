#include "control.h"

void control_process_message(ControlMessage* msg) {
    switch (msg->code) {
    case SET_VLAN:
        break;
    case UNSET_VLAN:
        break;
    case SET_TRUNK:
        break;
    case UNSET_TRUNK:
        break;
    default:
        break;
    }
}
