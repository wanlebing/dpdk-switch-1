#include "switch.h"
#include "port.h"
#include "list.h"
#include "actions.c"
#include "control.h"

#include "murmurhash.h"

#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_debug.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_ether.h>

#define NUM_MBUFS 32768
#define MBUF_CACHE_SIZE 0

#define BURST_RX_SIZE 512
#define BURST_TX_SIZE 512

#define VHOST_RETRY_NUM 8

#define clear() printf("\033[H\033[J")
#define noop (void)0

Switch sw;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

bool static inline is_vlan(struct rte_mbuf* packet) {
    struct ether_hdr* eth = rte_pktmbuf_mtod(packet, struct ether_hdr*);
    return (rte_cpu_to_be_16(eth->ether_type) == ETHER_TYPE_VLAN);
}

uint16_t static inline get_vlan_tag(struct rte_mbuf* packet) {
    struct ether_hdr* eth = rte_pktmbuf_mtod(packet, struct ether_hdr*);
    struct vlan_hdr* vlan = (struct vlan_hdr *) (eth + 1);
    return (rte_be_to_cpu_16(vlan->vlan_tci) & 0x0FFF);
}

Port* switch_lookup_port(Switch* s, const char* name) {
    Port* result = NULL;
    for (Node* n = s->ports->head; n != NULL; n=n->next) {
        Port* current = (Port*) n->value;
        if (strcmp(current->name, name)) {
            result = current;
            break;
        }
    }
    return result;
}

void static inline switch_process_vlan(Port* port, struct rte_mbuf** mbuf, int n) {
    int tag;
    if ((tag = port_get_vlan_tag(port))) {
        for (int i = 0; i < n; ++i) {
            if (!is_vlan(mbuf[i])) action_push_vlan(mbuf[i], tag);
            else if (tag == get_vlan_tag(mbuf[i])) action_pop_vlan(mbuf[i]);
            else if (port_is_vlan_trunk(port, get_vlan_tag(mbuf[i]))) noop;
            else action_drop(mbuf[i]);
        }
    }
}


static inline Port* switch_lookup_hash(struct rte_mbuf* packet, Switch* s) {
    Port* dst_port = NULL;
    if (packet == NULL) {
        goto lookup_end;
    }
    struct ether_hdr* eth = rte_pktmbuf_mtod(packet, struct ether_hdr*);
    struct ether_addr key = eth->d_addr;

    if (is_multicast_ether_addr(&key) || is_broadcast_ether_addr(&key)) goto lookup_end;

    int ret = rte_hash_lookup(s->hashmap, (const void *)&key);
    switch (ret) {
      case -EINVAL:
      case -ENOENT:
        dst_port = NULL;
        break;
      default:
        dst_port = s->hashmap_ports[ret];
        break;
    }

lookup_end:
    return dst_port;
}


int switch_rx_loop(void* _s) {
  Switch* s = (Switch*) _s;

  while (1) {
    Node* current = s->ports->head;
    while (current != NULL) {
      Port* p = (Port*) current->value;

      int received = 0;
      /* Port RX action */
      switch(p->type) {
        case PHY:
          received = rte_eth_rx_burst(p->id, 0, p->mbuf_rx, BURST_RX_SIZE);
          break;
        case VHOST:
          if (port_is_virtio_dev_runnning(p)) {
            int cnt = 0;
            int retries = 0;

//            do {
              received = rte_vhost_dequeue_burst(p->virtio_dev, 0 * VIRTIO_QNUM + VIRTIO_TXQ,
                                            s->mp, p->mbuf_rx, BURST_RX_SIZE);
  //            if (cnt > 0) received += cnt;
  //              else break;
  //          } while (cnt > 0 && (retries++ < VHOST_RETRY_NUM));
          } else goto next_port;
          break;
        }

        if (received < 1) {
          received = 0;
          goto next_port;
        }
/*
        pthread_mutex_lock(&mutex);
        for (int i = 0; i < received; ++i) {
          port_update_rx_stats(p, 1, s->mbuf_rx[i]->pkt_len, 0);
        }
        pthread_mutex_unlock(&mutex);
*/
        /* VLAN tagging/untagging on RX side */
      //  switch_process_vlan(p, s->mbuf_rx, received);

        while (received > 0) {
          int enq = rte_ring_sp_enqueue_burst(p->ring_rx, (void**) p->mbuf_rx, received);
//        port_update_rx_stats(p, 0, 0, received-enq);
          received -= enq;
        }

next_port:
        current = current->next;
      }

      for (Node* n = s->ports->head; n != NULL; n=n->next) {
        int received = 0;
        Port* p = (Port*)n->value;

        if (p->ring_rx != NULL) {
          received = rte_ring_sc_dequeue_burst(p->ring_rx, (void**) s->mbuf_pipeline, 256);
        }

        if (received < 1) continue;

        for (int i = 0; i < received; ++i) {
          //action_print(s->mbuf_pipeline[i]);
          action_learn(s->mbuf_pipeline[i], s, p);
          Port* dst_port = switch_lookup_hash(s->mbuf_pipeline[i], s);
          if (dst_port != NULL) action_output(s->mbuf_pipeline[i], dst_port);
          else action_flood(s->mbuf_pipeline[i], s, p);
        }
      }
//}
//    while (1) {
        //Node*
      current = s->ports->head;
      while (current != NULL) {
        Port* p = (Port*) current->value;
        if (p->ring_tx == NULL) goto tx_next_port;

          int n = p->mbuf_tx_counter;

          int cnt = rte_ring_dequeue_burst(p->ring_tx, p->mbuf_tx, 256);

          if (cnt <= 0) goto tx_next_port;

          p->mbuf_tx_counter = cnt;

          /* Port TX action */
          switch (p->type) {
            case PHY:
              rte_eth_tx_burst(p->id, 0, p->mbuf_tx, p->mbuf_tx_counter);
              break;
            case VHOST:
              if (port_is_virtio_dev_runnning(p)) {
                /* VLAN tagging/untagging on TX side */
        //        switch_process_vlan(p, p->mbuf_tx, p->mbuf_tx_counter);
                struct rte_mbuf** pkts = p->mbuf_tx;
                int retries = 0;
                do {
                  int vhost_qid = 0 * VIRTIO_QNUM + VIRTIO_RXQ;
                  unsigned int tx_pkts;

                  tx_pkts = rte_vhost_enqueue_burst(p->virtio_dev, vhost_qid,
                                                    p->mbuf_tx, cnt);
                  if (likely(tx_pkts)) {
                    /* Packets have been sent.*/
                    cnt -= tx_pkts;
                    /* Prepare for possible retry.*/
                    pkts = &pkts[tx_pkts];
                  } else {
                    /* No packets sent - do not retry.*/
                    break;
                  }
                } while (cnt && (retries++ < VHOST_RETRY_NUM));

                for (int i = 0; i < p->mbuf_tx_counter; i++) {
                 if (p->mbuf_tx[i] != NULL) rte_pktmbuf_free(p->mbuf_tx[i]);
               }
              }
              break;
            }
          tx_next_port:
            current = current->next;
          }
        }

        return 0;
}

int switch_tx_loop(void* _s) {
    Switch* s = (Switch*) _s;
    return 0;
}

int switch_pipeline(void* _s) {
    Switch* s = (Switch*) _s;
    int received;
    return 0;
}

/* Stats printing thread */
void* switch_print_stats_loop(void *_s)
{
  Switch* s = (Switch*) _s;
  while (1) {
      for (Node* n = s->ports->head; n != NULL; n=n->next) {
          Port* p = (Port*)n->value;
          port_print_stats(p);
      }
      sleep(1);
      clear();
  }

  /* the function must return something - NULL will do */
  return NULL;
}

/* Stats printing thread */
void* switch_control_loop(void *_s)
{
    Switch* s = (Switch*) _s;

    int sockfd, newsockfd, portno, clilen;

    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero((char *) &serv_addr, sizeof(serv_addr));

    portno = COMM_PORT;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));

    while (1) {
        listen(sockfd,5);

        clilen = sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

        bzero(buffer,256);
        n = read(newsockfd,buffer,255);

        ControlMessage* msg = (ControlMessage*) buffer;
        const char* response = control_process_message(s, msg);
        printf("%s\n", response);

        n = write(newsockfd, response, strlen(response));
    }
    /* the function must return something - NULL will do */
    return NULL;
}

void switch_run(Switch* s, int argc, char** argv) {
    switch_init(s, argc, argv);

    /* Launch all loops on separate cores */
    rte_eal_remote_launch(switch_rx_loop, (void*) s, 1);
    rte_eal_remote_launch(switch_pipeline, (void*) s, 2);
    rte_eal_remote_launch(switch_tx_loop, (void*) s, 3);

    /* Launch other threads on master core */
    pthread_t stats_thread, control_thread;
    //pthread_create(&stats_thread, NULL, switch_print_stats_loop, s);
    pthread_create(&control_thread, NULL, switch_control_loop, s);

    /* Start vhost session */
    rte_vhost_driver_session_start();

    rte_eal_mp_wait_lcore();
}

void switch_init(Switch* s, int argc, char** argv) {
    /* Initialize EAL */
    int ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        rte_panic("Cannot init EAL\n");
    }

    /* Initialize port list */
    s->ports = list_init();

    int phy_n = rte_eth_dev_count();
    int vhost_n = 2;

    /* Initialize physical ports and mbuf pool */
    s->mp = rte_pktmbuf_pool_create("MBUF_POOL", (phy_n + vhost_n) * 2 * NUM_MBUFS, MBUF_CACHE_SIZE, 0,
                                    RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

    switch_init_hash(s);

    for (int i = 0; i < phy_n; ++i) {
        Port* p;
        p = port_init_phy(i, s->mp);
        list_insert(s->ports, p);
    }

    /* Initialize vhost ports */
    for (int i = 0; i < vhost_n; ++i) {
        Port* p = port_init_vhost(i, s->mp);
        port_set_vlan_tag(p, 5);
        list_insert(s->ports, p);
    }
}

static inline uint32_t hash(const void *data, __attribute__((unused)) uint32_t data_len, uint32_t init_val)
{
    return MurmurHash2(data, sizeof(struct ether_addr), init_val);
}

void switch_init_hash(Switch* s) {
    int socketid = 0; //CPU socket (non-NUMA => socketid 0)

    struct rte_hash_parameters hash_params = {
        .name = NULL,
        .entries = 1024,
        .key_len = sizeof(struct ether_addr),
        .hash_func = hash,
        .hash_func_init_val = 1,
    };

    char name[64];
    /* create ipv4 hash */
    snprintf(name, sizeof(name), "hash_%d", socketid);
    hash_params.name = name;
    hash_params.socket_id = socketid;
    s->hashmap = rte_hash_create(&hash_params);

    if (s->hashmap == NULL) rte_exit(EXIT_FAILURE, "Unable to create the hashmap");
}
