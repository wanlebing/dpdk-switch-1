#ifndef PORT_H
#define PORT_H

#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_virtio_net.h>

#define MBUF_TX_MAX 4096
#define MAX_NAME_LEN 32

typedef enum port_type {
    PHY,
    VHOST
} port_type;

typedef struct Port {
    port_type type;
    int vlan_tag;

    struct rte_mbuf** mbuf_tx;
    int mbuf_tx_counter;

    struct rte_ring* ring_tx;
    struct rte_ring* ring_rx;

    char name[MAX_NAME_LEN];
    struct virtio_net *virtio_dev;
    int id;
} Port;

Port* port_init_phy(int phy_id, struct rte_mempool* mbuf_pool);
Port* port_init_vhost(int vhost_id, struct rte_mempool* mbuf_pool);
int port_is_virtio_dev_runnning(Port* p);

#endif /* PORT_H */
