#ifndef ACTIONS_H
#define ACTIONS_H

#include "switch.h"
#include "port.h"
#include <rte_mbuf.h>

void action_pop_vlan(struct rte_mbuf* packet);
void action_push_vlan(struct rte_mbuf* packet);
void action_drop(struct rte_mbuf* packet);
void action_output(struct rte_mbuf* packet, Port* p);
void action_flood(struct rte_mbuf* packet, Switch* s);

#endif /* ACTIONS_H */
