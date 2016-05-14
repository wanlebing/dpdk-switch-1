#pragma once

#include <stdint.h>
#include <rte_mbuf.h>

uint8_t* mac_to_string(uint8_t* bytes);
void print_packet(struct rte_mbuf* packet);
