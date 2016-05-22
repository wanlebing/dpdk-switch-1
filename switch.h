#ifndef SWITCH_H
#define SWITCH_H

#include "list.h"

#include <rte_mempool.h>

typedef struct Switch {
    List* ports;
    struct rte_mempool* mp;
} Switch;

int switch_rx_loop(void* s);
int switch_tx_loop(void* s);
int switch_pipeline(void* s);

void switch_run(Switch* s, int argc, char** argv);
void switch_init(Switch* s, int argc, char** argv);

#endif /* SWITCH_H */
