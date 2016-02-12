#include "init.h"
#include "config.h"
#include "stats.h"

#include <stdio.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>

#define RX_RING_SIZE 4096
#define TX_RING_SIZE 4096
#define MAX_PORTS 4

void init_app_config(void)
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

void init_rings(int n_ports)
{
	int i;

	for (i = 0; i < n_ports; i++) {
		char name[32];

		snprintf(name, sizeof(name), "ring_rx_%u", i);

		app.rings_rx[i] = rte_ring_create(name, app.ring_rx_size, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);

		if (app.rings_rx[i] == NULL)
			rte_panic("Cannot create RX ring %u\n", i);
	}

	for (i = 0; i < n_ports; i++) {
		char name[32];

		snprintf(name, sizeof(name), "ring_tx_%u", i);

		app.rings_tx[i] = rte_ring_create(name, app.ring_tx_size, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);

		if (app.rings_tx[i] == NULL)
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


void init_mbufs(void)
{
	return;
}

void init_vlan(void)
{
	int ret = rte_eth_dev_vlan_filter(0, 2005, 1);
	printf("dev_vlan_filter status port 0: %d\n", ret);
	rte_eth_dev_vlan_filter(1, 2005, 1);
	printf("dev_vlan_filter status port 1: %d\n", ret);
}

int set_port_vlan_tag(uint32_t port, uint16_t tag)
{
	if (tag > 4096) {
		return -1;
	}

	if (port > 32) {
		return -1;
	}

	app.vlan_tags[port] = tag;

	return 0;
}

int set_port_vlan_trunk(uint32_t port, uint16_t tag)
{
	app.vlan_trunks[app.ports[port]][tag] = 1;

	return 0;
}

void port_init(int port, struct rte_mempool *mbuf_pool)
{
    //default port config
    struct rte_eth_conf port_conf =  {
		.rxmode = {
			.max_rx_pkt_len = ETHER_MAX_LEN,
			.mq_mode = ETH_MQ_RX_DCB_RSS,
		}
    };

    app.ports[port] = port;

    app.mbuf_tx[port] = rte_malloc_socket(NULL, sizeof(struct mbuf_array), RTE_CACHE_LINE_SIZE, rte_socket_id());

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

