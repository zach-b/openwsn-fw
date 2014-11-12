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

#define QUEUE_WATCHER_PERIOD   10000

//=========================== typedef =========================================

//=========================== module variables ================================

typedef struct {
   uint16_t             periodMaintenance;
   opentimer_id_t       maintenanceTimerId;
   uint8_t              lastPacketsInQueue[MAXNUMNEIGHBORS];
   uint8_t              neighborRaw;
} otf_vars_t;

//=========================== prototypes ======================================

// admin
void      otf_init(void);
// notification from sixtop
void      otf_notif_addedCell(void);
void      otf_notif_removedCell(void);

/**
\}
\}
*/

#endif
