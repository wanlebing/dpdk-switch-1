#pragma once

#include <inttypes.h>

#define MBUF_ARRAY_SIZE 4096

struct mbuf_array {
	struct rte_mbuf *array[MBUF_ARRAY_SIZE];
	uint16_t n_mbufs;
};

struct app_config {
	int burst_size_worker_write;
	int burst_size_worker_read;
	int burst_size_tx_read;
	int burst_size_tx_write;
	int burst_size_rx_read;
	int burst_size_rx_write;


	int ring_rx_size;
	int ring_tx_size;

	struct rte_ring *rings_rx[32];
	struct rte_ring *rings_pre_tx[8];
	struct rte_ring *rings_tx[32];

	struct mbuf_array *mbuf_tx[32];
	struct mbuf_array mbuf_rx;

	uint32_t ports[32];

	uint16_t vlan_tags[32];
	uint8_t vlan_trunks[32][4096];

};
