#ifndef __BIER_H
#define __BIER_H


#include "opendefs.h"
#include "processIE.h"
//=========================== define ==========================================
#define BIER_MAX_TRACKS   				1
#define BIER_MAX_OWNED_BITS				3


//=========================== typedef =========================================

// bit - bundle mapping
typedef struct {
	uint8_t				bitIndex[BIER_MAX_OWNED_BITS];
	uint8_t				bundleID[BIER_MAX_OWNED_BITS];
} biermap_entry_t;

typedef struct {
	uint8_t				trackID[BIER_MAX_TRACKS];
	biermap_entry_t		biermap_entry[BIER_MAX_TRACKS];
} biermap_t;

//=========================== module variables ================================

typedef struct {
	biermap_t	biermap;
} bier_vars_t;

//=========================== prototypes ======================================

// admin
void      bier_init(void);
// from upper layer
owerror_t bier_send(OpenQueueEntry_t *msg);
// from lower layer
void      task_bierNotifSendDone(void);
void      task_bierNotifReceive(void);

/**
\}
\}
*/

#endif
