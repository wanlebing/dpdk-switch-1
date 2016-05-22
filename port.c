#include "port.h"
#include "switch.h"

#include <stdlib.h>
#include <unistd.h>

#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_virtio_net.h>

#define RX_RING_SIZE 4096
#define TX_RING_SIZE 4096

Port* port_init_phy(int phy_id, struct rte_mempool* mbuf_pool) {
    Port* p = malloc(sizeof(Port));

    /* Set port type to physical */
    p->type = PHY;

    /* Set physical port id */
    p->id = phy_id;

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

    rte_eth_promiscuous_enable(phy_id);

    return p;
}

static int new_device(struct virtio_net* dev) {
    RTE_LOG(DEBUG, USER1, "Callback: ifname=%s\n", dev->ifname);
    
    Port* p;
    Node* node = sw.ports->head;

    while (node != NULL) {
        p = node->value;
        if(!strcmp(p->name, dev->ifname)) {
            break;
        } else {
            node = node->next;
        }
    } 

    p->virtio_dev = dev;
    dev->priv = p;

    rte_vhost_enable_guest_notification(dev, VIRTIO_RXQ, 0);
    rte_vhost_enable_guest_notification(dev, VIRTIO_TXQ, 0);
    dev->flags |= VIRTIO_DEV_RUNNING;
}

static const struct virtio_net_device_ops virtio_net_device_ops = {
    .new_device =  new_device,
};

Port* port_init_vhost(int vhost_id, struct rte_mempool* mbuf_pool) {
    Port* p = malloc(sizeof(Port));
    p->type = VHOST;
    p->virtio_dev = NULL;
    p->id = vhost_id;

    snprintf(p->name, MAX_NAME_LEN, "vhost%d", vhost_id);

    /* Remove existing vhost socket file */
    unlink(p->name);

    rte_vhost_driver_callback_register(&virtio_net_device_ops); 
    rte_vhost_driver_register(p->name);

    return p;
}

int port_is_virtio_dev_runnning(Port* p) {
    if ((p->virtio_dev != NULL) && (p->virtio_dev->flags & VIRTIO_DEV_RUNNING)) {
        return 1;
    } else {
        return 0;
    }
}
