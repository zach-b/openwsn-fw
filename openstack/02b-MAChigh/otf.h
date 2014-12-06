#ifndef __OTF_H
#define __OTF_H

/**
\addtogroup MAChigh
\{
\addtogroup otf
\{
*/

#include "opendefs.h"
#include "opentimers.h"
#include "neighbors.h"

//=========================== define ==========================================

#define QUEUE_WATCHER_PERIOD   2000  // unit: ms
#define SLIDE_WINDOW_SIZE      4      // slide window size 
//#define NEIGHBOR_ORIENTED

//=========================== typedef =========================================

//=========================== module variables ================================

typedef struct {
   uint16_t             periodMaintenance;
   opentimer_id_t       maintenanceTimerId;
   uint8_t              lastPacketsInQueue[MAXNUMNEIGHBORS];
   uint8_t              lastNumPkt[SLIDE_WINDOW_SIZE];
   uint8_t              lastAvgNumPkt;
   uint8_t              neighborRaw;
} otf_vars_t;

//=========================== prototypes ======================================

// admin
void      otf_init(void);
// notification from sixtop
void      otf_notif_addedCell(void);
void      otf_notif_removedCell(void);

bool      debugPrint_lastNumPkt();

/**
\}
\}
*/

#endif
