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
char message[MAX];

void send_thread(int sockfd) {
  int color = 0;
  for (int i = 0; i < (int)strlen(name); ++i)
    color += name[i];
  color = (color % 7) + 31;

  char packet[MAX];

  while(1) {
    fgets(message, MAX - strlen(name) - 2, stdin);
    printf("\033[1A");
    sprintf(packet, "\r\033[%dm%s\033[0m: %s", color, name, message);
    memset(message, 0, sizeof(message));
    int mlen = send(sockfd, packet, strlen(packet), 0);
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
    printf("\a%s", packet);
  }
}

int main(int argc, char **argv) {

  std::thread tsend, trcv;
  int sockfd;
  struct sockaddr_in addr;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if(sockfd == -1) {
    perror("Erro ao criar socket");
    return 1;
  }

  addr.sin_family = AF_INET;
  addr.sin_port   = htons(5600);
  if (argc < 2)
    addr.sin_addr.s_addr = inet_addr("187.45.160.148");
  else
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
