#pragma once

int rx_loop(__attribute__((unused)) void *);
int processing_loop(__attribute__((unused)) void *);
int stats_print_loop(__attribute__((unused)) void *);
int fwd_loop(__attribute__((unused)) void *);
int tx_loop(__attribute__((unused)) void *arg);
int ctl_listener_loop(__attribute__((unused)) void *arg);
void dump_mac_table(void);
