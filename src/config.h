#pragma once

#include <inttypes.h>
#include "rte_virtio_net.h"

#define MBUF_ARRAY_SIZE 4096
#define MAX_PORTS_NUMBER 64

typedef enum port_type {
    NONE,
    PHY,
    VHOST
} port_type;

typedef struct port {
    port_type type;
    
    uint32_t index;
    uint16_t vlan_tag;
    uint8_t vlan_trunks[4096];

    struct rte_ring *ring_rx;
    struct rte_ring *ring_tx;

    struct mbuf_array *mbuf_tx;

    struct rte_mempool *mp;

    struct virtio_net *virtio_dev;

} port;

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

    struct rte_ring *rings_pre_tx[8];

    struct mbuf_array mbuf_rx;

    struct rte_mempool *mbuf_pool;
    
    uint8_t ports_counter;
    port ports[MAX_PORTS_NUMBER];
};
