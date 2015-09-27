#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <thread>
#include <mutex>
#include <curses.h>

const int NAME_LEN = 24;
const int MSG_LEN = 1000;
const short ncolors = 6;
struct packet {
  char name[NAME_LEN+1];
  short color;
  char msg[MSG_LEN+1];
};
const int PACKET_LEN = sizeof(struct packet);

char name[NAME_LEN];
char msg[MSG_LEN];//Usado para enviar
struct packet *pckt;
int ncolums = 0, nlines = 0;

WINDOW *chatwin, *typewin;

std::mutex mutwin;

int  input(WINDOW *win, char *str, int len, bool cont) {
  int ymax, xmax;
  getmaxyx(win, ymax, xmax);
  int i = 0;
  while(1) {
    int c = wgetch(win);
    if(c == KEY_ENTER || c == 10) {
      if (i > 0)
        break;
    }
    else if (c == KEY_BACKSPACE) {
      if (i > 0) {
        str[--i] = '\0';
        int y, x;
        mutwin.lock();
        getyx(win, y, x);
        if (x > 0) {
          mvwaddch(win, y, --x, ' ');
        }
        else if (y > 0) {
          y--;
          x = ncolums;
          mvwaddch(win, y, --x, ' ');
        }
        wmove(win, y, x);
        mutwin.unlock();
      }
    }
    else if (!((265 <= c && 276 >=c) || (258 <= c && 261 >= c ) || (c == 27) || c == 9)) {
      if (i < len) {
        str[i++] = c;
        waddch(win, c);
      }
    }
    int x, y;

    mutwin.lock();
    getyx(win, y, x);
    if(y == ymax && x == xmax) {
      wresize(win, ++ymax, xmax);
    }
    if (cont) {
      wattron(win, COLOR_PAIR(102));
      mvwprintw(win, 0, ncolums - 10, "%d/%d", i, len);
      wattroff(win, COLOR_PAIR(102));
    }
    wmove(win, y,x);
    wrefresh(win);
    mutwin.unlock();
  }
  return (i);
}

void exit_(void) {
  delwin(chatwin);
  delwin(typewin);
  endwin();
  exit(0);
}

void init_colors() {
  init_pair(1,COLOR_BLUE,COLOR_BLACK);
  init_pair(2,COLOR_GREEN,COLOR_BLACK);
  init_pair(3,COLOR_CYAN,COLOR_BLACK);
  init_pair(4,COLOR_MAGENTA,COLOR_BLACK);
  init_pair(5,COLOR_YELLOW,COLOR_BLACK);
  init_pair(6,COLOR_RED,COLOR_BLACK);
}

void send_thread(int sockfd) {
  while(1) {

    wclear(typewin);
    wattron(typewin, COLOR_PAIR(102));
    mvwhline(typewin, 0, 0, ' ', ncolums);
    mvwprintw(typewin, 0, ncolums - 10, "0/%d", MSG_LEN);
    wattroff(typewin, COLOR_PAIR(102));

    mutwin.lock();
    mvwprintw(typewin, 1, 0, ">> ");
    wrefresh(typewin);
    mutwin.unlock();

    memset(msg,0,MSG_LEN);
    input(typewin, msg, MSG_LEN, true);

    int mlen = send(sockfd, msg, strlen(msg), 0);
    if (mlen == -1 || mlen == 0) {
      printf("Erro ao enviar mensagem");
      exit_();
    }
  }
}

void rcv_thread(int sockfd) {
  pckt = (struct packet*)malloc(PACKET_LEN);

  while(1) {
    memset(pckt, 0, PACKET_LEN);

    int mlen = recv(sockfd, pckt, PACKET_LEN, 0);
    if(mlen == -1 || mlen == 0) {
      perror("Erro ao receber mensagem\n");
      exit_();
    }
    int y,x;

    getyx(typewin, y, x);


    wattron(chatwin, COLOR_PAIR(pckt->color));
    wprintw(chatwin, "%s: ", pckt->name);
    wattroff(chatwin, COLOR_PAIR(pckt->color));
    wprintw(chatwin, "%s\n", pckt->msg);

    mutwin.lock();
    wrefresh(chatwin);
    wrefresh(typewin);
    wmove(typewin, y,x);
    mutwin.unlock();
  }
}

int sockfd;
int main(int argc, char **argv) {
  std::thread tsend, trcv;
  struct sockaddr_in addr;
  unsigned int port = 5600;
  for (int i = 0; i < argc; i++) {
    if(strcmp(argv[i],"--port") == 0) {
      port = (short)atoi(argv[i+1]);
      printf("porta: %d\n", port);
      break;
    }
  }

  //Iniciando curses
  WINDOW *namewin;
  initscr();
  cbreak();
  noecho();

  start_color();
  init_colors();

  init_color(COLOR_BLACK,0,0,0);
  init_pair(101,COLOR_WHITE,COLOR_BLACK);
  init_pair(102,COLOR_BLACK,COLOR_WHITE);

  bkgd(COLOR_PAIR(101));

  getmaxyx(stdscr, nlines, ncolums);

  chatwin = newwin(nlines-5, ncolums, 0, 0);
  scrollok(chatwin, TRUE);
  typewin = newwin(5, ncolums, nlines-5, 0);
  keypad(typewin, TRUE);
  namewin = newwin(5, NAME_LEN+4, nlines/2-2, (ncolums-NAME_LEN-4)/2);
  keypad(namewin, TRUE);

  wbkgd(chatwin,COLOR_PAIR(101));
  wbkgd(typewin,COLOR_PAIR(101));
  wbkgd(namewin,COLOR_PAIR(101));
  wattron(namewin, A_BOLD);
  wattron(chatwin, A_BOLD);

  int y,x;
  getmaxyx(namewin, y, x);

  box(namewin, ACS_VLINE, ACS_HLINE);
  mvwprintw(namewin, 1, x/2 - 9, "Digite seu nome:");
  mvwhline(namewin, 3, 2, ' ', NAME_LEN);

  memset(name, 0, NAME_LEN);
  input(namewin, name, NAME_LEN, false);

  delwin(namewin);
  wrefresh(typewin);
  wrefresh(chatwin);


  //Iniciando conexão
  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if(sockfd == -1) {
    perror("Erro ao criar socket");
    exit_();
  }

  addr.sin_family = AF_INET;
  addr.sin_port   = htons(port);
  if (argc < 2)
    addr.sin_addr.s_addr = inet_addr("187.45.160.148");
  else
    addr.sin_addr.s_addr = inet_addr(argv[1]);
  memset(&addr.sin_zero,0,sizeof(addr.sin_zero));


  if(connect(sockfd,(struct sockaddr*)&addr,sizeof(addr)) != 0)
  {
    printf("Erro ao conectar!\n");
    exit_();
  }
  else {
    int mlen = send(sockfd, name, strlen(name), 0);
    if (mlen == -1 || mlen == 0) {
      wprintw(chatwin, "Erro ao enviar mensagem");
      exit_();
    }
  }

   tsend = std::thread(&send_thread, sockfd);
   trcv = std::thread(&rcv_thread, sockfd);

   tsend.join();
   trcv.join();

  close(sockfd);
  exit_();
}
