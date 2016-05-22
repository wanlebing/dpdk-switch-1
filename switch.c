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

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 0

int switch_rx_loop(void* s) {
    Switch* sw = (Switch*) s;
    while (1) {
        Node* current = sw->ports->head;
        while (current != NULL) {
            Port* p = (Port*) current->value;

            /* Port RX action */
            printf("%s\n", p->name);
            sleep(1);

            current = current->next;
        }
    }
    return 0;
}

int switch_tx_loop(void* s) {
    while (1);
    return 0;
}

int switch_pipeline(void* s) {
    Switch* sw = (Switch*) s;
    while (1) {
        Node* current = sw->ports->head;
        while (current != NULL) {
            Port* p = (Port*) current->value;

            /* Port TX action */
            //TODO

            current = current->next;
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
        list_insert(s->ports, (void*) p);
    }

}
