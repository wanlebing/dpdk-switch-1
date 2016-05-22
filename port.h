#ifndef PORT_H
#define PORT_H

#include <rte_mempool.h>
#include <rte_mbuf.h>

#define MBUF_TX_MAX 4096
#define MAX_NAME_LEN 32

typedef enum port_type {
    PHY,
    VHOST
} port_type;

typedef struct Port {
    port_type type;
    int vlan_tag;
    struct rte_mbuf* mbuf_tx;
    char name[MAX_NAME_LEN];
    struct virtio_net *virtio_dev;
} Port;

Port* port_init_phy(int phy_id, struct rte_mempool* mbuf_pool);
int _port_init_vhost(struct virtio_net* dev);
Port* port_init_vhost(struct rte_mempool* mbuf_pool);

#endif /* PORT_H */
