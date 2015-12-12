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

int forwarding_loop(void)
{
	printf("Forwarding loop started\n");
	return 0;
}

void init_mbufs(struct rte_mempool *mbuf_pool, int nb_ports)
{
	/* Creates a new mempool in memory to hold the mbufs. */
	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports, MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");
}

int main(int argc, char **argv)
{
	int ret;

	printf("%s\n", argv[1]);

	//EAL initialization
	ret = rte_eal_init(argc, argv);
	if (ret < 0) rte_panic("Cannot init EAL\n");


	//MBUF initialziation
	struct rte_mempool *mbuf_pool = NULL;
	init_mbufs(mbuf_pool, 2);	



	//main forwarding loop
	forwarding_loop();

	rte_eal_mp_wait_lcore();
	return 0;
}
