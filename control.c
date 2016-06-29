#include "control.h"
#include "port.h"
#include "switch.h"

const char* control_process_message(Switch* s, ControlMessage* msg) {
    char* response = "OK";
    Port* p = NULL;

    switch (msg->code) {
        case SET_VLAN:
        p = switch_lookup_port(s, msg->port_name);
        if (p == NULL) response = "Port does not exist";
        else port_set_vlan_tag(p, msg->tag);
        break;
        case UNSET_VLAN:
        p = switch_lookup_port(s, msg->port_name);
        if (p == NULL) response = "Port does not exist";
        else port_unset_vlan_tag(p);
        break;
        case SET_TRUNK:
        p = switch_lookup_port(s, msg->port_name);
        if (p == NULL) response = "Port does not exist";
        else port_set_vlan_trunk(p, msg->tag);
        break;
        case UNSET_TRUNK:
        p = switch_lookup_port(s, msg->port_name);
        if (p == NULL) response = "Port does not exist";
        else port_unset_vlan_trunk(p, msg->tag);
        break;
        default:
        response = "Fail";
        break;
    }

    return response;
}
