#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <stdint.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include "helpers.h"

int send_all(int sockfd, void *buff, size_t len);
int recv_all(int sockfd, void *buff, size_t len);

#define MSG_MAXSIZE 65 // 50 + 11 + 3 + 1 (unsubscribe + topic + SF)
#define BUFLEN 1500
#define TOPIC_MAXSIZE 50

struct tcp_message {
  /* in msg voi retine mesajele 
  de tipul exit, subscribe ..., unsubscribe ...
  primite de catre server de la clienti */
  char msg[MSG_MAXSIZE + 1];
};

struct udp_packet {
  char topic[TOPIC_MAXSIZE];
  uint8_t type;
  char payload[BUFLEN];
  in_port_t udp_port;       // pentru cand transmitem 
  struct in_addr udp_ip;  // pachetul catre clientii tcp (afisare)
};

/* elemente in vectorul socket - topicuri */
struct topicList {
  char topic[TOPIC_MAXSIZE];
  struct topicList* next;
};

struct info_tcp_client {
  char id[11]; /* id unic client tcp (cat sa incapa un int)*/
  struct in_addr ip; /* adresa ip server la care se conecteaza clientul */
  uint16_t port; /* port server la care se conecteaza clientul */
  int socket; /* socket pentru conexiunea client - server */
};

// void delete(struct topicList*, int socket, char* topic);
void insert(struct topicList*, int* len, char* topic, int socket);
struct topicList* createList();
int findTopic(struct topicList*, int socket, char* topic);

#endif /* __COMMON_H__ */
