#include "port.h"
#include "switch.h"

#include <stdlib.h>
#include <unistd.h>

#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_virtio_net.h>
#include <rte_ring.h>

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
    p->mbuf_tx_counter = 0;
    rte_eth_rx_queue_setup(phy_id, 0, RX_RING_SIZE, rte_eth_dev_socket_id(phy_id), NULL, mbuf_pool);
    rte_eth_tx_queue_setup(phy_id, 0, TX_RING_SIZE, rte_eth_dev_socket_id(phy_id), NULL);

    char name[14];
    snprintf(name, sizeof(name), "ring_tx_phy_%u", phy_id);
    p->ring_tx = rte_ring_create(name, TX_RING_SIZE, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);

    /* Set VLAN tag to 0 */
    p->vlan_tag = 0;

    /* Set device name */
    snprintf(p->name, MAX_NAME_LEN, "phy%d", phy_id);

    /* Start physical port */
    rte_eth_dev_start(phy_id);

    rte_eth_promiscuous_enable(phy_id);

    return p;
}

static void port_vhost_disable_notifications(struct virtio_net *virtio_dev)
{
    uint32_t i;
    uint64_t idx;

    for (i = 0; i < virtio_dev->virt_qp_nb; i++) {
        idx = i * VIRTIO_QNUM;
        rte_vhost_enable_guest_notification(virtio_dev, idx + VIRTIO_RXQ, 0);
        rte_vhost_enable_guest_notification(virtio_dev, idx + VIRTIO_TXQ, 0);
    }
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

    char name[14];
    snprintf(name, sizeof(name), "ring_tx_vhu_%c", p->name[5]);
    p->ring_tx = rte_ring_create(name, TX_RING_SIZE, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);

    snprintf(name, sizeof(name), "ring_rx_vhu_%c", p->name[5]);
    p->ring_rx = rte_ring_create(name, RX_RING_SIZE, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);

    p->mbuf_tx = rte_malloc(p->name, sizeof(struct rte_mbuf) * MBUF_TX_MAX, 0);

    port_vhost_disable_notifications(dev);

    dev->flags |= VIRTIO_DEV_RUNNING;
}

static const struct virtio_net_device_ops virtio_net_device_ops = {
    .new_device =  new_device,
};

Port* port_init_vhost(int vhost_id, struct rte_mempool* mbuf_pool) {
    Port* p = malloc(sizeof(Port));
    p->type = VHOST;
    p->virtio_dev = NULL;
    p->ring_tx = NULL;
    p->ring_rx = NULL;
    p->id = vhost_id;

    p->mbuf_tx_counter = 0;

    snprintf(p->name, MAX_NAME_LEN, "vhost%d", vhost_id);

    /* Remove existing vhost socket file */
    unlink(p->name);

    rte_vhost_driver_callback_register(&virtio_net_device_ops);
    rte_vhost_driver_register(p->name);

    return p;
}

int port_is_virtio_dev_runnning(Port* p) {
    return ((p->ring_rx != NULL) && (p->virtio_dev != NULL) && (p->virtio_dev->flags & VIRTIO_DEV_RUNNING));
}