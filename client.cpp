#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <thread>
#define MAX 1024

char name[20];

void send_thread(int sockfd) {
  strcat(name, ": ");
  char* str = (char*) malloc(MAX);
  strcpy(str, name);
  char* packet = str + strlen(name);

  if (!packet) {
    perror("Falha na alocação da memória");
    exit (1);
  }

  while(1) {
    memset(packet, 0, MAX - strlen(name));
    fgets(packet, MAX - strlen(name), stdin);
    int mlen = send(sockfd, str, MAX - strlen(name), 0);
    if (mlen == -1 || mlen == 0) {
      printf("Erro ao enviar mensagem");
      exit (1);
    }
  }
}

void rcv_thread(int sockfd) {
  char* packet = (char*) malloc(MAX);
  if (!packet) {
    perror("Falha na alocação da memória");
      exit (1);
  }

  while(1) {
    memset(packet, 0, MAX);
    int mlen = recv(sockfd, packet, MAX,0);
    if(mlen == -1) {
      perror("Erro ao receber mensagem\n");
      exit (1);
    }
    packet[mlen] = '\0';
    printf("%s\n", packet);
  }
}

int main(int argc, char **argv) {

  std::thread tsend, trcv;
  int sockfd;
  struct sockaddr_in addr;
  unsigned short port;

  if (argc < 2) {
    perror("Não foi encontrado IP de destino");
    return 1;
  }

  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if(sockfd == -1) {
    perror("Erro ao criar socket");
    return 1;
  }

  if (argc > 2) {
    port = (unsigned short) atoi(argv[2]);
  }
  else
    port = 1234;

  addr.sin_family = AF_INET;
  addr.sin_port   = htons(port);
  addr.sin_addr.s_addr = inet_addr(argv[1]);
  memset(&addr.sin_zero,0,sizeof(addr.sin_zero));

  if(connect(sockfd,(struct sockaddr*)&addr,sizeof(addr)) != 0)
  {
    printf("Erro ao se conectar!\n");
    return 1;
  }

  printf("Digite seu nome: ");
  fgets(name, 18, stdin);
  name[strlen(name) - 1] = '\0';

  tsend = std::thread(&send_thread, sockfd);
  trcv = std::thread(&rcv_thread, sockfd);

  tsend.join();
  trcv.join();

  close(sockfd);
  return 0;
}
