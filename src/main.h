struct rte_mempool;

int forwarding_loop(void);
void init_mbufs(struct rte_mempool *mbuf_pool, int nb_ports);
