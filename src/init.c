#include "init.h"
#include "config.h"
#include "stats.h"

#include <stdio.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_hash.h>
#include <rte_hash_crc.h>
#include <rte_virtio_net.h>
#include <inttypes.h>

#define RX_RING_SIZE 4096
#define TX_RING_SIZE 4096
#define MAX_PORTS 4

void
init_app_config(void)
{
    //Burst sizes
    app.burst_size_worker_write = 128;
    app.burst_size_worker_read = 128;
    app.burst_size_tx_read = 128;
    app.burst_size_tx_write = 128;
    app.burst_size_rx_read = 128;
    app.burst_size_rx_write = 128;

    //Rings
    app.ring_rx_size = 2048;
    app.ring_tx_size = 2048;
}

static const struct rte_eth_conf port_conf_default = {
    .rxmode = {
            .max_rx_pkt_len = ETHER_MAX_LEN,
            .hw_vlan_filter = 1,
            .mq_mode = ETH_MQ_RX_DCB_RSS,
    },
};

void
init_rings(int n_ports)
{
    int i;

    for (i = 0; i < n_ports; i++) {
        char name[32];

        snprintf(name, sizeof(name), "ring_rx_%u", i);

        app.ports[i].ring_rx = rte_ring_create(name, app.ring_rx_size, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);

        if (app.ports[i].ring_rx == NULL)
            rte_panic("Cannot create RX ring %u\n", i);
    }

    for (i = 0; i < n_ports; i++) {
        char name[32];

        snprintf(name, sizeof(name), "ring_tx_%u", i);

        app.ports[i].ring_tx = rte_ring_create(name, app.ring_tx_size, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);

        if (app.ports[i].ring_tx == NULL)
            rte_panic("Cannot create TX ring %u\n", i);
    }

    for (i = 0; i < 8; i++) {
        char name[32];

        snprintf(name, sizeof(name), "ring_qos_%u", i);

        app.rings_pre_tx[i] = rte_ring_create(name, app.ring_tx_size, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);

        if (app.rings_pre_tx[i] == NULL)
            rte_panic("Cannot create QoS ring %u\n", i);
    }

}


void
init_mbufs(void)
{
    return;
}

void
init_vlan(void)
{
    int ret = rte_eth_dev_vlan_filter(0, 2005, 1);
    printf("dev_vlan_filter status port 0: %d\n", ret);
    rte_eth_dev_vlan_filter(1, 2005, 1);
    printf("dev_vlan_filter status port 1: %d\n", ret);
}

int
set_port_vlan_tag(uint32_t port, uint16_t tag)
{
    if (tag > 4096) {
        return -1;
    }

    if (port > 32) {
        return -1;
    }

    app.ports[port].vlan_tag = tag;

    return 0;
}

int
set_port_vlan_trunk(uint32_t port, uint16_t tag)
{
    app.ports[port].vlan_trunks[tag] = 1;

    return 0;
}

static inline uint32_t
hash_crc(const void *data, __attribute__((unused)) uint32_t data_len, uint32_t init_val)
{
    const struct ether_addr *k;
    uint32_t t;
    const uint32_t *p;

    k = data;
    t = k->addr_bytes[0];
    p = (const uint32_t *)&k->addr_bytes[5];

#ifdef RTE_MACHINE_CPUFLAG_SSE4_2

    init_val = rte_hash_crc_4byte(t, init_val);
    init_val = rte_hash_crc_4byte(k->addr_bytes[1], init_val);
    init_val = rte_hash_crc_4byte(k->addr_bytes[2], init_val);
    init_val = rte_hash_crc_4byte(k->addr_bytes[3], init_val);
    init_val = rte_hash_crc_4byte(k->addr_bytes[4], init_val);
    init_val = rte_hash_crc_4byte(*p, init_val);

#else /* RTE_MACHINE_CPUFLAG_SSE4_2 */
    init_val = rte_jhash_1word(t, init_val);
    init_val = rte_jhash_1word(k->addr_bytes[1], init_val);
    init_val = rte_jhash_1word(k->addr_bytes[2], init_val);
    init_val = rte_jhash_1word(k->addr_bytes[3], init_val);
    init_val = rte_jhash_1word(k->addr_bytes[4], init_val);
    init_val = rte_jhash_1word(*p, init_val);
#endif /* RTE_MACHINE_CPUFLAG_SSE4_2 */
    return (init_val);
}



void
init_hash(void)
{
    int socketid = 0; //CPU socket (non-NUMA => socketid 0)

    struct rte_hash_parameters hash_params = {
        .name = NULL,
        .entries = 1024,
        .key_len = sizeof(struct ether_addr),
        .hash_func = hash_crc,
        .hash_func_init_val = 0,
    };

    char s[64];

    /* create ipv4 hash */
    snprintf(s, sizeof(s), "hash_%d", socketid);
    hash_params.name = s;
    hash_params.socket_id = socketid;
    lookup_struct = rte_hash_create(&hash_params);

    if (lookup_struct == NULL)
    {
        rte_exit(EXIT_FAILURE, "Unable to create the hash on socket %d\n", socketid);
    }
}

void
port_init(int port, struct rte_mempool *mbuf_pool)
{
    //default port config
    struct rte_eth_conf port_conf =  {
        .rxmode = {
            .max_rx_pkt_len = ETHER_MAX_LEN,
            .mq_mode = ETH_MQ_RX_DCB_RSS,
        }
    };

    app.ports[port].index = port;
    app.ports[port].type = PHY;

    app.ports[port].mbuf_tx = rte_malloc_socket(NULL, sizeof(struct mbuf_array), RTE_CACHE_LINE_SIZE, rte_socket_id());

    //1/1 tx/rx queue per port
    int rx_rings = 1;
    int tx_rings = 1;

    int q;

    if (port >= rte_eth_dev_count()) return;

    //Ethernet device configuration
    rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);

    //rx queue allocation and setup
    for (q = 0; q < rx_rings; q++) {
        rte_eth_rx_queue_setup(port, q, RX_RING_SIZE, rte_eth_dev_socket_id(port), NULL, mbuf_pool);
    }

    //tx queue allocation and setup
    for (q = 0; q < tx_rings; q++) {
        rte_eth_tx_queue_setup(port, q, TX_RING_SIZE, rte_eth_dev_socket_id(port), NULL);
    }

    /* Start the Ethernet port. */
    rte_eth_dev_start(port);

    /* Display the port MAC address. */
    struct ether_addr addr;
    rte_eth_macaddr_get(port, &addr);

    /* Enable RX in promiscuous mode for the Ethernet device. */
    rte_eth_promiscuous_enable(port);
}

static int
new_device (struct virtio_net *dev)
{
    struct virtio_net_data_ll *ll_dev;
    int lcore, core_add = 0;
    uint32_t device_num_min = num_devices;
    struct vhost_dev *vdev;
    uint32_t regionidx;

    vdev = rte_zmalloc("vhost device", sizeof(*vdev), RTE_CACHE_LINE_SIZE);
    if (vdev == NULL) {
        RTE_LOG(INFO, VHOST_DATA, "(%"PRIu64") Couldn't allocate memory for vhost dev\n",
            dev->device_fh);
        return -1;
    }
    vdev->dev = dev;
    dev->priv = vdev;

    /* Add device to main ll */
    ll_dev = get_data_ll_free_entry(&ll_root_free);
    if (ll_dev == NULL) {
        RTE_LOG(INFO, VHOST_DATA, "(%"PRIu64") No free entry found in linked list. Device limit "
            "of %d devices per core has been reached\n",
            dev->device_fh, num_devices);
        if (vdev->regions_hpa)
            rte_free(vdev->regions_hpa);
        rte_free(vdev);
        return -1;
    }
    ll_dev->vdev = vdev;
    add_data_ll_entry(&ll_root_used, ll_dev);
    vdev->vmdq_rx_q
        = dev->device_fh * queues_per_pool + vmdq_queue_base;

    /*reset ready flag*/
    vdev->ready = DEVICE_MAC_LEARNING;
    vdev->remove = 0;

    /* Find a suitable lcore to add the device. */
    RTE_LCORE_FOREACH_SLAVE(lcore) {
        if (lcore_info[lcore].lcore_ll->device_num < device_num_min) {
            device_num_min = lcore_info[lcore].lcore_ll->device_num;
            core_add = lcore;
        }
    }
    /* Add device to lcore ll */
    ll_dev = get_data_ll_free_entry(&lcore_info[core_add].lcore_ll->ll_root_free);
    if (ll_dev == NULL) {
        RTE_LOG(INFO, VHOST_DATA, "(%"PRIu64") Failed to add device to data core\n", dev->device_fh);
        vdev->ready = DEVICE_SAFE_REMOVE;
        destroy_device(dev);
        rte_free(vdev->regions_hpa);
        rte_free(vdev);
        return -1;
    }
    ll_dev->vdev = vdev;
    vdev->coreid = core_add;

    add_data_ll_entry(&lcore_info[vdev->coreid].lcore_ll->ll_root_used, ll_dev);

    /* Disable notifications. */
    rte_vhost_enable_guest_notification(dev, VIRTIO_RXQ, 0);
    rte_vhost_enable_guest_notification(dev, VIRTIO_TXQ, 0);
    lcore_info[vdev->coreid].lcore_ll->device_num++;
    dev->flags |= VIRTIO_DEV_RUNNING;

    RTE_LOG(INFO, VHOST_DATA, "(%"PRIu64") Device has been added to data core %d\n", dev->device_fh, vdev->coreid);

    return 0;
}


static const struct virtio_net_device_ops virtio_net_device_ops =
{
     .new_device =  new_device,
//     .destroy_device = destroy_device,
};

void
vhost_init(int port, struct rte_mempool *mbuf_pool)
{
    app.ports[port].index = port;
    app.ports[port].type = VHOST;
    app.ports[port].mp = mbuf_pool;

    char vhost_name[6];
    sprintf(vhost_name, "vhost%d", port);
    printf("%d\n", port);

    rte_vhost_driver_callback_register(&virtio_net_device_ops);

    rte_vhost_driver_register(vhost_name); 
    app.ports[port].mbuf_tx = rte_malloc("VHOST_TXQ", 1 * sizeof(struct mbuf_array), 0);
    app.ports[port].mbuf_tx->n_mbufs = 0;
}
