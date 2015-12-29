#include "utils.h"
#include <stdio.h>
#include <stdint.h>

char* mac_to_string(uint8_t* bytes)
{

	static char mac[17];
	sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x",
		(unsigned char) bytes[0],
		(unsigned char) bytes[1],
		(unsigned char) bytes[2],
		(unsigned char) bytes[3],
		(unsigned char) bytes[4],
		(unsigned char) bytes[5]);

	return mac;
} 
