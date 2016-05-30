#ifndef PORT_H
#define PORT_H

#include <stdbool.h>

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
    int vlan_trunks[4096];

    struct rte_mbuf** mbuf_tx;
    int mbuf_tx_counter;

    struct rte_ring* ring_tx;
    struct rte_ring* ring_rx;

    uint64_t stats_rx_packets;
    uint64_t stats_rx_bytes;
    uint64_t stats_rx_dropped;
    uint64_t stats_tx_packets;
    uint64_t stats_tx_bytes;
    uint64_t stats_tx_dropped;

    char name[MAX_NAME_LEN];
    struct virtio_net *virtio_dev;
    int id;
} Port;

Port* port_init_phy(int phy_id, struct rte_mempool* mbuf_pool);
Port* port_init_vhost(int vhost_id, struct rte_mempool* mbuf_pool);
int port_is_virtio_dev_runnning(Port* p);

void port_set_vlan_tag(Port* p, int tag);
void port_unset_vlan_tag(Port* p);
int port_get_vlan_tag(Port* p);

void port_set_vlan_trunk(Port* p, int tag);
void port_unset_vlan_trunk(Port* p, int tag);
bool port_is_vlan_trunk(Port* p, int tag);

void port_update_rx_stats(Port* p, int n, int bytes, int dropped);
void port_update_tx_stats(Port* p, int n, int bytes, int dropped);
void port_print_stats(Port* p);

#endif /* PORT_H */
