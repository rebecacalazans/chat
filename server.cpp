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

  std::thread tsend;
  std::thread trcv;
  int sockfd;
  int sock_client;
  struct sockaddr_in addr;
  unsigned short port;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if(sockfd == -1) {
    perror("Erro ao criar socket");
    return 1;
  }

  if (argc > 1) {
    port = (unsigned short) atoi(argv[1]);
  }
  else
    port = 1234;

  addr.sin_family = AF_INET;
  addr.sin_port   = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;

  memset(&addr.sin_zero,0,sizeof(addr.sin_zero));

  if(bind(sockfd,(struct sockaddr*)&addr,sizeof(addr)) == -1) {
    perror("Erro na funcao bind()\n");
    return 1;
  }

  if(listen(sockfd,1) == -1) {
    printf("Erro na funcao listen()\n");
    return 1;
  }

  printf("Aguardando conexoes...\n");

  sock_client = accept(sockfd,0,0);

  if(sock_client == -1) {
    perror("Erro na funcao accept()\n");
    return 1;
  }

  printf("Digite seu nome: ");
  fgets(name, 18, stdin);
  name[strlen(name) - 1] = '\0';

  tsend = std::thread(&send_thread, sock_client);
  trcv = std::thread(&rcv_thread, sock_client);

  tsend.join();
  trcv.join();

  close(sock_client);
  close(sockfd);
  return 0;
}
