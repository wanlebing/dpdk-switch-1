#pragma once

#include <inttypes.h>

struct rte_mempool;
struct app_config;
struct app_stats;

void init_mbufs(void);
void init_rings(int);
void port_init(int port, struct rte_mempool *mbuf_pool);
void init_app_config(void);
void init_vlan(void);
int set_port_vlan_tag(uint32_t port, uint16_t tag);

struct app_config app;
struct app_stats stats;
