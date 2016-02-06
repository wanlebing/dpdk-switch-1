#pragma once

struct rte_mempool;
struct app_config;

void init_mbufs(void);
void init_rings(int);
void port_init(int port, struct rte_mempool *mbuf_pool);
void init_app_config(void);

struct app_config app;
