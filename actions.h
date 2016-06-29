#ifndef ACTIONS_H
#define ACTIONS_H

#include "switch.h"
#include "port.h"
#include <rte_mbuf.h>

static void inline action_pop_vlan(struct rte_mbuf* packet);
static void inline action_push_vlan(struct rte_mbuf* packet, int tag);
static void inline action_drop(struct rte_mbuf* packet);
static void inline action_output(struct rte_mbuf* packet, Port* p);
static void inline action_flood(struct rte_mbuf* packet, Switch* s, Port* port);

#endif /* ACTIONS_H */
