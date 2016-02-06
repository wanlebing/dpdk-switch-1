#include "main.h"
#include "utils.h"
#include "init.h"
#include "config.h"
#include "stats.h"

//C
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/stat.h>
#include <linux/stat.h>

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

//for DPDK
#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 256

int rx_loop(__attribute__((unused)) void *arg)
{
	uint32_t i;
	uint16_t n_mbufs;

	RTE_LOG(INFO, USER1, "Core %u is doing RX\n", rte_lcore_id());

	while (1) {
		for (i = 0; i < 2; ++i) {
			
			n_mbufs = rte_eth_rx_burst(app.ports[i], 0, app.mbuf_rx.array, app.burst_size_rx_read);
			stats.rx_packets[app.ports[i]] += n_mbufs;

			if (!n_mbufs) continue;

			rte_ring_sp_enqueue_burst(app.rings_rx[i], (void **) app.mbuf_rx.array, n_mbufs);

			int m;
			for (m = 0; m < n_mbufs; ++m)
			{
				rte_pktmbuf_free(app.mbuf_rx.array[m]);
			}
		}
	}
	return 0;
}

int n_ports;



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
		for (i = 0; i < 2; ++i) {
/*
			switch(i) {
				case 0: target = 1; break;
				case 1: target = 0; break;
				case 2: target = 2; break;
				default: break;
			}
*/
			ret = rte_ring_sc_dequeue_burst(app.rings_rx[i], (void **) processed_mbuf->array, app.burst_size_worker_read);

			if (unlikely(!ret)) continue;



			//RTE_LOG(INFO, USER1, "PIPELINE: Dequeued packets\n");
			/**	TODO:
			 *	MAC table	
			**/


			//PIPELINE 1 - QoS+VLAN
			//Enqueuing packets to QoS rings based on their VLAN ID and PCP value
			int m;
			for (m = 0; m < ret; ++m)
			{
				struct rte_mbuf* packet = processed_mbuf->array[m];
				struct ether_hdr *eth = rte_pktmbuf_mtod(processed_mbuf->array[m], struct ether_hdr*);
				
				if (unlikely(eth->ether_type == 0x81))
				{
					//check PCP and enqueue
//					printf("VLAN\n");
					struct vlan_hdr* vlan = (struct vlan_hdr *)(eth + 1);
					//printf("VLAN: ID=%x  PCP=%d\n", vlan->vlan_tci & 0xFFFF, (vlan->vlan_tci & 0x00E0) >> 5);
					rte_ring_sp_enqueue(app.rings_qos[(vlan->vlan_tci & 0x00E0) >> 5], packet);
				}
				else //use priority 1 (normal)
				{
					//printf("Not VLAN\n");
					rte_ring_sp_enqueue(app.rings_qos[1], (void**) processed_mbuf->array[m]);
					
				}

			}

			int q, total = 0;
			ret = 0;
			for (q = 7; q >= 0; --q)
			{
				ret = rte_ring_sc_dequeue_burst(app.rings_qos[q], (void**) processed_mbuf->array, app.burst_size_worker_read);
				total += ret;
				rte_ring_sp_enqueue_burst(app.rings_tx[(i + 1) % 2], (void **) processed_mbuf->array, ret);
			}

			for (m = 0; m < total; ++m)
			{	
				rte_pktmbuf_free(processed_mbuf->array[m]);
			}

			//RTE_LOG(INFO, USER1, "PIPELINE: %d packets enqueued for TX\n", total);
		}
	}
	return 0;
}

int tx_loop(__attribute__((unused)) void *arg)
{
	uint32_t i;

	RTE_LOG(INFO, USER1, "Core %u is doing TX\n", rte_lcore_id());

	uint16_t n_mbufs, n_pkts;
	int ret;


	for (i = 0; ; ++i) {
		
		i %= 2;

		n_mbufs = app.mbuf_tx[i]->n_mbufs;

		ret = rte_ring_sc_dequeue_burst(
			app.rings_tx[i],
			(void **) app.mbuf_tx[i]->array,//[n_mbufs],
			app.burst_size_tx_read);

		if (ret == 0)
			continue;

	//	printf("TX: Dequeued\n");
	//
	//
	/*	
		int m;
		for (m = 0; m < ret; ++m)
		{
			struct rte_mbuf* packet = mbuf_tx[i]->array[m];
			struct ether_hdr *eth = rte_pktmbuf_mtod(mbuf_tx[i]->array[m], struct ether_hdr*);
			
			if (unlikely(eth->ether_type == 0x81))
			{
				//check PCP and enqueue
//					 printf("VLAN\n");
				 struct vlan_hdr* vlan = (struct vlan_hdr *)(eth + 1);
				 //printf("VLAN: ID=%x  PCP=%d\n", vlan->vlan_tci & 0xFFFF, (vlan->vlan_tci & 0x00E0) >> 5);
				 rte_ring_sp_enqueue(rings_qos[(vlan->vlan_tci & 0x00E0) >> 5], packet);
			}
			else //use priority 1 (normal)
			{
				//printf("Not VLAN\n");
				rte_ring_sp_enqueue(rings_qos[1], packet);
				
			}

		}
*/
		n_mbufs = ret;

		if (n_mbufs < ret) {
			app.mbuf_tx[i]->n_mbufs = ret;
			continue;
		}

		n_pkts = rte_eth_tx_burst(
			app.ports[i],
			0,
			app.mbuf_tx[i]->array,
			ret);

		stats.tx_packets[app.ports[i]] += n_pkts;

		if (n_pkts < n_mbufs) {
			uint16_t k;

			for (k = n_pkts; k < n_mbufs; k++) {
				struct rte_mbuf *pkt_to_free;

				pkt_to_free = app.mbuf_tx[i]->array[k];
				rte_pktmbuf_free(pkt_to_free);
			}
		}

		app.mbuf_tx[i]->n_mbufs = 0;
	}
	return 0;
}

int stats_print_loop(__attribute__((unused)) void *arg)
{
	while(1)
	{
		sleep(1);
		int i;
		for (i = 0; i < 2; ++i) {
			printf("Port %d:\nrx_packets=%lld tx_packets=%lld\n", i, stats.rx_packets[app.ports[i]], stats.tx_packets[app.ports[i]]);
		}
	}

	return 0;
}

#define FIFO_FILE "dpswitch_ctl"

int ctl_listener_loop(__attribute__((unused)) void *arg)
{
	FILE *fp;
	char readbuf[80];

	/* Create the FIFO if it does not exist */
	umask(0);
	mknod(FIFO_FILE, S_IFIFO|0666, 0);

	int i;

	while(1)
	{
			fp = fopen(FIFO_FILE, "r");
			fgets(readbuf, 80, fp);

			fclose(fp);

			if((fp = fopen(FIFO_FILE, "w")) == NULL) {
				perror("fopen");
				exit(1);
			}

			if (strcmp("stats",readbuf) == 0) {
				for (i = 0; i < 2; ++i)
					fprintf(fp, "PORT %d:\n\trx_packets=%lld\n\ttx_packets=%lld\n", i, stats.rx_packets[app.ports[i]], stats.tx_packets[app.ports[i]]);
			}
			else {
				fputs("Unrecognized request", fp);
			}



			fclose(fp);
	}




	return 0;
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

	//MBUF initialization
	struct rte_mempool *mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", num_mbufs * num_port, MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	init_app_config();

	//ports initialization
	int i;
	for (i = 0; i < num_port; ++i)
	{
		port_init(i, mbuf_pool);
	}


	//MAC address and ports table initialization
	//PJArray = (PWord_t) NULL;

	init_mbufs();
	init_rings(num_port);

	rte_eal_remote_launch(rx_loop, NULL, 1);
	rte_eal_remote_launch(processing_loop, NULL, 2);
	rte_eal_remote_launch(tx_loop, NULL, 3);
	ctl_listener_loop(NULL);

//	rx_loop(NULL);

	rte_eal_mp_wait_lcore();
	return 0;
}
