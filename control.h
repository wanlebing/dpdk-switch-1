#ifndef CONTROL_H
#define CONTROL_H

#include <stdint.h>

/* vswitch control messsage codes */
#define SET_VLAN    0x01
#define UNSET_VLAN  0x02
#define SET_TRUNK   0x04
#define UNSET_TRUNK 0x08

typedef struct ControlMessage {
    uint8_t code;
    void* arg;
} ControlMessage;

#endif /* CONTROL_H */
