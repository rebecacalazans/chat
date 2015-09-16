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

#define MAX 1024

std::vector<int> socks;
std::queue<char*> msgs;
std::mutex mutmsgs, mutsocks;


void send_thread() {
  int ok=1;
  char msg[1024];
  while(ok) {
    mutmsgs.lock();
    bool e = msgs.empty();
    mutmsgs.unlock();

    if(!e) {

      mutmsgs.lock();
        strcpy(msg, msgs.front());
        msgs.pop();
      mutmsgs.unlock();

      mutsocks.lock();
      bool e = socks.empty();
      mutsocks.unlock();

      if(!e) {
        for(int i = 0; ; i++) {
          mutsocks.lock();
          int s = socks.size();
          mutsocks.unlock();

          if (i >= s) break;

          int mlen = send(socks[i], msg, strlen(msg), 0);
          if(mlen == -1) {
            //perror("Erro ao enviar mensagem socket %d", i);
            close(socks[i]);
            mutsocks.lock();
              socks.erase(socks.begin()+i);
            mutsocks.unlock();
          }
        }
      }
    }
  }
}

void rcv_thread(int sockfd) {
  int ok=1;
  while(ok) {
    char* msg = (char*) malloc(MAX);
    memset(msg, 0, MAX);
    int mlen = recv(sockfd, msg, MAX,0);
    if(mlen == -1 || mlen == 0) {
      //perror("Erro ao receber mensagem socket");
      close(sockfd);
      ok = 0;
    }
    else {
      msg[mlen] = '\0';
      printf("Received: %s\n", msg);
      mutmsgs.lock();
      msgs.push(msg);
      mutmsgs.unlock();
    }
  }
}

int main(int argc, char **argv) {

  std::thread tsend;
  std::vector<std::thread> trcv;
  int sockfd, sock_client;
  struct sockaddr_in addr;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if(sockfd == -1) {
    perror("Erro ao criar socket");
    return 1;
  }


  addr.sin_family = AF_INET;
  addr.sin_port   = htons(5600);
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

  tsend = std::thread(&send_thread);

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
      printf("\033[32mClient connected: %s\033[0m\n", inet_ntoa(addr.sin_addr));

      trcv.push_back(std::thread(&rcv_thread,sock_client));
      mutsocks.lock();
      socks.push_back(sock_client);
      mutsocks.unlock();
    }
  }

  for(int i = 0; i < trcv.size(); i++) {
  trcv[i].join();
  }
    tsend.join();
  for(int i = 0; i < socks.size(); i++) {
    close(socks[i]);
  }

  close(sockfd);
  return 0;
}
