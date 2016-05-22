#include "port.h"

#include <stdlib.h>

#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>

#define RX_RING_SIZE 4096
#define TX_RING_SIZE 4096

Port* port_init_phy(int phy_id, struct rte_mempool* mbuf_pool) {
    Port* p = malloc(sizeof(Port));

    /* Set port type to physical */
    p->type = PHY;

    struct rte_eth_conf conf =  {
        .rxmode = {
            .max_rx_pkt_len = ETHER_MAX_LEN,
            .mq_mode = ETH_MQ_RX_DCB_RSS,
        }
    };

    /* Configure physical device */
    rte_eth_dev_configure(phy_id, 1, 1, &conf);

    /* Allocate memory for RX and TX queues */
    p->mbuf_tx = rte_malloc_socket(NULL, sizeof(struct rte_mbuf) * MBUF_TX_MAX,
            RTE_CACHE_LINE_SIZE, rte_socket_id());
    rte_eth_rx_queue_setup(phy_id, 0, RX_RING_SIZE, rte_eth_dev_socket_id(phy_id), NULL, mbuf_pool);
    rte_eth_tx_queue_setup(phy_id, 0, TX_RING_SIZE, rte_eth_dev_socket_id(phy_id), NULL);

    /* Set VLAN tag to 0 */
    p->vlan_tag = 0;

    /* Set device name */
    snprintf(p->name, MAX_NAME_LEN, "phy%d", phy_id);

    /* Start physical port */
    rte_eth_dev_start(phy_id);

    return p;
}

const struct virtio_net_device_ops port_ops_vhost = {
    .new_device =  _port_init_vhost,
};

int _port_init_vhost(struct virtio_net* dev) {
    
}


Port* port_init_vhost(int vhost_id, struct rte_mempool* mbuf_pool) {
    Port* p = malloc(sizeof(Port));

    p->type = VHOST;

    snprintf(p->name, MAX_NAME_LEN, "vhost%d", vhost_id); 

    rte_vhost_driver_callback_register(&port_ops_vhost); 

    rte_vhost_driver_register(p->name);
}

