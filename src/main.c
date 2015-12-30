#include "main.h"
#include "utils.h"

//C
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>
#include <inttypes.h>

//DPDK
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_mbuf.h>

//Judy arrays
#include <Judy.h>

//for DPDK
#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250

#define RX_RING_SIZE 2048
#define TX_RING_SIZE 1024

#define BURST_SIZE 32

//for Judy arrays
#define MAXLINELEN 32

uint8_t	Index[MAXLINELEN];       // string to insert

Pvoid_t	PJArray;		// Judy array.
PWord_t PValue;               // Judy array element.
Word_t	Bytes;                // size of JudySL array.

static const struct rte_eth_conf port_conf_default = {
    .rxmode = { .max_rx_pkt_len = ETHER_MAX_LEN }
};

void dump_mac_table(void)
{
    printf("MAC address\t\tPort\n-------------------------------\n");
    Index[0] = '\0';                    // start with smallest string.
    JSLF(PValue, PJArray, Index);       // get first string
    while (PValue != NULL)
    {
	printf("%s\t%lu\n", Index, *PValue);
	JSLN(PValue, PJArray, Index);   // get next string
    }
}

int forwarding_loop(int num_ports)
{
    printf("Forwarding loop started\n");

    int port;

    while (1)
    {
	for (port = 0; port < num_ports; port++)
	{

	    /* Get burst of RX packets, from first port of pair. */
	    struct rte_mbuf *bufs[BURST_SIZE];
	    const uint16_t nb_rx = rte_eth_rx_burst(port, 0, bufs, BURST_SIZE);


	    if (unlikely(nb_rx == 0))
	    {
		continue;
	    }
	    else
	    {
		int i;
		for (i = 0; i < nb_rx; ++i)
		{
		    struct ether_hdr *eth = rte_pktmbuf_mtod(bufs[i], struct ether_hdr*);
		    JSLI(PValue, PJArray, mac_to_string(eth->s_addr.addr_bytes));
		    *PValue = bufs[i]->port;
		    //dump_mac_table();

		    printf("src_mac=%s dst_mac=%s port=%d pkt_len=%dB\n",
			    mac_to_string(eth->s_addr.addr_bytes),
			    mac_to_string(eth->d_addr.addr_bytes),
			    bufs[i]->port,
			    bufs[i]->pkt_len);
		}
	    }	

	    /* Send burst of TX packets, to second port of pair. */
	    const uint16_t nb_tx = rte_eth_tx_burst(port ^ 1, 0, bufs, nb_rx);

	    /* Free any unsent packets. */
	    if (unlikely(nb_tx < nb_rx)) {
		uint16_t buf;
		for (buf = nb_tx; buf < nb_rx; buf++)
		    rte_pktmbuf_free(bufs[buf]);
	    }
	}
    }

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


    //MAC address and ports table initialization
    PJArray = (PWord_t) NULL;


    //main forwarding loop
    forwarding_loop(num_port);

    rte_eal_mp_wait_lcore();
    return 0;
}
