#include "utils.h"
#include <stdio.h>
#include <stdint.h>

uint8_t* mac_to_string(uint8_t* bytes)
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
