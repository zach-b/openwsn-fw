#ifndef __BIER_H
#define __BIER_H


#include "opendefs.h"
#include "processIE.h"
//=========================== define ==========================================



//=========================== typedef =========================================



//=========================== module variables ================================

typedef struct {
	asn_t last_asn;
} bier_vars_t;

//=========================== prototypes ======================================

// admin
void      bier_init(void);
// from upper layer
owerror_t bier_send(OpenQueueEntry_t *msg);
// from lower layer
void      task_bierNotifReceive(void);
void	  bier_notifEndOfSlotFrame(void);
bool	  bier_macIsBitSet(OpenQueueEntry_t* msg, uint8_t bitindex);
void 	  bier_macResetBit(OpenQueueEntry_t* msg, uint8_t bitindex);
void	  bier_macSetBit(OpenQueueEntry_t* msg, uint8_t bitindex);
uint16_t  bier_asnDiff(asn_t* someASN, asn_t* anotherASN);

/**
\}
\}
*/

#endif
