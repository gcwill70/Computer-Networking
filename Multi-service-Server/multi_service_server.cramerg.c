#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#define LISTENQ 10
#define MAXLINE 100
#define HTTP_OK "HTTP/1.0 200 OK \r\n\r\n"
#define HTTP_NF "HTTP/1.0 404 Not Found \r\n\r\n"
#define HTTP_FORB "HTTP/1.0 403 Forbidden \r\n\r\n"
#define TCP 0
#define UDP 1

int open_listenfd(int port, int type)
{
  int listenfd, optval = 1;
  struct sockaddr_in serveraddr;

  /* Create a socket descriptor */
  if(type) { //UDP
    if ((listenfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
      return -1;
  }
  else { //TCP
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
      return -1;
  }

  /* Eliminates "Address already in use" error from bind. */
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
		 (const void *)&optval , sizeof(int)) < 0)
    return -1;

  /* Listenfd will be an endpoint for all requests to port
     on any IP address for this host */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)port);
  if (bind(listenfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
    return -1;

  /* Make it a listening socket ready to accept
     connection requests */
  if(!type) { //TCP
    if (listen(listenfd, LISTENQ) < 0)
      return -1;
  }

  return listenfd;
}

void runHTTP(int listenfd) {
  int connfd, i, filefd;
  char buffer[1024];
  char* path_end;
  char path[1024];
  struct timeval tv;
  tv.tv_usec = 600;
  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(listenfd, &rfds);
  struct sockaddr_in clientaddr;
  size_t clientlen = sizeof(clientaddr);

  connfd = accept(listenfd, (struct sockaddr *)&clientaddr, (socklen_t*)&clientlen);
  if (connfd == -1) {
    perror("accept");
  }
  if(!fork()) { //if the child process
    //Receive the HTTP GET request
    memset(buffer, '\0', sizeof(buffer)); //reset the buffer before calling read
    read(connfd, buffer, MAXLINE); //HTTP get request
    printf("Get request: %s\n", buffer);
    path_end = strstr(buffer, " HTTP"); //search for the end of the file path
    //initialize buffer to null terminating characters with a dot in the front
    memset(path, '\0', sizeof(path));
    path[0] = '.';

    //2c. Get the filepath from the request
    for(i = 4; &(buffer[i]) != path_end; i++) {
      path[i-3] = buffer[i];
    }
    filefd = open(path, O_RDONLY);
    printf("File to open: %s\n", path);

    //2d. Attempt to open the file
    if(filefd == -1) {
      perror("open");
      if (errno == EACCES) { //forbidden file
        write(connfd, HTTP_FORB, strlen(HTTP_FORB));
      } else { //no such file or directory
        write(connfd, HTTP_NF, strlen(HTTP_NF));
      }
      close(connfd);
      return;
    }

    //2e. Send the file to the client
    memset(buffer, '\0', sizeof(buffer));
    write(connfd, HTTP_OK, strlen(HTTP_OK));
    while (read(filefd, buffer, MAXLINE-1) > 0) {
      write(connfd, buffer, strlen(buffer));
      memset(buffer, '\0', sizeof(buffer));
    }

    //2f. Close the connection
    printf("Closing %s\n", path);
    close(filefd);
    close(connfd);
  }
}

void runUDP(int listenfd) {
  char buffer[70];
  struct sockaddr clientaddr;
  socklen_t clientlen = sizeof(clientaddr);
  memset(buffer, '\0', sizeof(buffer));
  int recvlen = recvfrom(listenfd, buffer, 70, 0, &clientaddr, &clientlen);
  if(recvlen > 0) {
    int temp, seqNum;
    memcpy(&temp, buffer, sizeof(int));
    seqNum = htonl(ntohl(temp) + 1);
    char* pingMessage = malloc(sizeof(char)*65);
    memcpy(pingMessage, &(buffer[4]), sizeof(char)*65);
    memset(buffer, '\0', sizeof(buffer));
    memcpy(buffer, &seqNum, sizeof(seqNum));
    memcpy(&(buffer[sizeof(seqNum)]), pingMessage, sizeof(char)*65);
    if(sendto(listenfd, buffer, sizeof(buffer), 0, &clientaddr, clientlen) < 0) {
      perror("sendto");
      return;
    }
    free(pingMessage);
  }
  else if (recvlen == -1) {
    perror("recvfrom");
    return;
  }
}

int maxfd(int fd1, int fd2) {
  return fd1 > fd2 ? fd1 : fd2;
}

int main(int argc, char **argv) {
  /*
  Usage:
  ./multi_service_server <http service port> <ping service port>
  */
  int listenHTTP, listenUDP, portHTTP, portUDP;
  fd_set rfds;

  //Listen to the specified ports
  portHTTP = atoi(argv[1]);
  portUDP = atoi(argv[2]);
  if ((listenHTTP = open_listenfd(portHTTP, TCP)) == -1) {
    perror("listen (HTTP)");
    return EXIT_FAILURE;
  }
  if ((listenUDP = open_listenfd(portUDP, UDP)) == -1) {
    perror("listen (UDP)");
    close(listenHTTP);
    return EXIT_FAILURE;
  }

  //Start the service
  while(1) {
    FD_ZERO(&rfds);
    FD_SET(listenHTTP, &rfds);
    FD_SET(listenUDP, &rfds);
    select(maxfd(listenHTTP, listenUDP)+1, &rfds, NULL, NULL, NULL);
    if (FD_ISSET(listenHTTP, &rfds)) { //check if HTTP is ready
      if(!fork()) {
        runHTTP(listenHTTP);
      }
    }
    else if (FD_ISSET(listenUDP, &rfds)) { //check if UDP is ready
      runUDP(listenUDP);
    }
  }
}
