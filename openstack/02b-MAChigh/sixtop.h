#ifndef __SIXTOP_H
#define __SIXTOP_H

/**
\addtogroup MAChigh
\{
\addtogroup sixtop
\{
*/

#include "opentimers.h"
#include "opendefs.h"
#include "processIE.h"

//=========================== define ==========================================

enum sixtop_CommandID_num{
   SIXTOP_SOFT_CELL_REQ                = 0x00,
   SIXTOP_SOFT_CELL_RESPONSE           = 0x01,
   SIXTOP_REMOVE_SOFT_CELL_REQUEST     = 0x02,
};

// states of the sixtop-to-sixtop state machine
typedef enum {
   SIX_IDLE                            = 0x00,   // ready for next event
   // ADD: source
   SIX_SENDING_ADDREQUEST              = 0x01,   // generating LinkRequest packet
   SIX_WAIT_ADDREQUEST_SENDDONE        = 0x02,   // waiting for SendDone confirmation
   SIX_WAIT_ADDRESPONSE                = 0x03,   // waiting for response from the neighbor
   SIX_ADDRESPONSE_RECEIVED            = 0x04,   // I received the link response request command
   // ADD: destinations
   SIX_ADDREQUEST_RECEIVED             = 0x05,   // I received the link request command
   SIX_SENDING_ADDRESPONSE             = 0x06,   // generating resLinkRespone command packet
   SIX_WAIT_ADDRESPONSE_SENDDONE       = 0x07,   // waiting for SendDone confirmation
   // REMOVE: source
   SIX_SENDING_REMOVEREQUEST           = 0x08,   // generating resLinkRespone command packet
   SIX_WAIT_REMOVEREQUEST_SENDDONE     = 0x09,   // waiting for SendDone confirmation
   // REMOVE: destinations
   SIX_REMOVEREQUEST_RECEIVED          = 0x0a    // I received the remove link request command
} six2six_state_t;
       
typedef enum {
   B_TX = 0x00,
   B_RX = 0x01,
} sixtop_blacklist_type_t;

//=========================== typedef =========================================

#define SIX2SIX_TIMEOUT_MS 5000
#define MAXBLACKLISTLENGTH 20
#define MAXLIFETIME        5  // in seconds

//=========================== module variables ================================

typedef struct {
   uint16_t             periodMaintenance;
   bool                 busySendingKA;           // TRUE when busy sending a keep-alive
   bool                 busySendingEB;           // TRUE when busy sending an enhanced beacon
   uint8_t              dsn;                     // current data sequence number
   uint8_t              mgtTaskCounter;          // counter to determine what management task to do
   opentimer_id_t       maintenanceTimerId;
   opentimer_id_t       timeoutTimerId;          // TimeOut timer id
   uint16_t             kaPeriod;                // period of sending KA
   six2six_state_t      six2six_state;
   uint8_t              commandID;
} sixtop_vars_t;

typedef struct {
    bool used;
    uint16_t slotoffset;    // slotoffset of cell which can't be used
    uint8_t  channeloffset;  // channeloffset of cell wihch can't be used, if value=0xff, all channels (11~26) can'e be used
    uint8_t  lifetime;    // the life time of cell in blacklist (second)
} sixtop_blacklist_element_vars_t;

typedef struct {
    sixtop_blacklist_element_vars_t blacklistTx[MAXBLACKLISTLENGTH];
    sixtop_blacklist_element_vars_t blacklistRx[MAXBLACKLISTLENGTH];
} sixtop_blacklist_vars_t;

//=========================== prototypes ======================================

// admin
void      sixtop_init(void);
void      sixtop_setKaPeriod(uint16_t kaPeriod);
// scheduling
void      sixtop_addCells(open_addr_t* neighbor, uint16_t numCells);
void      sixtop_removeCell(open_addr_t*  neighbor);
void      sixtop_removeCellByInfo(open_addr_t*  neighbor,cellInfo_ht* cellInfo);
bool      sixtop_isBlacklisted(uint16_t tsNum, uint16_t choffset, sixtop_blacklist_type_t type);
// from upper layer
owerror_t sixtop_send(OpenQueueEntry_t *msg);
// from lower layer
void      task_sixtopNotifSendDone(void);
void      task_sixtopNotifReceive(void);
// debugging
bool      debugPrint_myDAGrank(void);
bool      debugPrint_kaPeriod(void);
bool      debugPrint_blacklist(void);
// sixtop blacklist
void      sixtop_markBlacklist(uint16_t slotOffset, uint16_t channelOffset, sixtop_blacklist_type_t type);
uint8_t   sixtop_getSix2sixState();

/**
\}
\}
*/

#endif
