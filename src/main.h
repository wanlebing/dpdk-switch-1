struct rte_mempool;

int rx_loop(__attribute__((unused)) void *);
int processing_loop(__attribute__((unused)) void *);
int fwd_loop(__attribute__((unused)) void *);
int tx_loop(__attribute__((unused)) void *arg);
void init_mbufs(void);
void init_rings(int);
void port_init(int port, struct rte_mempool *mbuf_pool);
void dump_mac_table(void);
