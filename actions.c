#include "actions.h"
#include "switch.h"
#include "port.h"

#include <rte_mbuf.h>

/* Strip VLAN tag from packet */
void action_pop_vlan(struct rte_mbuf* packet) {

}

/* Add VLAN tag to packet */
void action_push_vlan(struct rte_mbuf* packet) {

}

/* Drop packet */
void action_drop(struct rte_mbuf* packet) {

}

/* Enqueue packet to TX queue of given port */
void action_output(struct rte_mbuf* packet, Port* p) {

}

/* Enqueue packet to all ports except in_port */
void action_flood(struct rte_mbuf* packet, Switch* s, Port* in_port) {

}

