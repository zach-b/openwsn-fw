#ifndef __OPENQUEUE_H
#define __OPENQUEUE_H

/**
\addtogroup cross-layers
\{
\addtogroup OpenQueue
\{
*/

#include "opendefs.h"
#include "IEEE802154.h"

//=========================== define ==========================================

#define QUEUELENGTH  10

//=========================== typedef =========================================

typedef struct {
   uint8_t  creator;
   uint8_t  owner;
} debugOpenQueueEntry_t;

//=========================== module variables ================================

typedef struct {
   OpenQueueEntry_t queue[QUEUELENGTH];
} openqueue_vars_t;

//=========================== prototypes ======================================

// admin
void               openqueue_init(void);
bool               debugPrint_queue(void);
uint8_t            openqueue_getToBeSentPackets();  // return the number of packet owned by SIXTOP_TO_IEEE802154E
uint8_t            openqueue_getToBeSentPacketsByNeighbor(open_addr_t* toNeighbor);
uint8_t            openqueue_getNumOfPacketByCreator(uint8_t creator); // for specific app of QoS
// called by any component
OpenQueueEntry_t*  openqueue_getFreePacketBuffer(uint8_t creator);
owerror_t         openqueue_freePacketBuffer(OpenQueueEntry_t* pkt);
void               openqueue_removeAllCreatedBy(uint8_t creator);
void               openqueue_removeAllOwnedBy(uint8_t owner);
// called by res
OpenQueueEntry_t*  openqueue_sixtopGetSentPacket(void);
OpenQueueEntry_t*  openqueue_sixtopGetReceivedPacket(void);
// called by IEEE80215E
OpenQueueEntry_t*  openqueue_macGetDataPacket(open_addr_t* toNeighbor);
OpenQueueEntry_t*  openqueue_macGetAdvPacket(void);
OpenQueueEntry_t*  openqueue_macGetOneHopPacket(open_addr_t* toNeighbor);
OpenQueueEntry_t*  openqueue_macGetForwardPacket(open_addr_t* toNeighbor);

/**
\}
\}
*/

#endif
