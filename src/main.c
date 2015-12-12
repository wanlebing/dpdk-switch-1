#include "main.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>

#include <inttypes.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_mbuf.h>

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250

#define RX_RING_SIZE 2048
#define TX_RING_SIZE 1024

static const struct rte_eth_conf port_conf_default = {
	.rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN }
};


int forwarding_loop(void)
{
	printf("Forwarding loop started\n");
	return 0;
}


void port_init(int port, struct rte_mempool *mbuf_pool)
{
	//default port config
	struct rte_eth_conf port_conf =  {
		.rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN }
	};

	
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

int main(int argc, char **argv)
{
	int ret;
	int num_port;
	int num_mbufs = 8191;

	printf("%s\n", argv[1]);

	//EAL initialization
	ret = rte_eal_init(argc, argv);
	if (ret < 0) rte_panic("Cannot init EAL\n");

	
	num_port = rte_eth_dev_count();
	
	//MBUF initialziation
	struct rte_mempool *mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", num_mbufs * num_port, MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	//ports initialization
	int i;
	
	for (i = 0; i < num_port; ++i)
	{
		port_init(i, mbuf_pool);
	}


	//main forwarding loop
	forwarding_loop();

	rte_eal_mp_wait_lcore();
	return 0;
}
