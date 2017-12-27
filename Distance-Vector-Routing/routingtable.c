/*
 * routingtable.c
 *
 *  Created on: Oct 30, 2017
 *      Author: geoff
 */

#include "ne.h"
#include "router.h"

struct route_entry routingTable[MAX_ROUTERS];
int NumRoutes;

void printTable(int routerID, struct route_entry* table, int numEntries) {
  int i;
  printf("Routing table for router #%d:\n", routerID);
  for(i = 0; i < numEntries; i++) {
    printf("\tDestination: %d, Next hop: %d, Cost: %d\n", table[i].dest_id, table[i].next_hop, table[i].cost);
  }
}

 /* Routine Name    : InitRoutingTbl
  * INPUT ARGUMENTS : 1. (struct pkt_INIT_RESPONSE *) - The INIT_RESPONSE from Network Emulator
  *                   2. int - My router's id received from command line argument.
  * RETURN VALUE    : void
  * USAGE           : This routine is called after receiving the INIT_RESPONSE message from the Network Emulator.
  *                   It initializes the routing table with the bootstrap neighbor information in INIT_RESPONSE.
  *                   Also sets up a route to itself (self-route) with next_hop as itself and cost as 0.
  */
void InitRoutingTbl (struct pkt_INIT_RESPONSE *InitResponse, int myID) {
  NumRoutes = InitResponse->no_nbr + 1;
  unsigned int routerID;
  for(routerID = 0; routerID < NumRoutes; routerID++) { //for each connected router and itself
    if(InitResponse->nbrcost[routerID].nbr == myID) {
      routingTable[routerID].dest_id = myID;
      routingTable[routerID].next_hop = myID;
      routingTable[routerID].cost = 0;
      continue;
    }
    routingTable[routerID].dest_id = InitResponse->nbrcost[routerID].nbr;
    routingTable[routerID].next_hop = InitResponse->nbrcost[routerID].nbr;
    routingTable[routerID].cost = InitResponse->nbrcost[routerID].cost;
  }
}

 /* Routine Name    : UpdateRoutes
  * INPUT ARGUMENTS : 1. (struct pkt_RT_UPDATE *) - The Route Update message from one of the neighbors of the router.
  *                   2. int - The direct cost to the neighbor who sent the update.
  *                   3. int - My router's id received from command line argument.
  * RETURN VALUE    : int - Return 1 : if the routing table has changed on running the function.
  *                         Return 0 : Otherwise.
  * USAGE           : This routine is called after receiving the route update from any neighbor.
  *                   The routing table is then updated after running the distance vector protocol.
  *                   It installs any new route received, that is previously unknown. For known routes,
  *                   it finds the shortest path using current cost and received cost.
  *                   It also implements the forced update and split horizon rules. My router's id
  *                   that is passed as argument may be useful in applying split horizon rule.
  */
int UpdateRoutes(struct pkt_RT_UPDATE *RecvdUpdatePacket, int costToNbr, int myID) {
  int status = 0;
  unsigned int newDist = 0;
  int i, j;
  for (i = 0; i < RecvdUpdatePacket->no_routes; i++) { //for each entry in the routing table
    j = 0;
    //find matching entry or at to the end of the table
    while (routingTable[j].dest_id != RecvdUpdatePacket->route[i].dest_id && j != NumRoutes - 1) {
      if (j >= MAX_ROUTERS - 1) {
        printf("No self-entry in routing table for router #%d\nPrinting its routing table...\n", myID);
        printTable(myID, routingTable, MAX_ROUTERS);
        return 0;
      }
      j++;
    }
    newDist = costToNbr + RecvdUpdatePacket->route[i].cost;
    if (routingTable[j].dest_id != RecvdUpdatePacket->route[i].dest_id && j == NumRoutes - 1) { //router isn't in the table
      NumRoutes++;
      j++; //go to next spot after self-entry
      routingTable[j].next_hop = RecvdUpdatePacket->sender_id;
      routingTable[j].cost = newDist;
      routingTable[j].dest_id = RecvdUpdatePacket->route[i].dest_id;
      status = 1;
    } else if ((routingTable[j].next_hop == RecvdUpdatePacket->sender_id) || (newDist < routingTable[j].cost && myID != RecvdUpdatePacket->route[i].next_hop)) {
      routingTable[j].next_hop = RecvdUpdatePacket->sender_id;
      routingTable[j].cost = newDist;
      status = 1;
    }
  }
  return status;
}

 /* Routine Name    : ConvertTabletoPkt
  * INPUT ARGUMENTS : 1. (struct pkt_RT_UPDATE *) - An empty pkt_RT_UPDATE structure
  *                   2. int - My router's id received from command line argument.
  * RETURN VALUE    : void
  * USAGE           : This routine fills the routing table into the empty struct pkt_RT_UPDATE.
  *                   My router's id  is copied to the sender_id in pkt_RT_UPDATE.
  *                   Note that the dest_id is not filled in this function. When this update message
  *                   is sent to all neighbors of the router, the dest_id is filled.
  */
void ConvertTabletoPkt(struct pkt_RT_UPDATE *UpdatePacketToSend, int myID) {
  UpdatePacketToSend->sender_id = myID;
  UpdatePacketToSend->no_routes = NumRoutes;
  int i;
  for (i = 0; i < NumRoutes; i++) {
    UpdatePacketToSend->route[i] = routingTable[i];
  }
}

 /* Routine Name    : PrintRoutes
  * INPUT ARGUMENTS : 1. (FILE *) - Pointer to the log file created in router.c, with a filename that uses MyRouter's id.
  *                   2. int - My router's id received from command line argument.
  * RETURN VALUE    : void
  * USAGE           : This routine prints the routing table to the log file
  *                   according to the format and rules specified in the Handout.
  */
void PrintRoutes (FILE* Logfile, int myID) {
  fprintf(Logfile, "Routing Table:\n");
  int i, j;
  for (i = 0; i < NumRoutes; i++) { //for each table entry
		for (j = 0; j < MAX_ROUTERS; j++) { //for each router
			if (routingTable[i].dest_id == j) { //if destinations match
				fprintf(Logfile, "R%d -> R%d: R%d, %d\n", myID, routingTable[i].dest_id, routingTable[i].next_hop, routingTable[i].cost);
			}
		}
	}
}

 /* Routine Name    : UninstallRoutesOnNbrDeath
  * INPUT ARGUMENTS : 1. int - The id of the inactive neighbor
  *                   (one who didn't send Route Update for FAILURE_DETECTION seconds).
  *
  * RETURN VALUE    : void
  * USAGE           : This function is invoked when a nbr is found to be dead. The function checks all routes that
  *                   use this nbr as next hop, and changes the cost to INFINITY.
  */
void UninstallRoutesOnNbrDeath(int DeadNbr) {
  int i;
  for (i = 0; i < NumRoutes; i++) {
		if (routingTable[i].next_hop == DeadNbr) {
			routingTable[i].cost = INFINITY;
		}
	}
}
