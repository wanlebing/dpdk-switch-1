#include "init.h"
#include "config.h"
#include "stats.h"

#include <stdio.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_hash.h>
#include <rte_hash_crc.h>
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

	app.vlan_tags[port] = tag;

	return 0;
}

int
set_port_vlan_trunk(uint32_t port, uint16_t tag)
{
	app.vlan_trunks[app.ports[port]][tag] = 1;

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

