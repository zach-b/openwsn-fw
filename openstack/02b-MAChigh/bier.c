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
#include "iphc.h"

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
	// set metadata
	msg->owner        = COMPONENT_BIER;
	msg->l2_frameType = IEEE154_TYPE_DATA;

	// set l2-security attributes
	msg->l2_securityLevel   = IEEE802154_SECURITY_LEVEL;
	msg->l2_keyIdMode       = IEEE802154_SECURITY_KEYIDMODE;
	msg->l2_keyIndex        = IEEE802154_SECURITY_K2_KEY_INDEX;

	// TODO : set trackID in a clever way (dest address ?)
	msg->l2_trackID = 1;

	return bier_send_internal(msg);
}

//======= from lower layer

void task_bierNotifEndOfSlotFrame() {
	OpenQueueEntry_t *msg;

	// notify upper layers that BIER packets transmission succeeded (always...) :
	msg = openqueue_bierGetSentPacket();
	while (msg!=NULL){
		iphc_sendDone(msg, E_SUCCESS);
		msg = openqueue_bierGetSentPacket();
	}
	// delete any pending BIER packet
	openqueue_removeAllOwnedBy(COMPONENT_BIER_TO_IEEE802154E);
	// reset all bierDoNotSend
	schedule_resetBierDoNotSend();
	// send received BIER packets up the stack
	msg = openqueue_bierGetPacketToSendUp();
	while(msg!=NULL){
		if(msg->l2_bierBitmapLength>1){
			openserial_printInfo(COMPONENT_BIER,
					ERR_BIER_RECEIVED,
					(errorparameter_t)*msg->l2_bierBitmap,
					(errorparameter_t)*(msg->l2_bierBitmap+1));
		} else{
			openserial_printInfo(COMPONENT_BIER,
					ERR_BIER_RECEIVED,
					(errorparameter_t)*msg->l2_bierBitmap,
					(errorparameter_t)0);
		}
		iphc_receive(msg);
		msg = openqueue_bierGetPacketToSendUp();
	}
}


void task_bierNotifReceive() {
    OpenQueueEntry_t*    msg;
    OpenQueueEntry_t*    prevmsg;
    ipv6_header_iht      ipv6_outer_header;
    ipv6_header_iht      ipv6_inner_header;
    uint8_t              page_length;
    uint8_t 			 i;

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
    if (msg->l2_frameType != IEEE154_TYPE_DATA){
    	openserial_printError(
    			COMPONENT_BIER,
				ERR_MSG_UNKNOWN_TYPE,
				(errorparameter_t)msg->l2_frameType,
				(errorparameter_t)0
    	);
    	// free the packet's RAM memory
    	openqueue_freePacketBuffer(msg);
    }
    // take ownership
    msg->owner = COMPONENT_BIER;
    msg->creator = COMPONENT_BIER;

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

    // get the packet infos
    memset(&ipv6_outer_header,0,sizeof(ipv6_header_iht));
    memset(&ipv6_inner_header,0,sizeof(ipv6_header_iht));
    iphc_retrieveIPv6Header(msg,&ipv6_outer_header,&ipv6_inner_header,&page_length);
    msg->l2_bierBitmap = ipv6_outer_header.bierBitmap;
    msg->l2_bierBitmapLength = ipv6_outer_header.bierBitmap_length;

    // check for an existing message :
    prevmsg = openqueue_bierGetDataPacketTrack(msg->l2_trackID);
    if(prevmsg==NULL){
    	// this is the first time I receive this message
    	// send to upper layer if I am the destination
    	if (idmanager_isMyAddress(&ipv6_inner_header.dest)){
    	 	msg->owner = COMPONENT_BIER_TO_IPHC;
    	  	// do not send the packet up before the end of the timeframe
    	}else{
    		// forward the BIER packet
    	  	// set metadata
    	   	msg->owner        = COMPONENT_BIER;
    	   	msg->l2_frameType = IEEE154_TYPE_DATA;
   	    	// set l2-security attributes
  	    	msg->l2_securityLevel   = IEEE802154_SECURITY_LEVEL;
   	    	msg->l2_keyIdMode       = IEEE802154_SECURITY_KEYIDMODE;
   	    	msg->l2_keyIndex        = IEEE802154_SECURITY_K2_KEY_INDEX;
   	    	bier_send_internal(msg);
   	    }
    } else{
    	// I have received this message before, check that they are the same, make an AND on the bitmap and delete it
    	// TODO : check that they are the same
    	if (msg->l2_bierBitmapLength == prevmsg->l2_bierBitmapLength){
        	// AND on the bitmap :
    		for(i=0; i<msg->l2_bierBitmapLength; i++){
    		    *(prevmsg->l2_bierBitmap+i) &= *(msg->l2_bierBitmap+i);
    		}
    	} else{
    		openserial_printError(
    			COMPONENT_BIER,
				ERR_BIER_DIFFERENT_MSG,
				(errorparameter_t)msg->l2_trackID,
				(errorparameter_t)0
    		);
    	}
		// delete :
		openqueue_freePacketBuffer(msg);
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
	uint8_t i;

	// Set mac dst address to broadcast
	msg->l2_nextORpreviousHop.type = ADDR_64B;
	for (i=0;i<8;i++) {
		msg->l2_nextORpreviousHop.addr_64b[i] = 0xff;
	}

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
	msg->owner  = COMPONENT_BIER_TO_IEEE802154E;
    return E_SUCCESS;
}

bool bier_macIsBitSet(OpenQueueEntry_t* msg, uint8_t bitindex){
	uint8_t bitmask;

	bitmask = (uint8_t)1 << (7-schedule_getBitIndex()%8);
	return (*(msg->l2_bierBitmap + bitindex/8) & bitmask)!=0;
}

void bier_macSetBit(OpenQueueEntry_t* msg, uint8_t bitindex){
	uint8_t bitmask;

	bitmask = (uint8_t)1 << (7-schedule_getBitIndex()%8);
	*(msg->l2_bierBitmap + bitindex/8) |= bitmask;
}

void bier_macResetBit(OpenQueueEntry_t* msg, uint8_t bitindex){
	uint8_t bitmask;

	bitmask = (uint8_t)1 << (7-schedule_getBitIndex()%8);
	*(msg->l2_bierBitmap + bitindex/8) &= ~bitmask;
}
