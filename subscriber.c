#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

#include "common.h"
#include "helpers.h"

#define NUM_CONEXIONS 2


/* functii pentru formatarea tipului de payload */
void format0_payload(struct udp_packet recv_packet) {
  /* the sign of the value is in payload[0] (0/1) */
  /* the rest of the value itself is from payload + 1*/
  uint32_t payload_int = ntohl(*(uint32_t *)(recv_packet.payload + 1));
  if (recv_packet.payload[0] == 0) {
    /* the number is positive */
    printf("%s:%hu - %s - INT - %d\n", inet_ntoa(recv_packet.udp_ip), 
      recv_packet.udp_port, recv_packet.topic, payload_int);
  } else {
    /* the number is negative */
    printf("%s:%hu - %s - INT - -%d\n", inet_ntoa(recv_packet.udp_ip), 
      recv_packet.udp_port, recv_packet.topic, payload_int);
  }
}

void format1_payload(struct udp_packet recv_packet) {
  uint16_t payload_short_real = ntohs(*(uint16_t *)recv_packet.payload);
  printf("%s:%hu - %s - SHORT_REAL - %0.2f\n", inet_ntoa(recv_packet.udp_ip), 
      recv_packet.udp_port, recv_packet.topic, (float)(payload_short_real) / 100);
}

void format2_payload(struct udp_packet recv_packet) {
  uint32_t payload_int = ntohl(*(uint32_t *)(recv_packet.payload + 1));
  uint8_t power = *(uint8_t *)(recv_packet.payload + 5);
  if (recv_packet.payload[0] == 0) {
    /* the number is positive */
    printf("%s:%hu - %s - FLOAT - %f\n", inet_ntoa(recv_packet.udp_ip), 
      recv_packet.udp_port, recv_packet.topic, payload_int / pow(10, power));
  } else {
    /* the number is negative */
    printf("%s:%hu - %s - FLOAT - -%f\n", inet_ntoa(recv_packet.udp_ip), 
      recv_packet.udp_port, recv_packet.topic, payload_int / pow(10, power));
  }
}

void format3_payload(struct udp_packet recv_packet) {
  printf("%s:%hu - %s - STRING - %s\n", inet_ntoa(recv_packet.udp_ip), 
      recv_packet.udp_port, recv_packet.topic, recv_packet.payload);
}

void run_tcp_client(int sockfd) {
  int rc;
  char buf[BUFLEN];
  memset(buf, 0, BUFLEN);

  struct udp_packet recv_packet;
  memset(&recv_packet, 0, sizeof(struct udp_packet));

  struct tcp_message send_msg;
  memset(&send_msg, 0, sizeof(struct tcp_message));

  /* multiplexeaza intre citirea de la tastatura si primirea unui
  mesaj de la server */
  struct pollfd fds[2];
  fds[0].fd = sockfd;
  fds[0].events = POLLIN;
  fds[1].fd = 0;
  fds[1].events = POLLIN;

  while (1) {
    rc = poll(fds, NUM_CONEXIONS, 0);
    DIE(rc < 0, "poll");
    if (fds[0].revents & POLLIN) {
      /* se primeste mesajul de la server */
      int rc = recv_all(sockfd, &recv_packet, sizeof(recv_packet));
      DIE(rc < 0, "recv");
      if (strncmp(recv_packet.payload, "exit", 4) == 0) {
        close(sockfd);
        return;
      } else {
        /* se primeste un pachet de tip udp_packet */
        if (recv_packet.type == 0)
          format0_payload(recv_packet);
        else if (recv_packet.type == 1)
          format1_payload(recv_packet);
        else if (recv_packet.type == 2)
          format2_payload(recv_packet);
        else if (recv_packet.type == 3)
          format3_payload(recv_packet);
      }
    } else if (fds[1].revents & POLLIN) {
      /* s-au primit date de la stdin */
      fgets(buf, sizeof(buf), stdin);
      memcpy(send_msg.msg, buf, strlen(buf));
        
      /* se verifica daca s-a citit un mesaj gol */
      if (isspace(buf[0])) {
        continue;
      } else if (strncmp(buf, "exit", 4) == 0) {
        send_all(sockfd, &send_msg, sizeof(struct tcp_message));
        close(sockfd);
        return;
      } else if (strncmp(buf, "subscribe", 9) == 0) {
        send_all(sockfd, &send_msg, sizeof(struct tcp_message));
        printf("Subscribed to topic.\n");
      } else if (strncmp(buf, "unsubscribe", 11) == 0) {
        send_all(sockfd, &send_msg, sizeof(struct tcp_message));
        printf("Unsubscribed from topic.\n");
      } else {
        /* comanda invalida */
        continue;
      }
    }
  }
}

int main(int argc, char *argv[]) {
  setvbuf(stdout, NULL, _IONBF, BUFSIZ);
  int sockfd = -1;

  if (argc != 4) {
    printf("\n Usage: %s <id_client> <ip_server> <port_server>\n", argv[0]);
    return 1;
  }

  /* Parsam port-ul ca un numar */
  uint16_t port;
  int rc = sscanf(argv[3], "%hu", &port);
  DIE(rc != 1, "Given port is invalid");

  /* Obtinem un socket TCP pentru conectarea la server */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  DIE(sockfd < 0, "socket");

  /* dezactivam algoritmul lui Nagle */
  int no_delay = 1;
  if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &no_delay, sizeof(int)) < 0)
    perror("setsockopt(TCP_NODELAY) failed for tcp");

  /* Completăm in serv_addr adresa serverului, familia de adrese si portul
  pentru conectare */
  struct sockaddr_in serv_addr;
  socklen_t socket_len = sizeof(struct sockaddr_in);

  memset(&serv_addr, 0, socket_len);
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);
  rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
  DIE(rc <= 0, "inet_pton");

  /* Ne conectăm la server */
  rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  DIE(rc < 0, "connect");

  /* retinem informatiile despre client */
  struct info_tcp_client info;
  memset(&info, 0, sizeof(struct info_tcp_client));
  memcpy(info.id, argv[1], strlen(argv[1])); 
  info.port = port;
  rc = inet_aton(argv[2], &info.ip);
  DIE(rc == 0, "inet_aton");
  /* inca nu stim socketul pe care serverul i-l ofera dupa accept,
  deci va fi setat pe 0 */

  /* trimitem mesaj serverului dupa ce ne-am conectat
  cu informatie despre clientul tcp */ 
  send_all(sockfd, &info, sizeof(struct info_tcp_client));

  run_tcp_client(sockfd);

  if (sockfd > 0)
    close(sockfd);

  return 0;
}