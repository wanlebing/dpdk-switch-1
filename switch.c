#include "switch.h"
#include "port.h"
#include "list.h"
#include "actions.c"

#include "murmurhash.h"

#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>

#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_ether.h>

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 0

#define BURST_RX_SIZE 256
#define BURST_TX_SIZE 256

#define clear() printf("\033[H\033[J")
#define noop (void)0

Switch sw;

bool static inline is_vlan(struct rte_mbuf* packet) {
    struct ether_hdr* eth = rte_pktmbuf_mtod(packet, struct ether_hdr*);
    return (rte_cpu_to_be_16(eth->ether_type) == ETHER_TYPE_VLAN);
}

uint16_t static inline get_vlan_tag(struct rte_mbuf* packet) {
    struct ether_hdr* eth = rte_pktmbuf_mtod(packet, struct ether_hdr*);
    struct vlan_hdr* vlan = (struct vlan_hdr *) (eth + 1);
    return (rte_be_to_cpu_16(vlan->vlan_tci) & 0x0FFF);
}

void static inline switch_process_vlan(Port* port, struct rte_mbuf** mbuf, int n) {
    int tag;
    if (tag = port_get_vlan_tag(port)) {
        for (int i = 0; i < n; ++i) {
            if (!is_vlan(mbuf[i])) action_push_vlan(mbuf[i], tag);
            else if (tag == get_vlan_tag(mbuf[i])) action_pop_vlan(mbuf[i]);
            else if (port_is_vlan_trunk(port, get_vlan_tag(mbuf[i]))) noop;
            else action_drop(mbuf[i]);
        }
    }
}

static inline Port* switch_lookup_hash(struct rte_mbuf* packet, Switch* s) {
    Port* dst_port = NULL;
    struct ether_hdr* eth = rte_pktmbuf_mtod(packet, struct ether_hdr*);
    struct ether_addr key = eth->d_addr;

    if (is_multicast_ether_addr(&key) || is_broadcast_ether_addr(&key)) goto lookup_end;

    int ret = rte_hash_lookup(s->hashmap, (const void *)&key);
    switch (ret) {
      case -EINVAL:
      case -ENOENT:
        dst_port = NULL;
        break;
      default:
        dst_port = s->hashmap_ports[ret];
        break;
    }

lookup_end:
    return dst_port;
}

int switch_rx_loop(void* _s) {
    Switch* s = (Switch*) _s;
    int received = 0;

    while (1) {
        Node* current = s->ports->head;
        while (current != NULL) {
            Port* p = (Port*) current->value;

            /* Port RX action */
            switch(p->type) {
                case PHY:
                    received = rte_eth_rx_burst(p->id, 0, s->mbuf_rx, BURST_RX_SIZE);
                    break;
                case VHOST:
                    if (port_is_virtio_dev_runnning(p)) {
                        received = rte_vhost_dequeue_burst(p->virtio_dev, 0 * VIRTIO_QNUM + VIRTIO_TXQ,
                                                           s->mp, s->mbuf_rx, BURST_RX_SIZE);
                    } else goto next_port;
                    break;
            }

            if (received < 1) goto next_port;

            for (int i = 0; i < received; ++i) {
                port_update_rx_stats(p, 1, s->mbuf_rx[i]->pkt_len, 0);
            }

            /* VLAN tagging/untagging on RX side */
            switch_process_vlan(p, s->mbuf_rx, received);

            int enq = rte_ring_sp_enqueue_bulk(p->ring_rx, (void**) s->mbuf_rx, received);
            port_update_rx_stats(p, 0, 0, received-enq);

next_port:
            current = current->next;
        }
    }
    return 0;
}

int switch_tx_loop(void* _s) {
    Switch* s = (Switch*) _s;
    while (1) {
        Node* current = s->ports->head;
        while (current != NULL) {
            Port* p = (Port*) current->value;

            if (p->ring_tx != NULL) {
                /* TODO define 256 as MAX_TX_RING_DEQUEUE SIZE */
                p->mbuf_tx_counter += rte_ring_sc_dequeue_burst(p->ring_tx, (void**) &(p->mbuf_tx), 256);
            } else {
                goto next_port;
            }

            if (p->mbuf_tx_counter < 1) {
                goto next_port;
            }

            /* Port TX action */
            switch (p->type) {
                case PHY:
                    rte_eth_tx_burst(p->id, 0, p->mbuf_tx, p->mbuf_tx_counter);
                    break;
                case VHOST:
                    if (port_is_virtio_dev_runnning(p)) {
                        /* VLAN tagging/untagging on TX side */
                        switch_process_vlan(p, p->mbuf_tx, p->mbuf_tx_counter);

                        int enq = rte_vhost_enqueue_burst(p->virtio_dev, 0 * VIRTIO_QNUM + VIRTIO_RXQ, p->mbuf_tx,
                                                p->mbuf_tx_counter);
                        p->mbuf_tx_counter -= enq;
                        port_update_tx_stats(p, enq, 0, 0);
                    }
                    break;
            }

next_port:
            current = current->next;
        }
    }
    return 0;
}

int switch_pipeline(void* _s) {
    Switch* s = (Switch*) _s;
    int received;
    while (1) {
        for (Node* n = s->ports->head; n != NULL; n=n->next) {
            received = 0;
            Port* p = (Port*)n->value;

            if (p->ring_rx != NULL) {
                received = rte_ring_sc_dequeue_burst(p->ring_rx, (void**) s->mbuf_pipeline, 256);
            }

            if (received < 1) continue;

            for (int i = 0; i < received; ++i) {
                action_learn(s->mbuf_pipeline[i], s, p);
                Port* dst_port = switch_lookup_hash(s->mbuf_pipeline[i], s);
                if (dst_port != NULL) action_output(s->mbuf_pipeline[i], dst_port);
                else action_flood(s->mbuf_pipeline[i], s, p);
            }
        }

    }
    return 0;
}

/* Stats printing thread */
void* switch_print_stats_loop(void *_s)
{
  Switch* s = (Switch*) _s;
  while (1) {
      for (Node* n = s->ports->head; n != NULL; n=n->next) {
          Port* p = (Port*)n->value;
          port_print_stats(p);
      }
      sleep(1);
      clear();
  }

  /* the function must return something - NULL will do */
  return NULL;
}

/* Stats printing thread */
void* switch_control_loop(void *_s)
{
  Switch* s = (Switch*) _s;
  while (1) {
      sleep(1);
  }

  /* the function must return something - NULL will do */
  return NULL;
}

void switch_run(Switch* s, int argc, char** argv) {
    switch_init(s, argc, argv);

    /* Launch all loops on separate cores */
    rte_eal_remote_launch(switch_rx_loop, (void*) s, 1);
    rte_eal_remote_launch(switch_pipeline, (void*) s, 2);
    rte_eal_remote_launch(switch_tx_loop, (void*) s, 3);

    /* Launch other threads on master core */
    pthread_t stats_thread, control_thread;
    pthread_create(&stats_thread, NULL, switch_print_stats_loop, s);
    pthread_create(&control_thread, NULL, switch_control_loop, s);

    /* Start vhost session */
    rte_vhost_driver_session_start();

    rte_eal_mp_wait_lcore();
}

void switch_init(Switch* s, int argc, char** argv) {
    /* Initialize EAL */
    int ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        rte_panic("Cannot init EAL\n");
    }

    /* Initialize port list */
    s->ports = list_init();

    int phy_n = rte_eth_dev_count();
    int vhost_n = 2;

    /* Initialize physical ports and mbuf pool */
    s->mp = rte_pktmbuf_pool_create("MBUF_POOL", (phy_n + vhost_n) * 2 * NUM_MBUFS, MBUF_CACHE_SIZE, 0,
                                    RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

    switch_init_hash(s);

    for (int i = 0; i < phy_n; ++i) {
        Port* p;
        p = port_init_phy(i, s->mp);
        list_insert(s->ports, p);
    }

    /* Initialize vhost ports */
    for (int i = 0; i < vhost_n; ++i) {
        Port* p = port_init_vhost(i, s->mp);
        port_set_vlan_tag(p, 5);
        list_insert(s->ports, p);
    }
}

static inline uint32_t hash(const void *data, __attribute__((unused)) uint32_t data_len, uint32_t init_val)
{
    return MurmurHash2(data, sizeof(struct ether_addr), init_val);
}

void switch_init_hash(Switch* s) {
    int socketid = 0; //CPU socket (non-NUMA => socketid 0)

    struct rte_hash_parameters hash_params = {
        .name = NULL,
        .entries = 1024,
        .key_len = sizeof(struct ether_addr),
        .hash_func = hash,
        .hash_func_init_val = 1,
    };

    char name[64];
    /* create ipv4 hash */
    snprintf(name, sizeof(name), "hash_%d", socketid);
    hash_params.name = name;
    hash_params.socket_id = socketid;
    s->hashmap = rte_hash_create(&hash_params);

    if (s->hashmap == NULL) rte_exit(EXIT_FAILURE, "Unable to create the hashmap");
}
