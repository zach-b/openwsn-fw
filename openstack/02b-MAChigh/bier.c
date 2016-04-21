#include "opendefs.h"
#include "bier.h"
#include "sixtop.h"
#include "openserial.h"
#include "openqueue.h"
#include "neighbors.h"
#include "IEEE802154E.h"
#include "iphc.h"
#include "otf.h"
#include "packetfunctions.h"
#include "openrandom.h"
#include "debugpins.h"
#include "leds.h"
#include "processIE.h"
#include "IEEE802154.h"
#include "IEEE802154_security.h"
#include "idmanager.h"
#include "schedule.h"

//=========================== variables =======================================

bier_vars_t bier_vars;

//=========================== prototypes ======================================

// send internal
owerror_t     bier_send_internal(OpenQueueEntry_t*    msg);

//=========================== public ==========================================

void bier_init() {
}

//======= from upper layer

owerror_t bier_send(OpenQueueEntry_t *msg) {

	uint8_t i;

	// set metadata
	msg->owner        = COMPONENT_BIER;
	msg->l2_frameType = IEEE154_TYPE_DATA;

	// set l2-security attributes
	msg->l2_securityLevel   = IEEE802154_SECURITY_LEVEL;
	msg->l2_keyIdMode       = IEEE802154_SECURITY_KEYIDMODE;
	msg->l2_keyIndex        = IEEE802154_SECURITY_K2_KEY_INDEX;

	// TODO : set trackID in a clever way (dest address ?). Reset mac dst address to broadcast
	msg->l2_trackID = 1;
	msg->l2_nextORpreviousHop.type = ADDR_64B;
	for (i=0;i<8;i++) {
		msg->l2_nextORpreviousHop.addr_64b[i] = 0xff;
	}

	// TEST :
	msg -> l2_bundleID = 1;

	return bier_send_internal(msg);
}

//======= from lower layer

void task_bierNotifSendDone() {
   OpenQueueEntry_t* msg;
   
   // get recently-sent packet from openqueue
   msg = openqueue_bierGetSentPacket();
   if (msg==NULL) {
      openserial_printCritical(
         COMPONENT_BIER,
         ERR_NO_SENT_PACKET,
         (errorparameter_t)0,
         (errorparameter_t)0
      );
      return;
   }
   
   // take ownership
   msg->owner = COMPONENT_BIER;
   
   // update neighbor statistics
   if (msg->l2_sendDoneError==E_SUCCESS) {
      neighbors_indicateTx(
         &(msg->l2_nextORpreviousHop),
         msg->l2_numTxAttempts,
         TRUE,
         &msg->l2_asn
      );
   } else {
      neighbors_indicateTx(
         &(msg->l2_nextORpreviousHop),
         msg->l2_numTxAttempts,
         FALSE,
         &msg->l2_asn
      );
   }
   
   // send the packet to where it belongs
   switch (msg->creator) {
      
      case COMPONENT_BIER:
         // TODO : check if we have anything to do here
         // discard packets
         openqueue_freePacketBuffer(msg);
         break;
      
      default:
         // send the rest up the stack
         iphc_sendDone(msg,msg->l2_sendDoneError);
         break;
   }
}

// TODO : we assume we have no IE in Bier messages, check that it is valid.
void task_bierNotifReceive() {
    OpenQueueEntry_t* msg;
    // get received packet from openqueue
    msg = openqueue_bierGetReceivedPacket();
    if (msg==NULL) {
        openserial_printCritical(
            COMPONENT_BIER,
            ERR_NO_RECEIVED_PACKET,
            (errorparameter_t)0,
            (errorparameter_t)0
        );
        return;
    }
   
    // take ownership
    msg->owner = COMPONENT_BIER;

    // TODO : change everything

    // update neighbor statistics
    neighbors_indicateRx(
        &(msg->l2_nextORpreviousHop),
        msg->l1_rssi,
        &msg->l2_asn,
        msg->l2_joinPriorityPresent,
        msg->l2_joinPriority
    );
   
    // reset it to avoid race conditions with this var.
    msg->l2_joinPriorityPresent = FALSE; 
   
    // send the packet up the stack, if it qualifies
    switch (msg->l2_frameType) {
    case IEEE154_TYPE_BEACON:
    case IEEE154_TYPE_DATA:
    case IEEE154_TYPE_CMD:
        if (msg->length>0) {
            // send to upper layer
            iphc_receive(msg);
        } else {
            // free up the RAM
            openqueue_freePacketBuffer(msg);
        }
            break;
    case IEEE154_TYPE_ACK:
    default:
        // free the packet's RAM memory
        openqueue_freePacketBuffer(msg);
        // log the error
        openserial_printError(
            COMPONENT_BIER,
            ERR_MSG_UNKNOWN_TYPE,
            (errorparameter_t)msg->l2_frameType,
            (errorparameter_t)0
        );
        break;
    }
}

//=========================== private =========================================

/**
\brief Transfer packet to MAC.

This function adds a IEEE802.15.4 header to the packet and leaves it the 
OpenQueue buffer. The very last thing it does is assigning this packet to the 
virtual component COMPONENT_SIXTOP_TO_IEEE802154E. Whenever it gets a change,
IEEE802154E will handle the packet.

\param[in] msg The packet to the transmitted

\returns E_SUCCESS iff successful.
*/
owerror_t bier_send_internal(OpenQueueEntry_t* msg) {

   // TODO : handle retries
   // assign a number of retries
   msg->l2_retriesLeft = 1;
   // record this packet's dsn (for matching the ACK) TODO : dsn number is in the sixtop vars, check that there is no problem with that...
   msg->l2_dsn = sixtop_vars.dsn++;
   // this is a new packet which I never attempted to send
   msg->l2_numTxAttempts = 0;
   // transmit with the default TX power
   msg->l1_txPower = TX_POWER;
   // add a IEEE802.15.4 header
   ieee802154_prependHeader(msg,
                            msg->l2_frameType,
                            FALSE,
                            msg->l2_dsn,
                            &(msg->l2_nextORpreviousHop)
                            );
   // change owner to IEEE802154E fetches it from queue
   msg->owner  = COMPONENT_BIER_TO_IEEE802154E;
   return E_SUCCESS;
}
