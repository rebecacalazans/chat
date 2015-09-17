#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <termios.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <thread>
#include <mutex>
#define MAX 1024

int color;
char name[20];
char message[MAX];

std::mutex mutmessage;

struct termios oldtio = {};
void startInput() {
  if (tcgetattr(0, &oldtio) < 0)
    perror("tcsetattr()");

  struct termios newtio = oldtio;
  newtio.c_lflag &= ~ICANON;
  newtio.c_lflag &= ~ECHO;
  newtio.c_cc[VMIN] = 1;
  newtio.c_cc[VTIME] = 0;
  if (tcsetattr(0, TCSANOW, &newtio) < 0)
    perror("tcsetattr ICANON");
}

void stopInput() {
  if (tcsetattr(0, TCSANOW, &oldtio) < 0)
    perror ("tcsetattr ~ICANON");
}

void send_thread(int sockfd) {
  color = 0;
  for (int i = 0; i < (int)strlen(name); ++i)
    color += name[i];
  color = (color % 7) + 31;

  char packet[MAX];

  while(1) {
    mutmessage.lock();
    message[0] = '\0';
    mutmessage.unlock();

    int i = 0;

    startInput();
    while (1) {
      char c = getchar();
      if (c == '\n') {
        if (i == 0) continue;
        printf("\n");
        break;
      }

      if (c == 0x7f) { // Backspace
        if (i > 0) {
          mutmessage.lock();
          message[--i] = '\0';
          mutmessage.unlock();

          printf("\b \b");
        }
      } else if (c == 27) { // Escape sequence
        // Ignore escape sequence
        c = getchar();
        c = getchar();
        continue;
      } else {
        mutmessage.lock();
        message[i++] = c;
        message[i] = '\0';
        mutmessage.unlock();
        printf("%c", c);
      }

      if (i == MAX-1) {
        mutmessage.lock();
        message[i] = '\n';
        mutmessage.unlock();
        printf("\n");
        break;
      }
    }
    stopInput();

    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int meslines = (strlen(message) + w.ws_col - 1) / w.ws_col;
    printf("\033[%dA", meslines);

    mutmessage.lock();
    sprintf(packet, "\033[%dm%s\033[0m: %s", color, name, message);
    mutmessage.unlock();

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
    int mlen = recv(sockfd, packet, MAX, 0);
    if(mlen == -1) {
      perror("Erro ao receber mensagem\n");
      exit (1);
    }
    packet[mlen] = '\0';

    // Get terminal size
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int col = w.ws_col - strlen(packet) + 9; // Get terminal size minus packet size (except escape sequences (9 chars))
    /*
    int meslines = (strlen(message) + w.ws_col - 1) / w.ws_col-1;
    if (meslines > 0)
      printf("\033[%dA", meslines);
    */

    printf("\r%s", packet);
    for (int i = 0; i < col; ++i) {
      printf(" ");
    }

    printf("\n\033[%dm%s\033[0m> ", color, name);

    mutmessage.lock();
    printf("%s", message);
    mutmessage.unlock();
    fflush(stdout);
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

  printf("\033[%dm%s\033[0m> ", color, name);
  fflush(stdout);

  tsend.join();
  trcv.join();

  close(sockfd);
  return 0;
}
