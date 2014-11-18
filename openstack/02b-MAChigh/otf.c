#include "opendefs.h"
#include "otf.h"
#include "neighbors.h"
#include "sixtop.h"
#include "scheduler.h"
#include "openqueue.h"
#include "openserial.h"
#include "IEEE802154E.h"
#include "idmanager.h"

//=========================== variables =======================================

otf_vars_t otf_vars;

//=========================== prototypes ======================================

void otf_addCell_task(void);
void otf_removeCell_task(void);

void otf_maintenance_timer_cb(void);
void timer_otf_management_fired(void);

// helper functions
uint8_t otf_getAvgNumOfPkt(uint8_t currentNumOfPkt);
//=========================== public ==========================================

void otf_init(void) {
   otf_vars.periodMaintenance  = QUEUE_WATCHER_PERIOD;
   otf_vars.neighborRaw        = 0;
   memset(&(otf_vars.lastNumPkt[0]),0,sizeof(otf_vars.lastNumPkt));
   otf_vars.lastAvgNumPkt      = 0;
   memset(&(otf_vars.lastPacketsInQueue[0]),0,sizeof(otf_vars.lastPacketsInQueue));
   otf_vars.maintenanceTimerId = opentimers_start(
      otf_vars.periodMaintenance,
      TIMER_PERIODIC,
      TIME_MS,
      otf_maintenance_timer_cb
   );
}

void otf_notif_addedCell(void) {
   scheduler_push_task(otf_addCell_task,TASKPRIO_OTF);
}

void otf_notif_removedCell(void) {
   scheduler_push_task(otf_removeCell_task,TASKPRIO_OTF);
}

void otf_maintenance_timer_cb(void) {
   scheduler_push_task(timer_otf_management_fired,TASKPRIO_OTF_MAINTAIN);
}

void timer_otf_management_fired(void) {
    uint8_t currentPacketsInQueue;
    uint8_t currentAvgPacketsInQueue;
    open_addr_t* address;
    uint8_t output[3];
    
    if (ieee154e_isSynch() == FALSE) {
        return;
    }
    
    if (neighbors_getNumNeighbors() == 0) {
        return;
    }
        
#ifdef NEIGHBOR_ORIENTED 
    otf_vars.neighborRaw = (otf_vars.neighborRaw + 1)%MAXNUMNEIGHBORS;
    address = neighbors_getNeighborByIndex(otf_vars.neighborRaw);
    
    do {
        if(address != NULL) {
            currentPacketsInQueue = openqueue_getToBeSentPacketsByNeighbor(address);
            if (currentPacketsInQueue < otf_vars.lastPacketsInQueue[otf_vars.neighborRaw]) {
                if (otf_vars.lastPacketsInQueue[otf_vars.neighborRaw] - currentPacketsInQueue > 1) {
                    sixtop_removeCell(address);
                } else {
                    //return directly. Wait for next time to see whether the packet is indeed decreased
                    return;
                }
            } else {
                if (currentPacketsInQueue > otf_vars.lastPacketsInQueue[otf_vars.neighborRaw]) {
                    sixtop_addCells(address,1);
                } else {
                    if (currentPacketsInQueue == otf_vars.lastPacketsInQueue[otf_vars.neighborRaw]) {
                        if (currentPacketsInQueue == 0) {
                            // if there is no packet in queue, try to remove cells if existed
                            sixtop_removeCell(address);
                        } else {
                            sixtop_addCells(address,1);
                        }
                    } else {
                        // No possible to be here.
                    }
                }
            }
            otf_vars.lastPacketsInQueue[otf_vars.neighborRaw] = currentPacketsInQueue;
            output[0] = address->addr_64b[6];
            output[1] = address->addr_64b[7];
            output[2] = currentPacketsInQueue;
            openserial_printStatus(STATUS_QUEUE_OTF,&(output[0]),sizeof(output));
            break;
        } else {
            otf_vars.neighborRaw = (otf_vars.neighborRaw + 1)%MAXNUMNEIGHBORS;
            address = neighbors_getNeighborByIndex(otf_vars.neighborRaw);
        } 
    } while (TRUE);
#else
    currentPacketsInQueue  = openqueue_getToBeSentPackets();
    currentAvgPacketsInQueue = otf_getAvgNumOfPkt(currentPacketsInQueue);
    
    if (currentAvgPacketsInQueue < otf_vars.lastAvgNumPkt) {
        otf_removeCell_task();
    } else {
        if (currentAvgPacketsInQueue > otf_vars.lastAvgNumPkt) {
            otf_addCell_task();
        } else {
            if (currentAvgPacketsInQueue == otf_vars.lastAvgNumPkt) {
                if (currentAvgPacketsInQueue == 0) {
                    // if there is no packet in queue, try to remove cells if existed
                    otf_removeCell_task();
                } else {
                    otf_addCell_task();
                }
            } else {
                // No possible to be here.
            }
        }
    }
    otf_vars.lastAvgNumPkt = currentAvgPacketsInQueue;
#endif
}

//=========================== private =========================================

void otf_addCell_task(void) {
   open_addr_t          neighbor;
   bool                 foundNeighbor;
   
   // get preferred parent
   foundNeighbor = neighbors_getPreferredParentEui64(&neighbor);
   if (foundNeighbor==FALSE) {
      return;
   }
   
   // call sixtop
   sixtop_addCells(
      &neighbor,
      1
   );
}

void otf_removeCell_task(void) {
   open_addr_t          neighbor;
   bool                 foundNeighbor;
   
   // get preferred parent
   foundNeighbor = neighbors_getPreferredParentEui64(&neighbor);
   if (foundNeighbor==FALSE) {
      return;
   }
   
   // call sixtop
   sixtop_removeCell(
      &neighbor
   );
}

bool debugPrint_lastNumPkt() {
   uint8_t output;
   
   output = otf_vars.lastAvgNumPkt;
   
   openserial_printStatus(
       STATUS_LASTPKTNUM,
       (uint8_t*)&output,
       sizeof(output)
   );
   return TRUE;
}

// helper function
uint8_t otf_getAvgNumOfPkt(uint8_t currentNumOfPkt) {
    uint8_t avg;
    uint8_t i;
    open_addr_t* myId;
    
    avg = 0;
    
    // remove the old data
    for (i=SLIDE_WINDOW_SIZE;i>1;i--) {
        otf_vars.lastNumPkt[i-1] = otf_vars.lastNumPkt[i-2];
    }
    // update the lastest one
    otf_vars.lastNumPkt[0] = currentNumOfPkt;
    
    // sum all data
    for (i=0;i<SLIDE_WINDOW_SIZE;i++) {
        avg += otf_vars.lastNumPkt[i];
    }
    
    // calcluate the round up averge value
    if (avg%SLIDE_WINDOW_SIZE ==0) {
        avg = avg/SLIDE_WINDOW_SIZE;
    } else {
        avg = avg/SLIDE_WINDOW_SIZE + 1;
    }
    
    myId = idmanager_getMyID(ADDR_64B);
   
    printf("My ID is 0x%x 0x%x ... \n",myId->addr_64b[6],myId->addr_64b[7]);
    
    printf("average pkt is: %d\n",avg);
    
    return avg;
}