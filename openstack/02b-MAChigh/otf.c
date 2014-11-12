#include "opendefs.h"
#include "otf.h"
#include "neighbors.h"
#include "sixtop.h"
#include "scheduler.h"
#include "openqueue.h"

//=========================== variables =======================================

otf_vars_t otf_vars;

//=========================== prototypes ======================================

void otf_addCell_task(void);
void otf_removeCell_task(void);

void otf_maintenance_timer_cb(void);
void timer_otf_management_fired(void);

//=========================== public ==========================================

void otf_init(void) {
   otf_vars.periodMaintenance  = QUEUE_WATCHER_PERIOD;
   otf_vars.neighborRaw        = 0;
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
    open_addr_t* address;
    
    if (ieee154e_isSynch() == FALSE) {
        return;
    }
    
    if (neighbors_getNumNeighbors() == 0) {
        return;
    }
    
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
            break;
        } else {
            otf_vars.neighborRaw = (otf_vars.neighborRaw + 1)%MAXNUMNEIGHBORS;
            address = neighbors_getNeighborByIndex(otf_vars.neighborRaw);
        } 
    } while (TRUE);
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