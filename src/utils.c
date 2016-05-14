#include "utils.h"
#include <stdio.h>
#include <stdint.h>

#include <rte_ether.h>
#include <rte_hexdump.h>
#include <rte_mbuf.h>

uint8_t*
mac_to_string(uint8_t* bytes)
{

	static uint8_t mac[17];
	sprintf((char*) mac, "%02x:%02x:%02x:%02x:%02x:%02x",
		(unsigned char) bytes[0],
		(unsigned char) bytes[1],
		(unsigned char) bytes[2],
		(unsigned char) bytes[3],
		(unsigned char) bytes[4],
		(unsigned char) bytes[5]);

	return mac;
}

void
print_packet(struct rte_mbuf* packet)
{
    struct eth_hdr* l2 = rte_pktmbuf_mtod(packet, struct eth_hdr*);
    rte_hexdump(stdout, "PACKET", l2, 18);
}
