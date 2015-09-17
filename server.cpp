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
  while(ok) {
    mutmsgs.lock();
    bool e = msgs.empty();
    mutmsgs.unlock();

    if(!e) {
      mutmsgs.lock();
        char* msg = msgs.front();
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

          int mlen = send(socks[i], msg, strlen(msg), 0);
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
}

void rcv_thread(int sockfd) {
  int ok=1;
  while(ok) {
    char* msg = (char*) malloc(MAX);
    memset(msg, 0, MAX);
    int mlen = recv(sockfd, msg, MAX,0);
    if(mlen == -1 || mlen == 0) {
      close(sockfd);
      ok = 0;
    }
    else {
      msg[mlen] = '\0';
      printf("\033[94mReceived from\033[0m %d: %s\n", sockfd, msg);
      mutmsgs.lock();
      msgs.push(msg);
      mutmsgs.unlock();
    }
  }
}

int main(int argc, char **argv) {

  std::thread tsend;
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
      printf("\033[94mClient connected\033[0m: %d (%s)\n", sock_client, inet_ntoa(addr.sin_addr));

      mutsocks.lock();
      socks.push_back(sock_client);
      mutsocks.unlock();
      std::thread(&rcv_thread,sock_client).detach();
    }
  }

  tsend.join();
  for(int i = 0; i < socks.size(); i++) {
    close(socks[i]);
  }

  close(sockfd);
  return 0;
}
