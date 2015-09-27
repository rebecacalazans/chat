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
#include <vector>
#include <queue>
#include <mutex>

const int NAME_LEN = 24;
const int MSG_LEN = 1000;
const int MAX_USERS = 1024;
const short ncolors = 6;
struct packet {
  char name [NAME_LEN+1];
  short color;
  char msg[MSG_LEN+1];
};
const int PACKET_LEN = sizeof(struct packet);;

std::vector<int> socks;
std::queue<struct packet*> msgs;
std::mutex mutmsgs, mutsocks;

char names[NAME_LEN][MAX_USERS];

void send_thread() {
    mutmsgs.lock();
    bool e = msgs.empty();
    mutmsgs.unlock();

    if(!e) {
      mutmsgs.lock();
      struct packet* msg = msgs.front();
      msgs.pop();
      mutmsgs.unlock();

      mutsocks.lock();
      bool e = socks.empty();
      mutsocks.unlock();

      printf("\033[94mSending message to all clients\033[0m\n");

      if(!e) {
        std::vector<int> toremove;

        for(int i = 0; ; i++) {
          mutsocks.lock();
          int s = socks.size();
          mutsocks.unlock();

          if (i >= s) break;

          printf("\033[94mSending to client\033[0m %d: ", socks[i]);

          int mlen = send(socks[i], msg, PACKET_LEN, 0);
          if(mlen == -1) {
            printf("\033[31mClient disconnected!\033[0m\n");
            close(socks[i]);
            toremove.push_back(i);
          } else {
            printf("\033[32mSucceed!\033[0m\n");
          }
        }

        if (toremove.size() > 0) {
          mutsocks.lock();
          for (int i = 0; i < (int)toremove.size(); ++i) {
            socks.erase(socks.begin()+toremove[i]);
          }
          mutsocks.unlock();
        }
      } else {
        printf("\033[31mAll clients connected!\033[0m\n");
      }

      free (msg);
    }
}

void rcv_thread(int sockfd) {
  int ok=1;
  short color = 0;
  for (int i = 0; i < (int)strlen(names[sockfd]); ++i)
    color += names[sockfd][i];
  color = color % ncolors + 1;

  while(ok) {
    struct packet* pckt = (struct packet*) malloc(PACKET_LEN);
    memset((char*)pckt, 0, PACKET_LEN);
    strcpy(pckt->name,names[sockfd]);
    pckt->color = color;
    int mlen = recv(sockfd, pckt->msg, MSG_LEN,0);
    if(mlen == -1 || mlen == 0) {
      close(sockfd);
      ok = 0;
    }
    else {
      pckt->msg[mlen] = '\0';
      printf("\033[94mReceived from\033[0m %d: %s\n", sockfd, pckt->msg);
      mutmsgs.lock();
      msgs.push(pckt);
      mutmsgs.unlock();
      send_thread();
    }
  }
}

int main(int argc, char **argv) {

  std::thread tsend;
  int sockfd, sock_client;
  unsigned int port;
  struct sockaddr_in addr;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if(sockfd == -1) {
    perror("Erro ao criar socket");
    return 1;
  }
  port = 5600;
  for (int i = 0; i < argc; i++) {
    if(strcmp(argv[i],"--port") == 0) {
      port = (short)atoi(argv[i+1]);
      printf("porta: %d\n", port);
      break;
    }
  }



  addr.sin_family = AF_INET;
  addr.sin_port   = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;

  memset(&addr.sin_zero,0,sizeof(addr.sin_zero));

  if(bind(sockfd,(struct sockaddr*)&addr,sizeof(addr)) == -1) {
    perror("Erro na funcao bind()");
    return 1;
  }
  if(listen(sockfd,1) == -1) {
    printf("Erro na funcao listen()\n");
    return 1;
  }

  while(1) {
    sock_client = accept(sockfd,0,0);
    if(sock_client == -1) {
      perror("Erro na funcao accept()");
      return 1;
    }
    else {
      // Get client info
      socklen_t addr_size = sizeof(struct sockaddr_in);
      int res = getpeername(sock_client, (struct sockaddr*)&addr, &addr_size);
      printf("\033[94mClient connected\033[0m: %d (%s)\n", sock_client, inet_ntoa(addr.sin_addr));
      int mlen = recv(sock_client, names[sock_client], MSG_LEN,0);
      if(mlen == -1 || mlen == 0) {
        close(sock_client);
      }
      else {
        names[sock_client][mlen]='\0';
        mutsocks.lock();
        socks.push_back(sock_client);
        mutsocks.unlock();
        send_thread();
        std::thread(&rcv_thread,sock_client).detach();
      }
    }
  }

  tsend.join();
  for(int i = 0; i < socks.size(); i++) {
    close(socks[i]);
  }

  close(sockfd);
  return 0;
}
