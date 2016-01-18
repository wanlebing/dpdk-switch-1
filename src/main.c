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
#include <rte_malloc.h>

//Judy arrays
#include <Judy.h>

//for DPDK
#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 256

#define RX_RING_SIZE 4096
#define TX_RING_SIZE 4096

#define MBUF_ARRAY_SIZE 4096

#define BURST_SIZE 128

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


#define MAX_PORTS 4

uint32_t ports[MAX_PORTS];
struct mbuf_array {
	struct rte_mbuf *array[MBUF_ARRAY_SIZE];
	uint16_t n_mbufs;
};

struct mbuf_array mbuf_rx;

struct rte_ring *rings_rx[MAX_PORTS];
struct rte_ring *rings_qos[8];
struct rte_ring *rings_tx[MAX_PORTS];


const uint32_t burst_size_rx_read = 128;

int rx_loop(__attribute__((unused)) void *arg)
{
	uint32_t i;
	//int ret;

	RTE_LOG(INFO, USER1, "Core %u is doing RX\n", rte_lcore_id());

	while (1) {
		for (i = 0; i < 3; ++i) {
			uint16_t n_mbufs;
			

			n_mbufs = rte_eth_rx_burst(
				ports[i],
				0,
				mbuf_rx.array,
				burst_size_rx_read);

			if (n_mbufs == 0)
				continue;

	//		printf("RX: %d on port %d\n", n_mbufs, i);

			//do {
			rte_ring_sp_enqueue_burst(
				rings_rx[i],
				(void **) mbuf_rx.array,
				n_mbufs);
			//} while (ret < 0);
		}
	}
	return 0;
}

int n_ports;

int burst_size_worker_write = 128;
int burst_size_worker_read = 128;

int processing_loop(__attribute__((unused)) void *arg)
{
	struct mbuf_array *processed_mbuf;
	uint32_t i;

	//uint32_t target;

	RTE_LOG(INFO, USER1, "Core %u is doing work (no pipeline)\n", rte_lcore_id());

	processed_mbuf = rte_malloc_socket(NULL, sizeof(struct mbuf_array), RTE_CACHE_LINE_SIZE, rte_socket_id());
	if (processed_mbuf == NULL)
		rte_panic("Worker thread: cannot allocate buffer space\n");

	int ret;

	while (1) {
		for (i = 0; i < 3; ++i) {
/*
			switch(i) {
				case 0: target = 1; break;
				case 1: target = 0; break;
				case 2: target = 2; break;
				default: break;
			}
*/
			ret = rte_ring_sc_dequeue_burst(rings_rx[i], (void **) processed_mbuf->array, burst_size_worker_read);

			if (unlikely(!ret)) continue;

			//RTE_LOG(INFO, USER1, "PIPELINE: Dequeued packets\n");
			/**	TODO:
			 *	MAC table	
			**/


			//PIPELINE 1 - QoS
			//Enqueuing packets to QoS rings based on their PCP value
			int m;
			for (m = 0; m < ret; ++m)
			{
				struct rte_mbuf* packet = processed_mbuf->array[m];
				struct ether_hdr *eth = rte_pktmbuf_mtod(processed_mbuf->array[m], struct ether_hdr*);
				
				if (unlikely(eth->ether_type == 0x81))
				{
					//check PCP and enqueue
					 printf("VLAN\n");
				}
				else //use priority 1 (normal)
				{
					printf("Not VLAN\n");
					
				}

			}

			rte_ring_sp_enqueue_burst(rings_tx[(i + 1) % 2], (void **) processed_mbuf->array, ret);
	
			//rte_pktmbuf_free(*processed_mbuf->array);

			//RTE_LOG(INFO, USER1, "PIPELINE: %d packets enqueued for TX on port %u\n", ret, target);
		}
	}
	return 0;
}

struct mbuf_array *mbuf_tx[MAX_PORTS];
int burst_size_tx_read = 128;
int burst_size_tx_write = 128;


int tx_loop(__attribute__((unused)) void *arg)
{
	uint32_t i;

	RTE_LOG(INFO, USER1, "Core %u is doing TX\n", rte_lcore_id());

	uint16_t n_mbufs, n_pkts;
	int ret;


	for (i = 0; ; ++i) {
		
		i %= 3;


		n_mbufs = mbuf_tx[i]->n_mbufs;

		ret = rte_ring_sc_dequeue_burst(
			rings_tx[i],
			(void **) mbuf_tx[i]->array,//[n_mbufs],
			burst_size_tx_read);

		if (ret == 0)
			continue;

	//	printf("TX: Dequeued\n");

		n_mbufs = ret;

		if (n_mbufs < ret) {
		mbuf_tx[i]->n_mbufs = ret;
			continue;
		}

		n_pkts = rte_eth_tx_burst(
			ports[i],
			0,
			mbuf_tx[i]->array,
			ret);

		if (n_pkts < n_mbufs) {
			uint16_t k;

			for (k = n_pkts; k < n_mbufs; k++) {
				struct rte_mbuf *pkt_to_free;

				pkt_to_free = mbuf_tx[i]->array[k];
				rte_pktmbuf_free(pkt_to_free);
			}
		}

		mbuf_tx[i]->n_mbufs = 0;
	}
	return 0;
}

void init_mbufs(void)
{
	return;
}

void port_init(int port, struct rte_mempool *mbuf_pool)
{
    //default port config
    struct rte_eth_conf port_conf =  {
		.rxmode = { 
			.max_rx_pkt_len = ETHER_MAX_LEN
		}
    };

    ports[port] = port;

    mbuf_tx[port] = rte_malloc_socket(NULL, sizeof(struct mbuf_array), RTE_CACHE_LINE_SIZE, rte_socket_id());

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


int ring_rx_size = 2048;
int ring_tx_size = 2048;

void init_rings(int n_ports)
{
	int i;

	for (i = 0; i < n_ports; i++) {
		char name[32];

		snprintf(name, sizeof(name), "ring_rx_%u", i);

		rings_rx[i] = rte_ring_create(
			name,
			ring_rx_size,
			rte_socket_id(),
			RING_F_SP_ENQ | RING_F_SC_DEQ);

		if (rings_rx[i] == NULL)
			rte_panic("Cannot create RX ring %u\n", i);
	}

	for (i = 0; i < n_ports; i++) {
		char name[32];

		snprintf(name, sizeof(name), "ring_tx_%u", i);

		rings_tx[i] = rte_ring_create(
			name,
			ring_tx_size,
			rte_socket_id(),
			RING_F_SP_ENQ | RING_F_SC_DEQ);

		if (rings_tx[i] == NULL)
			rte_panic("Cannot create TX ring %u\n", i);
	}
	
	for (i = 0; i < 8; i++) {
		char name[32];

		snprintf(name, sizeof(name), "ring_qos_%u", i);

		rings_qos[i] = rte_ring_create(
			name,
			ring_tx_size,
			rte_socket_id(),
			RING_F_SP_ENQ | RING_F_SC_DEQ);

		if (rings_qos[i] == NULL)
			rte_panic("Cannot create QoS ring %u\n", i);
	}

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

    n_ports = num_port;

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

    init_mbufs();
    init_rings(num_port);

    rte_eal_remote_launch(rx_loop, NULL, 1);
	rte_eal_remote_launch(processing_loop, NULL, 2);
	rte_eal_remote_launch(tx_loop, NULL, 3);

    rte_eal_mp_wait_lcore();
    return 0;
}
