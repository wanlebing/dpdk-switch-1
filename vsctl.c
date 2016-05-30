#include "control.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    int sockfd, portno, n;

    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];

    portno = COMM_PORT;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    server = gethostbyname("localhost");

    bzero((char *) &serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;

    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);

    serv_addr.sin_port = htons(portno);

    connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr));

    bzero(buffer,256);

    ControlMessage msg;

    if (strcmp(argv[1], "set") == 0) {
        if (strcmp(argv[3], "tag") == 0) {
            msg.code = SET_VLAN;
            msg.tag = atoi(argv[4]);
        } else if (strcmp(argv[3], "trunk") == 0) {
            msg.code = SET_TRUNK;
            msg.tag = atoi(argv[4]);
        } else {
            (void)0;
        }
    } else if (strcmp(argv[1], "unset") == 0) {
        if (strcmp(argv[3], "tag") == 0) {
            msg.code = UNSET_VLAN;
            msg.tag = atoi(argv[4]);
        } else if (strcmp(argv[3], "trunk") == 0) {
            msg.code = UNSET_TRUNK;
            msg.tag = atoi(argv[4]);
        } else {
            (void)0;
        }
    } else {
        (void)0;
    }

    strcpy(msg.port_name, argv[2]);
    memcpy(buffer, &msg, 256);

    n = write(sockfd,buffer,256);

    bzero(buffer,256);
    n = read(sockfd,buffer,255);

    printf("%s\n",buffer);
    return 0;
}
