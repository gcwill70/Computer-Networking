/*
 * router.c
 *
 *  Created on: Oct 30, 2017
 *      Author: geoff
 */

#include <sys/timerfd.h>
#include <stdbool.h>
#include "ne.h"
#include "router.h"

//Global variables
int maxfd = 0;
clock_t startTime;
struct sockaddr_in ne_addr, router_addr;
struct pkt_INIT_REQUEST init_request;
struct pkt_INIT_RESPONSE init_response;
fd_set rfds;
struct pkt_RT_UPDATE send_pkt, recv_pkt;
struct itimerspec updateTimer, convergeTimer;
struct itimerspec timeoutTimer[MAX_ROUTERS];
bool didConverge = false; //converge flag

int getInitResp(char* hostname, int ne_port, int router_port) {
  //1. Create a socket
  int send_fd = 0;
  if((send_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    printf("\nError: failed to create the socket.\n");
    return -2;
  }
  //2. Initialize socket addresses
  router_addr.sin_family = AF_INET;
  router_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  router_addr.sin_port = htons(router_port);
  ne_addr.sin_family = AF_INET;
  inet_aton(hostname, &(ne_addr.sin_addr));
  ne_addr.sin_port = htons(ne_port);

  //3. Bind to the socket
  if(bind(send_fd, (struct sockaddr*)&router_addr, sizeof(router_addr)) < 0) {
    close(send_fd);
    printf("\nError: failed to bind to the socket.\n");
    return -1;
  }
  //4. Send init request
  if (sendto(send_fd, (struct pkt_INIT_REQUEST*)&init_request, sizeof(init_request), 0,
  (struct sockaddr*)&ne_addr, sizeof(ne_addr)) < 0) {
  	printf("\nError: failed to send initialization request.\n");
  	return -1;
  }
  //5. Get init response
  if (recvfrom(send_fd, (struct pkt_INIT_RESPONSE *)&init_response, sizeof(init_response),
  0, NULL, NULL) < 0) {
    printf("\nError: failed to receive initialization response.\n");
    return -1;
  }
  return send_fd;
}

int* initTimer(struct itimerspec* timer, int size, int tv_sec) {
  int i;
  int* ret_fd = malloc(sizeof(int) * size); //return array of file descriptors
  for (i = 0; i < size; i++) { //for each timer
    //set appropriate timeout
    timer[i].it_value.tv_sec = tv_sec;
    timer[i].it_value.tv_nsec = 0;
    //set interval to 0 so the timer only expires once (can be reset using resetTimer)
    timer[i].it_interval.tv_sec = 0;
    timer[i].it_interval.tv_nsec = 0;
    //get file descriptor and set timer to 0
    ret_fd[i] = timerfd_create(CLOCK_REALTIME, 0);
    timerfd_settime(ret_fd[i], 0, &(timer[i]), NULL);
    //update max file descriptor if necessary
    if (maxfd < ret_fd[i]) {maxfd = ret_fd[i];}
  }
  return ret_fd;
}

void resetTimer(int timer_fd, struct itimerspec* timer, int size, int tv_sec) {
  int i;
  for (i = 0; i < size; i++) { //for each timer
    //set appropriate timeout and interval
    timer[i].it_value.tv_sec = tv_sec;
    timer[i].it_value.tv_nsec = 0;
    //set timer to 0
    timerfd_settime(timer_fd, 0, &(timer[i]), NULL);
  }
}

void fdSet(int* fd, int size) {
  int i;
  for (i = 0; i < size; i++) { //for each file descriptor
    FD_SET(fd[i], &rfds);
  }
}

int sendUpdate(int update_fd, int send_fd, FILE* Logfile, int routerID) {
  //1. Zero out packet to send and convert
  memset(&send_pkt, 0, sizeof(send_pkt));
  ConvertTabletoPkt(&send_pkt, routerID);

  //2. Send packet to each neighbor
  int i;
  for (i = 0; i < init_response.no_nbr; i++) {
    send_pkt.dest_id = init_response.nbrcost[i].nbr;
    hton_pkt_RT_UPDATE(&send_pkt);
    if (sendto(send_fd, &send_pkt, sizeof(send_pkt), 0, (struct sockaddr*)&ne_addr, sizeof(ne_addr)) < 0) {
      return 0;
    }
    ntoh_pkt_RT_UPDATE(&send_pkt);
  }

  //3. Reset timer and flush the log file
  resetTimer(update_fd, &updateTimer, 1, UPDATE_INTERVAL);
  fflush(Logfile);
  return 1;
}

int getRow(int sender_id) {
  int i;
  for (i = 0; i < init_response.no_nbr; i++) {
    if (sender_id == init_response.nbrcost[i].nbr) {return i;}
  }
  return -1;
}

int recvUpdate(int recv_fd, int converge_fd, int timeout_fd, FILE* Logfile, int routerID) {
  //1. Receive the packet and convert endian
  if (recvfrom(recv_fd, &recv_pkt, sizeof(recv_pkt), 0, NULL, NULL) < 0) {
    return 0;
  }
  ntoh_pkt_RT_UPDATE(&recv_pkt);

  //2. Determine if routes need to be updated
  int row = getRow(recv_pkt.sender_id); //get the row of the routing table
  if (row == -1) {return -1;}
  if (UpdateRoutes(&recv_pkt, init_response.nbrcost[row].cost, routerID)) {
    //table was changed so print to the log file
    PrintRoutes(Logfile, routerID);
    //reset the converge timer because a change was just made
    resetTimer(converge_fd, &convergeTimer, 1, CONVERGE_TIMEOUT);
    //reset converge flag because table was just changed
    didConverge = false;
  }

  //3. Link is active so reset timeout timer & flush log file
  resetTimer(timeout_fd, timeoutTimer, init_response.no_nbr, FAILURE_DETECTION);
  fflush(Logfile);

  return 1;
}

int converged(int converge_fd, FILE* Logfile, int routerID) {
  //1. Check if the table has converged yet
  if(!didConverge) {
    clock_t currentTime = clock(); //get current time
    double timeElapsed = (double)(currentTime - startTime) / CLOCKS_PER_SEC;
    fprintf(Logfile, "%le:Converged\n", timeElapsed);
  }

  //2. Reset converge timer
  resetTimer(converge_fd, &convergeTimer, 1, CONVERGE_TIMEOUT);

  //3. Set converge flag to true & flush log file
  didConverge = true;
  fflush(Logfile);
  return 1;
}

int nbrTimeout(int whichNeighbor, int converge_fd, FILE* Logfile, int routerID) {
  //1. Change cost to INFINITY & log the change
  UninstallRoutesOnNbrDeath(init_response.nbrcost[whichNeighbor].nbr);
  PrintRoutes(Logfile, routerID);

  //2. Rest converge timer and reset converge flag
  resetTimer(converge_fd, &convergeTimer, 1, CONVERGE_TIMEOUT);
  didConverge = false;
  fflush(Logfile);
  return 1;
}

int main (int argc, char *argv[]) {
  if (argc != 5) {
    printf("Error: wrong number of command line arguments.\nUsage: router <router id> <ne hostname> <ne UDP port> <router UDP port>\n");
    return -1;
  }
  //1. Read in command line arguments
  int routerID = atoi(argv[1]);
  char* hostname = argv[2];
  int ne_port = atoi(argv[3]);
  int router_port = atoi(argv[4]);
  init_request.router_id = htonl(routerID);

  //2. Get initialization response
  memset(&ne_addr, 0, sizeof(ne_addr));
  memset(&router_addr, 0, sizeof(router_addr));
  int recv_fd = getInitResp(hostname, ne_port, router_port);
  if(recv_fd == -1) {return -1;} //check if response could not be received

  //3. Initialize routing table
  ntoh_pkt_INIT_RESPONSE(&init_response);
  InitRoutingTbl(&init_response, routerID);

  //4. Open log file and print initial table
  char filename[24] = "router";
  strcat(filename, argv[1]);
  strcat(filename, ".log");
  FILE* Logfile = fopen(filename, "w");
  PrintRoutes(Logfile, routerID);

  //5. Set up timers
  int* converge_fd = initTimer(&convergeTimer, 1, CONVERGE_TIMEOUT);
  int* update_fd = initTimer(&updateTimer, 1, UPDATE_INTERVAL);
  int* timeout_fd = initTimer(timeoutTimer, init_response.no_nbr, FAILURE_DETECTION);
  startTime = clock();

  //6. Run main loop
  int i;
  while (1) {
    ///a) Set file descriptors
    FD_ZERO(&rfds);
    fdSet(&recv_fd, 1);
    fdSet(converge_fd, 1);
    fdSet(update_fd, 1);
    fdSet(timeout_fd, init_response.no_nbr);

    ///b) Call select() and check each file descriptor
    select(maxfd + 1, &rfds, NULL, NULL, NULL);
    //Case 1: need to send update to neighbor
    if (FD_ISSET(*update_fd, &rfds)) {
      sendUpdate(*update_fd, recv_fd, Logfile, routerID); 
    }
    //Case 2: received update from neighbor
    if (FD_ISSET(recv_fd, &rfds)) {
      recvUpdate(recv_fd, *converge_fd, *timeout_fd, Logfile, routerID);
    }
    //Case 3: routing table has converged
    if (FD_ISSET(*converge_fd, &rfds)) {
      converged(*converge_fd, Logfile, routerID);
    }
    //Case 4: neighbor has timed out
    for (i = 0; i < init_response.no_nbr; i++) {
      if (FD_ISSET(timeout_fd[i], &rfds)) {
        nbrTimeout(i, *converge_fd, Logfile, routerID);
      }
    }
  }

  //7. Clean up
  free(converge_fd);
  free(update_fd);
  free(timeout_fd);
  fclose(Logfile);
  return 0;
}
