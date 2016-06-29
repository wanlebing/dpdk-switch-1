#include "actions.h"
#include "switch.h"
#include "port.h"

#include <rte_mbuf.h>
#include <rte_ether.h>
#include <rte_ring.h>
#include <rte_memcpy.h>
#include <rte_malloc.h>
#include <rte_hexdump.h>

/* Strip VLAN tag from packet */
static void inline action_pop_vlan(struct rte_mbuf* packet) {
    rte_vlan_strip(packet);
}

/* Add VLAN tag to packet */
static void inline action_push_vlan(struct rte_mbuf* packet, int tag) {
    rte_vlan_insert(&packet);
    struct ether_hdr* eth = rte_pktmbuf_mtod(packet, struct ether_hdr*);
    struct vlan_hdr* vlan = (struct vlan_hdr*)(eth + 1);
    vlan->vlan_tci = rte_cpu_to_be_16(tag);
    vlan->vlan_tci |= rte_cpu_to_be_16(0x0000);
}

/* Drop packet */
static void inline action_drop(struct rte_mbuf* packet) {
    rte_pktmbuf_free(packet);
}

/* Enqueue packet to TX queue of given port */
static void inline action_output(struct rte_mbuf* packet, Port* p) {
    if (p->ring_tx != NULL && p->is_active) {
        rte_ring_sp_enqueue_bulk(p->ring_tx, &packet, 1);
    }
}

/* Enqueue packet to all ports except in_port */
static void inline action_flood(struct rte_mbuf* packet, Switch* s, Port* in_port) {
    for (Node* n = s->ports->head; n != NULL; n = n->next) {
        if ((Port*) n->value != in_port) {
            //          struct rte_mbuf* copy = rte_malloc("struct rte_mbuf", sizeof(struct rte_mbuf), 0);
            //          rte_memcpy(copy, packet, sizeof(struct rte_mbuf));
            action_output(packet, (Port*) n->value);
        } else {
            continue;
        }
        //      action_drop(packet);
    }
}

static void inline action_print(struct rte_mbuf* packet) {
    if (packet == NULL) goto error;
    struct ether_hdr* l2 = rte_pktmbuf_mtod(packet, struct ether_hdr*);
    rte_hexdump(stdout, "PACKET", l2, 18);

    error:
    return;
}

static void inline action_loop(struct rte_mbuf* packet, Port* port) {
    struct ether_hdr* eth = rte_pktmbuf_mtod(packet, struct ether_hdr*);
    eth->d_addr = eth->s_addr;
    action_output(packet, port);
}

static void inline
action_learn(struct rte_mbuf* packet, Switch* s, Port* p) {
    if (s->hashmap != NULL && packet != NULL) {
        struct ether_hdr* l2 = rte_pktmbuf_mtod(packet, struct ether_hdr*);
        struct ether_addr key = l2->s_addr;
        int ret = rte_hash_add_key(s->hashmap, (void*)&key);
        s->hashmap_ports[ret] = p;
    }
}
