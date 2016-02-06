#pragma once

struct rte_mempool;
struct app_config;
struct app_stats;

void init_mbufs(void);
void init_rings(int);
void port_init(int port, struct rte_mempool *mbuf_pool);
void init_app_config(void);

struct app_config app;
struct app_stats stats;
