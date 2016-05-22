#include "switch.h"
#include "port.h"
#include "list.h"

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

#define BURST_RX_SIZE 256
#define BURST_TX_SIZE 256

Switch sw;

int switch_rx_loop(void* _s) {
    Switch* s = (Switch*) _s;
    int received;

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
                        received = rte_vhost_dequeue_burst(p->virtio_dev, p->id * VIRTIO_QNUM + VIRTIO_TXQ,
                                                           s->mp, s->mbuf_rx, BURST_RX_SIZE);
                    }
                    break;
            }

            if (received < 1) continue;

            int m;
            for (m = 0; m < received; ++m) {
                rte_pktmbuf_free(s->mbuf_rx[m]);
            }

            current = current->next;
        }
        sleep(1);
    }
    return 0;
}

int switch_tx_loop(void* _s) {
    Switch* s = (Switch*) _s;
    while (1) {
        Node* current = s->ports->head;
        while (current != NULL) {
            Port* p = (Port*) current->value;

            /* Port TX action */
            switch (p->type) {
                case PHY:
                    rte_eth_tx_burst(p->id, 0, p->mbuf_tx, BURST_TX_SIZE);
                    break;
                case VHOST:
                    if (port_is_virtio_dev_runnning(p)) {
                        printf("%s\n", p->name);
                    }
                    break;
            }
        }
    }
    return 0;
}

int switch_pipeline(void* _s) {
    return 0;
}

void switch_run(Switch* s, int argc, char** argv) {
    switch_init(s, argc, argv);

    printf("switch address: %d\n", s);

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
   
    /* Allocate memory Switch variable */
    printf("switch address: %d\n", s);

    /* Initialize port list */
    s->ports = list_init();

    int phy_n = rte_eth_dev_count();
    int vhost_n = 2;

    /* Initialize physical ports and mbuf pool */
    s->mp = rte_pktmbuf_pool_create("MBUF_POOL", (phy_n + vhost_n) * NUM_MBUFS, MBUF_CACHE_SIZE, 0,
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
