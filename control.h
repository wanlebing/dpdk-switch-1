#ifndef CONTROL_H
#define CONTROL_H

#include <stdint.h>

typedef struct Switch Switch;

#define COMM_PORT 7733

/* vswitch control messsage codes */
#define SET_VLAN    0x01
#define UNSET_VLAN  0x02
#define SET_TRUNK   0x04
#define UNSET_TRUNK 0x08

typedef struct ControlMessage {
    uint8_t code;
    char port_name[16];
    int tag;
} ControlMessage;

const char* control_process_message(Switch* s, ControlMessage* msg);

#endif /* CONTROL_H */
