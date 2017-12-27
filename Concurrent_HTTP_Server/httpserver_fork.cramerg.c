#include <sys/types.h>
#include <sys/socket.h>
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

int open_listenfd(int port)
{
  int listenfd, optval=1;
  struct sockaddr_in serveraddr;

  /* Create a socket descriptor */
  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    return -1;

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
  if (listen(listenfd, LISTENQ) < 0)
    return -1;

  return listenfd;
}

int main(int argc, char **argv) {
  int listenfd, connfd, filefd, port, clientlen;
  struct sockaddr_in clientaddr;

  //1. Listen to the specified port
  port = atoi(argv[1]);
  if((listenfd = open_listenfd(port)) == -1) {
    perror("listen");
    return EXIT_FAILURE;
  }
  clientlen = sizeof(clientaddr);
  char buffer[1024];
  char* path_end;
  char path[1024];
  int i, childpid;

  //2. Run the server
  while(1) {
    //2a. Accept an incoming connection
    connfd = accept(listenfd, (struct sockaddr *)&clientaddr, (socklen_t*)&clientlen);
    if (connfd == -1) {
      perror("accept");
      continue;
    }
    if( (childpid = fork()) == 0) { //if the child process
      //close(listenfd);
      //2b. Receive the HTTP GET request
      memset(buffer, '\0', sizeof(buffer)); //reset the buffer before calling read
      read(connfd, buffer, MAXLINE); //HTTP get request
      path_end = strstr(buffer, " HTTP"); //search for the end of the file path
      //initialize buffer to null terminating characters with a dot in the front
      memset(path, '\0', sizeof(path));
      path[0] = '.';

      //2c. Get the filepath from the request
      for(i = 4; &(buffer[i]) != path_end; i++) {
        path[i-3] = buffer[i];
      }

      //2d. Attempt to open the file
      if((filefd = open(path, O_RDONLY)) == -1) {
        perror("open");
        if (errno == EACCES) { //forbidden file
          write(connfd, HTTP_FORB, strlen(HTTP_FORB));
        } else { //no such file or directory
          write(connfd, HTTP_NF, strlen(HTTP_NF));
        }
        close(connfd);
        continue;
      }

      //2e. Send the file to the client
      memset(buffer, '\0', sizeof(buffer));
      write(connfd, HTTP_OK, strlen(HTTP_OK));
      while (read(filefd, buffer, MAXLINE-1) > 0) {
        write(connfd, buffer, strlen(buffer));
        //sleep(1); //this line will cause text to be written slowly so simultaneous connections can be seen easily
        memset(buffer, '\0', sizeof(buffer));
      }

      //2f. Close the connection
      close(filefd);
      close(connfd);
      exit(0);
    }
  }
}
