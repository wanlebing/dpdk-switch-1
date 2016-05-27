#include "switch.h"
#include "port.h"
#include "list.h"
#include "actions.c"

#include <unistd.h>
#include <stdio.h>

#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_malloc.h>

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 0

#define BURST_RX_SIZE 1
#define BURST_TX_SIZE 1

Switch sw;

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
                    }
                    break;
            }

            if (received < 1) goto next_port;

            rte_ring_sp_enqueue_bulk(p->ring_rx, (void**) s->mbuf_rx, received);

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
                        int enq = rte_vhost_enqueue_burst(p->virtio_dev, 0 * VIRTIO_QNUM + VIRTIO_RXQ, p->mbuf_tx,
                                                p->mbuf_tx_counter);
                        p->mbuf_tx_counter -= enq;
                        for (int i = 0; i < enq; ++i) {
                            printf("Sent %d\n to port %s)\n", enq, p->name);
                            action_print(p->mbuf_tx[i]);
                        }
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

            if (p->ring_rx != NULL)
                received = rte_ring_sc_dequeue_burst(p->ring_rx, (void**) s->mbuf_pipeline, 256);

            if (received < 1) continue;

            for (int i = 0; i < received; ++i) {
          //      action_print(s->mbuf_pipeline[i]);
                action_flood(s->mbuf_pipeline[i], s, p);
            }
        }

    }
    return 0;
}

void switch_run(Switch* s, int argc, char** argv) {
    switch_init(s, argc, argv);

    /* Launch all loops on separate cores */
    rte_eal_remote_launch(switch_rx_loop, (void*) s, 1);
    rte_eal_remote_launch(switch_pipeline, (void*) s, 2);
    rte_eal_remote_launch(switch_tx_loop, (void*) s, 3);

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

    for (int i = 0; i < phy_n; ++i) {
        Port* p;
        p = port_init_phy(i, s->mp);
        list_insert(s->ports, p);
    }

    /* Initialize vhost ports */
    for (int i = 0; i < vhost_n; ++i) {
        Port* p = port_init_vhost(i, s->mp);
        list_insert(s->ports, p);
    }

}
