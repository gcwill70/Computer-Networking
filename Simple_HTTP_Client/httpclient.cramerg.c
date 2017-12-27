#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h>
#include <strings.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define MAXLINE 100
#define MAXSIZE 1024

int open_clientfd(char *hostname, int port)
{
  int clientfd;
  struct hostent *hp;
  struct sockaddr_in serveraddr;

  if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    return -1; /* check errno for cause of error */

  /* Fill in the server's IP address and port */
  if ((hp = gethostbyname(hostname)) == NULL)
    return -2; /* check h_errno for cause of error */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  bcopy((char *)hp->h_addr,
  (char *)&serveraddr.sin_addr.s_addr, hp->h_length);
  serveraddr.sin_port = htons(port);

  /* Establish a connection with the server */
  if (connect(clientfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
    return -1;
  return clientfd;
}

//Usage: ./httpclient ee347lnx4.ecn.purdue.edu 80 /ece463/lab1/test_short.txt
int main(int argc, char** argv) {
	if(argc != 4) {
		printf("Error: wrong number of command line arguments.\n");
		return 1;
	}

  char* servername = argv[1];
  int serverport = atoi(argv[2]);
  char* pathname = argv[3];
  char buffer[MAXSIZE];
  char http_get_request[MAXSIZE];
  int status;

  //1. Establish connection
  //http://ee347lnx4.ecn.purdue.edu:80/ece463
  int clientfd = open_clientfd(servername, serverport);
  if (clientfd < 0) {
    printf("Error opening connection \n");
    return 0;
  }

  //2. Form get request and send
  strcpy(http_get_request, "GET ");
  strcat(http_get_request, strcat(pathname, " HTTP/1.0\r\n\r\n"));
  //GET <file path> HTTP/1.0<CR><LF><CR><LF>
  write(clientfd, http_get_request, strlen(http_get_request));

  /* EXAMPLE HEADER:
  HTTP/1.1 200 OK\r\n
  Date: Tue, 05 Sep 2017 18:31:15 GMT\r\n
  Server: Apache/2.2.22 (Ubuntu)\r\n
  Last-Modified: Mon, 28 Aug 2017 21:25:11 GMT\r\n
  ETag: \"1020eb1-ff7-557d6ee009809\"\r\n
  Accept-Ranges: bytes\r\n
  Content-Length: 4087git \r\n
  Vary: Accept-Encoding\r\n
  Connection: close\r\n
  Content-Type: text/plain\r\n\r\n
  */

  //3. Display server response
  memset(buffer, 0, sizeof(buffer));
  while((status = recv(clientfd, buffer, MAXSIZE-1, 0)) > 0) {
    buffer[MAXSIZE-1] = '\0';
    printf("%s", buffer);
    memset(buffer, 0, sizeof(buffer));
  }
  if(status != 0) {
    printf("Error: could not receive bytes properly. recv() returned errno %d).\n", status);
    close(clientfd);
    return 0;
  }

  close(clientfd);
  return 0;
}
