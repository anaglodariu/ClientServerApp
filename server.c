#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/timerfd.h>
#include <errno.h>
#include <math.h>

#include "common.h"
#include "helpers.h"

#define MAX_CLIENTS 1000
#define MAX_CONNECTIONS 10

void update_poll_fds(struct pollfd *poll_fds, int *current_clients, int i) {
  if (i == *current_clients - 1) {
    /* daca socketul inchis este ultimul din multimea de citire */
    (*current_clients)--;
    return;
  }
  /* scoatem din multimea de citire socketul inchis */
  for (int j = i; j < *current_clients - 1; j++) {
    poll_fds[j] = poll_fds[j + 1];
  }

  (*current_clients)--;
}

void update_conn_clients(struct info_tcp_client *connected_clients, int max_id, int socket) {
  for (int i = 0; i < max_id; i++) {
    if (connected_clients[i].socket == socket) {
      printf("Client %s disconnected.\n", connected_clients[i].id);
      connected_clients[i].socket = 0;
      memset(connected_clients[i].id, 0, sizeof(connected_clients[i].id));
      memset(&connected_clients[i].ip, 0, sizeof(connected_clients[i].ip));
      connected_clients[i].port = 0;
      return;
    }
  }
}

int check_socket(struct info_tcp_client *clients, int *max_id, int socket) {
  for (int i = 0; i < *max_id; i++) {
    if (clients[i].socket == socket && clients[i].socket != 0) {
      /* daca clientul nu e inca introdus in vector, campul socket
      va fi 0 (necompletat) */
      return 1; /* e deja conectat si introdus in clients */
    }
  }
  return 0;
}

/* functie care verifica ca id-ul este in vector */
int check_id(struct info_tcp_client **clients, int *max_id, struct info_tcp_client info_tcp, int socket) {
  for (int i = 0; i < *max_id; i++) {
    if (strcmp(info_tcp.id, (*clients)[i].id) == 0) {
      return 1; /* e deja conectat si introdus in clients */
    }
  }

  /* e recent conectat si nu e inca
  adaugat in vector, deci il adaugam pe 
  primul loc neocupat in vector */
  for (int i = 0; i < *max_id; i++) {
    if ((*clients)[i].socket == 0) {
      (*clients)[i] = info_tcp;
      (*clients)[i].socket = socket;
      return 0;
    }
  }

  /* nu mai e loc in vector, realocam */
  *clients = realloc(*clients, 2 * (*max_id) * sizeof(struct info_tcp_client));
  DIE(clients == NULL, "realloc");

  *max_id = 2 * (*max_id);
  /* adaugam in vector clientul nou conectat*/
  (*clients)[*max_id / 2] = info_tcp;
  (*clients)[*max_id / 2].socket = socket;

  return 0;
}


void run_server(int listenfd, int udpfd) {
  char buf[BUFLEN];
  memset(buf, 0, BUFLEN);


  // struct topicList* list = createList();
  // int len = 3; /* spatiu alocat pentru 3 elemente la inceput */

  struct info_tcp_client info_tcp;

  int current_clients = 3; /* la inceput : stdin, socket tcp si udp */
  int max_clients = 3; /* maxim max_clienti spatiu alocat pentru poll_fds */
  struct pollfd *poll_fds = malloc(max_clients * sizeof(struct pollfd));
  int rc;

  /* in vectorul connected_clients pastram info-ul clientilor tcp conectati la server*/
  int max_id = 3;
  struct info_tcp_client *connected_clients = calloc(max_id, sizeof(struct info_tcp_client));

  struct udp_packet recv_packet_udp;
  memset(&recv_packet_udp, 0, sizeof(struct udp_packet));

  /* setam socket-ul listenfd pentru ascultare */
  rc = listen(listenfd, MAX_CLIENTS);
  DIE(rc < 0, "listen");

  /* se adauga noul file descriptor (socketul pe care se asculta conexiuni) in
  multimea read_fds */
  poll_fds[2].fd = listenfd;
  poll_fds[2].events = POLLIN;

  /* se adauga un nou file descriptor (socketul pe care se primesc pachete de la
  clientii udp) in multimea read_fds */
  poll_fds[0].fd = udpfd;
  poll_fds[0].events = POLLIN;

  /* se adauga file descriptor-ul pentru stdin */
  poll_fds[1].fd = 0;
  poll_fds[1].events = POLLIN;

  while (1) {
    rc = poll(poll_fds, current_clients, 0);
    DIE(rc < 0, "poll");

    if (poll_fds[1].revents & POLLIN) {
      if (poll_fds[1].fd == 0) {
        /* s-au primit date de la stdin */
        fgets(buf, sizeof(buf), stdin);
        /* se verifica daca s-a citit un mesaj gol */
        if (isspace(buf[0])) {
          continue;
        }
        if (strncmp(buf, "exit", 4) == 0) {
          struct udp_packet send_packet;
          memset(&send_packet, 0, sizeof(struct udp_packet));
          memcpy(send_packet.payload, buf, strlen(buf));
          for (int i = 0; i < max_id; i++) {
            if (connected_clients[i].socket != 0) {
              send_all(connected_clients[i].socket, &send_packet, sizeof(send_packet));
              printf("Client %s disconnected.\n", connected_clients[i].id);
            }
          }
          free(poll_fds);
          free(connected_clients);
          close(listenfd);
          close(udpfd);
          return;
        }
      }
    }

    for (int i = 0; i < current_clients; i++) {
      if (poll_fds[i].revents & POLLIN) {
        if (poll_fds[i].fd == udpfd) {
          /* s-au primit date pe socketul udp*/
          struct sockaddr_in src_addr;
          socklen_t src_len = sizeof(src_addr);
          int rc = recvfrom(udpfd, &recv_packet_udp, sizeof(recv_packet_udp), 0,
                            (struct sockaddr *)&src_addr, &src_len);
          DIE(rc < 0, "recvfrom");
          recv_packet_udp.udp_ip = src_addr.sin_addr;
          recv_packet_udp.udp_port = ntohs(src_addr.sin_port);
          for (int i = 3; i < current_clients; i++) {
            // if (findTopic(list, poll_fds[i].fd, recv_packet_udp.topic) == 1)
            send_all(poll_fds[i].fd, &recv_packet_udp, sizeof(recv_packet_udp));
          }
        } else if (poll_fds[i].fd == listenfd) {
          /* a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
          pe care serverul o accepta */
          struct sockaddr_in client_addr;
          socklen_t client_len = sizeof(client_addr);
          int newsockfd = accept(listenfd, (struct sockaddr *)&client_addr, &client_len);
          DIE(newsockfd < 0, "accept");

          /* se adauga noul socket intors de accept() la multimea descriptorilor
          de citire */
          if (i + 1 == max_clients) {
            poll_fds = realloc(poll_fds, 2 * current_clients * sizeof(struct pollfd));
            DIE(poll_fds == NULL, "realloc");
            max_clients = 2 * current_clients;
          }
          poll_fds[current_clients].fd = newsockfd;
          poll_fds[current_clients].events = POLLIN;
          current_clients++;
        } else {
          /* s-au primit date pe unul din socketii de client,
          asa ca serverul trebuie sa le receptioneze */
          int ret1 = check_socket(connected_clients, &max_id, poll_fds[i].fd);
          if (ret1 == 0) {
            /* clientul este proaspat conectat (inca nu a fost adaugat la vectorul
            connected_clients) */
            /* fiind proaspat conectat, trebuie sa primim informatiile despre client
            (id, port, ip) */
            int rc = recv_all(poll_fds[i].fd, &info_tcp, sizeof(info_tcp));
            DIE(rc < 0, "recv");

            int ret2 = check_id(&connected_clients, &max_id, info_tcp, poll_fds[i].fd);
            if (ret2 == 0) {
              printf("New client %s connected from %s:%hu.\n", info_tcp.id,
                   inet_ntoa(info_tcp.ip), info_tcp.port);
            } else if (ret2 == 1) {
              struct udp_packet send_packet;
              memset(&send_packet, 0, sizeof(struct udp_packet));
              char *msg = "exit";
              memcpy(send_packet.payload, msg, strlen(msg));
              send_all(poll_fds[i].fd, &send_packet, sizeof(send_packet));
              update_poll_fds(poll_fds, &current_clients, i);
              printf("Client %s already connected.\n", info_tcp.id);
            }
          } else if (ret1 == 1) {
            /* clientul era deja conectat si primim date de la el pe server */
            struct tcp_message received_msg;
            memset(&received_msg, 0, sizeof(struct tcp_message));

            int rc = recv_all(poll_fds[i].fd, &received_msg, sizeof(received_msg));
            DIE(rc < 0, "recv");
            if (strncmp(received_msg.msg, "exit", 4) == 0) {
              update_conn_clients(connected_clients, max_id, poll_fds[i].fd);
              update_poll_fds(poll_fds, &current_clients, i);
              break;
            } else if (strncmp(received_msg.msg, "subscribe", 9) == 0) {
              // char *token = strtok(received_msg.msg, " ");
              // token = strtok(NULL, " ");
              // char topic[TOPIC_MAXSIZE];
              // strcpy(topic, token);
              // int rc = findTopic(list, poll_fds[i].fd, topic);
              /* COD NEIMPLEMENTAT (IMI DADEA TIMEOUT) */
              continue;
            } else if (strncmp(received_msg.msg, "unsubscribe", 11) == 0) {
              /* COD NEIMPLEMENTAT */
              continue;
            }
          }
        }
      }
    }
  }
}

int main(int argc, char *argv[]) {
  setvbuf(stdout, NULL, _IONBF, BUFSIZ);

  if (argc != 2) {
    printf("\n Usage: %s <port>\n", argv[0]);
    return 1;
  }

  /* parsam port-ul ca un numar */
  uint16_t port;
  int rc = sscanf(argv[1], "%hu", &port);
  DIE(rc != 1, "Given port is invalid");

  /* obtinem un socket TCP pentru receptionarea conexiunilor tcp */
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  DIE(listenfd < 0, "socket tcp");

  /* completăm in serv_addr adresa serverului, familia de adrese si portul
  pentru conectare */
  struct sockaddr_in serv_addr_tcp;
  socklen_t socket_len = sizeof(struct sockaddr_in);

  /* facem adresa socket-ului reutilizabila, ca sa nu primim eroare in caz ca
  rulam de 2 ori rapid */
  int enable = 1;
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed for tcp");
  
  /* dezactivam algoritmul lui Nagle */
  int no_delay = 1;
  if (setsockopt(listenfd, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(int)) < 0)
    perror("setsockopt(TCP_NODELAY) failed for tcp");

  memset(&serv_addr_tcp, 0, socket_len);
  serv_addr_tcp.sin_family = AF_INET;
  serv_addr_tcp.sin_addr.s_addr = INADDR_ANY;
  serv_addr_tcp.sin_port = htons(port);

  struct sockaddr_in serv_addr_udp;

  /* obtinem un socket UDP pentru receptionarea packetelor de la clientii udp */
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  DIE(sockfd < 0, "socket udp");

  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed for udp");

  memset(&serv_addr_udp, 0, socket_len);
  serv_addr_udp.sin_family = AF_INET; 
  serv_addr_udp.sin_addr.s_addr = INADDR_ANY;
  serv_addr_udp.sin_port = htons(port);

  /* asociem adresa serverului cu socketul creat folosind bind */
  rc = bind(listenfd, (const struct sockaddr *)&serv_addr_tcp, sizeof(serv_addr_tcp));
  DIE(rc < 0, "bind tcp failed");

  rc = bind(sockfd, (const struct sockaddr *)&serv_addr_udp, sizeof(serv_addr_udp));
  DIE(rc < 0, "bind udp failed");

  run_server(listenfd, sockfd);

  if (listenfd > 0)
    close(listenfd);
  if (sockfd > 0)
    close(sockfd);

  return 0;
}
