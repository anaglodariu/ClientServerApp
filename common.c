#include "common.h"

#include <sys/socket.h>
#include <sys/types.h>

/* verifica daca un anume socket este abonat la un anume topic */
int findTopic(struct topicList* list, int socket, char* topic) {
  struct topicList* aux = &list[socket - 5];
  while (aux != NULL) {
    if (strcmp(aux->topic, topic) == 0) {
      return 1; /* clientul de pe socket este abonat la acest topic */
    }
    aux = aux->next;
  }
  return 0;
}

struct topicList* createList() {
  /* construim un vector in care pozitia 0 in vector 
  corespunde primului fd al clientilor (= 5) */
  struct topicList* list = (struct topicList*)calloc(3, sizeof(struct topicList));
  DIE(list == NULL, "calloc list");
  return list;
}

/* insereaza un topic la socketul corespunzator */
void insert(struct topicList* list, int* len, char* topic, int socket) {
  if (*len <= socket - 5) {
    list = (struct topicList*)realloc(list, (*len * 2) * sizeof(struct topicList));
    DIE(list == NULL, "realloc list");
    *len = *len * 2;
  }
  
  /* daca e primul in lista de topicuri */
  if (list[socket - 5].topic[0] == '\0') {
    strcpy(list[socket - 5].topic, topic);
    list[socket - 5].next = NULL;
  } else {
    /* daca nu e primul in lista de topicuri este adaugat la final */
    struct topicList* new = (struct topicList*)calloc(1, sizeof(struct topicList));
    DIE(new == NULL, "calloc new");
    strcpy(new->topic, topic);
    new->next = NULL;

    struct topicList* aux = &list[socket - 5];
    while (aux->next != NULL) {
      aux = aux->next;
    }
    aux->next = new;
  }
}

/* functie care primeste exact len octeti din revc */
int recv_all(int sockfd, void *buffer, size_t len) {
  size_t bytes_received = 0;
  size_t bytes_remaining = len;
  char *buff = buffer;

  while(bytes_remaining) {
    bytes_received = recv(sockfd, buff, bytes_remaining, 0);
    if (bytes_received < 0) /* caz eroare recv */
      return bytes_received;
    
    bytes_remaining -= bytes_received;
    buff += bytes_received;
  }
  /* returnezi numarul total de octeti primiti */
  return len;
}

/* functie care trimite exact len octeti cu send */
int send_all(int sockfd, void *buffer, size_t len) {
  size_t bytes_sent = 0;
  size_t bytes_remaining = len;
  char *buff = buffer;
  
  while(bytes_remaining) {
    bytes_sent = send(sockfd, buff, bytes_remaining, 0);
    if (bytes_sent < 0) /* caz eroare send */
      return bytes_sent;
    
    bytes_remaining -= bytes_sent;
    buff += bytes_sent;
  }

  /* returnezi numarul total de octeti primiti */
  return len;
}