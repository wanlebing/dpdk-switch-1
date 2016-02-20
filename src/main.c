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
#include <rte_hash.h>

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
			/*for (m = 0; m < n_mbufs; ++m)
			{
				rte_pktmbuf_free(app.mbuf_rx.array[m]);
			}*/
		}
	}
	return 0;
}

int n_ports;

static inline void
insert_into_hash(uint8_t port, struct ether_addr key)
{
	//printf("Passed port: %d\n", port);

	if (lookup_struct != NULL)
	{
		int ret =  rte_hash_add_key(lookup_struct, (void*)&key);
		lookup_struct_ports[ret] = port;
		//RTE_LOG(DEBUG, USER1, "hash_add_key return val: %d\n", ret);
	}
}

static inline uint8_t
get_port_from_hash(struct ether_addr key)
{
	if (lookup_struct != NULL)
	{
		int ret = rte_hash_lookup(lookup_struct, (const void *)&key);
		//RTE_LOG(DEBUG, USER1, "hash_lookup return val: %d\n", ret);
		//printf("Lookup'd port = %d\n---------------\n", lookup_struct_ports[ret]);
		if (ret > 0) return lookup_struct_ports[ret];
		else return (uint8_t)255;
	}
	return (uint8_t)255;
}

static inline void
enqueue_packet_proc_tx(struct rte_mbuf* packet, uint8_t port, uint8_t src_port)
{
	uint8_t i;
	if (port != 255)
	{
		//rte_ring_sp_enqueue(app.rings_pre_tx[dst_port], (void**) processed_mbuf->array[m]);
		rte_ring_sp_enqueue(app.rings_pre_tx[port], (void**) packet);
	}
	else //flood
	{
		for (i = 0; i < 2; ++i) {
			if (i == src_port) continue;
			rte_ring_sp_enqueue(app.rings_pre_tx[app.ports[i]], (void**) packet);
		}

	}
}

int processing_loop(__attribute__((unused)) void *arg)
{
	struct mbuf_array *processed_mbuf;
	uint32_t i;
	int m;

	RTE_LOG(INFO, USER1, "Core %u is doing pipeline\n", rte_lcore_id());

	processed_mbuf = rte_malloc_socket(NULL, sizeof(struct mbuf_array), RTE_CACHE_LINE_SIZE, rte_socket_id());

	if (processed_mbuf == NULL)
		rte_panic("Worker thread: cannot allocate buffer space\n");

	int ret;

	while (1) {
		//VLAN logic on RX side
		for (i = 0; i < 2; ++i)
		{
			ret = rte_ring_sc_dequeue_burst(app.rings_rx[i], (void **) processed_mbuf->array, app.burst_size_worker_read);

			if (unlikely(!ret)) continue;

			//Enqueuing packets to QoS rings based on their VLAN ID and PCP value

			for (m = 0; m < ret; ++m)
			{
				struct ether_hdr *eth = rte_pktmbuf_mtod(processed_mbuf->array[m], struct ether_hdr*);

				//Register source MAC and port of packet to the flows table
				insert_into_hash(processed_mbuf->array[m]->port, eth->s_addr);
				uint8_t dst_port = get_port_from_hash(eth->d_addr);
				if (dst_port == 255) {

				}


				struct vlan_hdr* vlan = (struct vlan_hdr*)(eth + 1);

				if (eth->ether_type != rte_be_to_cpu_16(ETHER_TYPE_VLAN))
				{
					//Untagged frame ...
					if (app.vlan_tags[app.ports[i]])
					{
						//... has arrived on the tagged port. Need to tag this one.
						int status = rte_vlan_insert(&processed_mbuf->array[m]);
						eth = rte_pktmbuf_mtod(processed_mbuf->array[m], struct ether_hdr*);
						vlan = (struct vlan_hdr*)(eth + 1);
						vlan->vlan_tci = rte_cpu_to_be_16(app.vlan_tags[app.ports[i]]);
						vlan->vlan_tci |= rte_cpu_to_be_16(0x0000);
					}

					//uint16_t vlan_tag = rte_be_to_cpu_16(vlan->vlan_tci) & 0x0FFF;
					//printf("%d\n", vlan_tag);

					//Enqueue the frame to the correct pre_tx_queue according to the FIB TODO: replace (i + 1) % 2 with correct port, based on the FIB
					enqueue_packet_proc_tx(processed_mbuf->array[m], dst_port, processed_mbuf->array[m]->port);

				}
				else
				{
					//Frame is already tagged - need to perform VLAN trunking logic
					uint16_t vlan_tag = rte_be_to_cpu_16(vlan->vlan_tci) & 0x0FFF;
					uint16_t vlan_pcp = rte_be_to_cpu_16(vlan->vlan_tci) & 0xE000 >> 13;

					//printf("%d\n", vlan_tag);
					if (app.vlan_trunks[app.ports[i]][vlan_tag])
					{
						//Frame is allowed on this port - use priority from the PCP field.
						enqueue_packet_proc_tx(processed_mbuf->array[m], dst_port, processed_mbuf->array[m]->port);

					}
					//else drop frame
					else
					{
						rte_pktmbuf_free(processed_mbuf->array[m]);
					}

				}

			}
		}
/*
		for (m = 0; m < ret; ++m)
		{
			rte_pktmbuf_free(processed_mbuf->array[m]);
		}
*/
		//VLAN on TX side
		for (i = 0; i < 2; ++i)
		{
			ret = rte_ring_sc_dequeue_burst(app.rings_pre_tx[i], (void**) processed_mbuf->array, app.burst_size_worker_read);

			if (!ret) continue;
			//ret = rte_ring_sp_enqueue_burst(app.rings_tx[i], (void **) processed_mbuf->array, ret);

			for (m = 0; m < ret; ++m)
			{
				struct ether_hdr *eth = rte_pktmbuf_mtod(processed_mbuf->array[m], struct ether_hdr*);
				struct vlan_hdr* vlan = (struct vlan_hdr*)(eth + 1);

				if (eth->ether_type != rte_be_to_cpu_16(ETHER_TYPE_VLAN))
				{
					//Untagged frame ...
					if (!app.vlan_tags[app.ports[i]])
					{
						//... has to be sent on the untagged port - just enqueue
						rte_ring_sp_enqueue(app.rings_tx[i], (void **) processed_mbuf->array[m]);
					} //else  has to be sent on tagged port - drop
				}
				else
				{
					//tagged frame
					uint16_t vlan_tag = rte_be_to_cpu_16(vlan->vlan_tci) & 0x0FFF;

					if (!app.vlan_tags[app.ports[i]])
					{
						//... has to be sent on the untagged port - do VLAN trunking

						if (app.vlan_trunks[app.ports[i]][vlan_tag])
						{
							rte_ring_sp_enqueue(app.rings_tx[i], (void **) processed_mbuf->array[m]);
						}
					}
					else //... has to be sent on the tagged port - if tag matches - untag, else drop
					{
						if (vlan_tag == app.vlan_tags[app.ports[i]])
						{
							rte_vlan_strip(processed_mbuf->array[m]);
							rte_ring_sp_enqueue(app.rings_tx[i], (void **) processed_mbuf->array[m]);
						}
					}
				}
/*
				//TODO: Not needed
				for (m = 0; m < ret; ++m)
				{
					rte_pktmbuf_free(processed_mbuf->array[m]);
				}
*/
			}

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

	for (i = 0; ; ++i, i %= 2) {
		
		n_mbufs = app.mbuf_tx[i]->n_mbufs;

		ret = rte_ring_sc_dequeue_burst(app.rings_tx[i], (void **) app.mbuf_tx[i]->array, app.burst_size_tx_read);

		if (!ret) continue;

		n_mbufs = ret;

		if (n_mbufs < ret) {
			app.mbuf_tx[i]->n_mbufs = ret;
			continue;
		}

		n_pkts = rte_eth_tx_burst(app.ports[i], 0, app.mbuf_tx[i]->array, ret);

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
	/*FILE *fp;
	char readbuf[80];

	// Create the FIFO if it does not exist */
/*	umask(0);
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
*/



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

	init_hash();

	//init_vlan();

	set_port_vlan_trunk(app.ports[0], 6);
	set_port_vlan_tag(app.ports[1], 6);

	rte_eal_remote_launch(rx_loop, NULL, 1);
	rte_eal_remote_launch(processing_loop, NULL, 2);
	rte_eal_remote_launch(tx_loop, NULL, 3);


	//ctl_listener_loop(NULL);

//	rx_loop(NULL);

	rte_eal_mp_wait_lcore();
	return 0;
}
