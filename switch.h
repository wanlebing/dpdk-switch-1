#ifndef SWITCH_H
#define SWITCH_H

#include "list.h"

#include <rte_mempool.h>
#include <rte_mbuf.h>

#define RX_MBUF_SIZE 4096

typedef struct Switch {
    List* ports;
    struct rte_mempool* mp;
    struct rte_mbuf* mbuf_rx[RX_MBUF_SIZE];
    struct rte_mbuf* mbuf_pipeline[RX_MBUF_SIZE];
} Switch;

int switch_rx_loop(void* _s);
int switch_tx_loop(void* _s);
int switch_pipeline(void* _s);

void switch_run(Switch* s, int argc, char** argv);
void switch_init(Switch* s, int argc, char** argv);

extern Switch sw;

#endif /* SWITCH_H */
