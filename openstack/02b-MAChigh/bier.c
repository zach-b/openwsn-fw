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
	uint8_t i;
	// Initialize biermap :
	for(i=0; i<BIER_MAX_TRACKS; i++){
		bier_vars.biermap.trackID[i]=0;
	}
	// Initialize bitmap
	switch (idmanager_getMyID(ADDR_64B)->addr_64b[7]) {
	   case 1 :
		   bier_vars.biermap.trackID[0]=1;
		   bier_vars.biermap.biermap_entry[0].length = 2;

		   bier_vars.biermap.biermap_entry[0].bitIndex[0]=0;
		   bier_vars.biermap.biermap_entry[0].bundleID[0]=1;

		   bier_vars.biermap.biermap_entry[0].bitIndex[1]=1;
		   bier_vars.biermap.biermap_entry[0].bundleID[1]=2;
	       break;
	   case 2 :
		   bier_vars.biermap.trackID[0]=1;
		   bier_vars.biermap.biermap_entry[0].length = 2;

		   bier_vars.biermap.biermap_entry[0].bitIndex[0]=2;
		   bier_vars.biermap.biermap_entry[0].bundleID[0]=3;

		   bier_vars.biermap.biermap_entry[0].bitIndex[1]=3;
		   bier_vars.biermap.biermap_entry[0].bundleID[1]=5;
		   break;
	   case 3 :
	   	   bier_vars.biermap.trackID[0]=1;
	   	   bier_vars.biermap.biermap_entry[0].length = 2;

	   	   bier_vars.biermap.biermap_entry[0].bitIndex[0]=2;
	   	   bier_vars.biermap.biermap_entry[0].bundleID[0]=4;

	   	   bier_vars.biermap.biermap_entry[0].bitIndex[1]=4;
	   	   bier_vars.biermap.biermap_entry[0].bundleID[1]=6;
	   	   break;
	   case 4 :
	   	   break;
	}
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

void task_bierNotifEndOfSlotFrame() {
	uint8_t i;

	// delete any pending BIER packet
	openqueue_removeAllOwnedBy(COMPONENT_BIER_TO_IEEE802154E);
	// send received BIER packets up the stack
    for(i=0; i<QUEUELENGTH; i++){
    	if(openqueue_vars.queue[i].owner == COMPONENT_BIER_TO_IPHC){
   			openserial_printInfo(COMPONENT_BIER,
   					ERR_BIER_RECEIVED,
					(errorparameter_t)*openqueue_vars.queue[i].l2_bierBitmap,
					(errorparameter_t)*(openqueue_vars.queue[i].l2_bierBitmap+1));
   			iphc_receive(&openqueue_vars.queue[i]);
    	}
    }
}


// TODO : we assume we have no IE in Bier messages, check that it is valid.
// TODO : handle incoming packets.
void task_bierNotifReceive() {
    OpenQueueEntry_t* msg;
    bool 				 elimination;
    ipv6_header_iht      ipv6_outer_header;
    ipv6_header_iht      ipv6_inner_header;
    uint8_t              page_length;
    uint8_t				 i, j;
    uint8_t				 bitmask;
    biermap_entry_t	     *biermap_entry;

    elimination = FALSE;
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

    // get the packet infos
    memset(&ipv6_outer_header,0,sizeof(ipv6_header_iht));
    memset(&ipv6_inner_header,0,sizeof(ipv6_header_iht));
    iphc_retrieveIPv6Header(msg,&ipv6_outer_header,&ipv6_inner_header,&page_length);
    msg->l2_bierBitmap = ipv6_outer_header.bierBitmap;
    msg->l2_bierBitmapLength = ipv6_outer_header.bierBitmap_length;

    // check if we have already received this message (we assume only one BIER message per (track, time frame)).
    // TODO : do it in a cleaner way I.E not using openqueue_vars...
    for(i=0; i<QUEUELENGTH; i++){
    	if((openqueue_vars.queue[i].owner == COMPONENT_BIER_TO_IEEE802154E || openqueue_vars.queue[i].owner == COMPONENT_BIER_TO_IPHC) &&
    			(&openqueue_vars.queue[i]!=msg) && openqueue_vars.queue[i].l2_trackID == msg->l2_trackID){
    		// should be the same msg, make an AND on the bitmaps.
    		elimination = TRUE;
    		if(msg->l2_bierBitmapLength != openqueue_vars.queue[i].l2_bierBitmapLength){
    			// TODO : error to serial
    		}else{
    			for(j=0; j<openqueue_vars.queue[i].l2_bierBitmapLength; j++){
    				*(openqueue_vars.queue[i].l2_bierBitmap+j) &= *(msg->l2_bierBitmap+j);
    			}
    			if(openqueue_vars.queue[i].owner != COMPONENT_BIER_TO_IPHC){
    				// if in the received msg the bit for this transmission is not set for do not send (this may be the case for bidirectional bits)
    				// find the right bitmapentry :
    				for(j=0; j< BIER_MAX_TRACKS; j++){
    					if(bier_vars.biermap.trackID[j]==msg->l2_trackID){
    						biermap_entry = &bier_vars.biermap.biermap_entry[j];
    						break;
    					}
    				}
    				// find the bitindex associated with bundleID :
    				for (j=0; j<biermap_entry->length; j++){
    					if (biermap_entry->bundleID[j]==openqueue_vars.queue[i].l2_bundleID){
    						// check if the bit is set
    						bitmask = (uint8_t)1 << (7-biermap_entry->bitIndex[j]);
    						if( (*(msg->l2_bierBitmap+ (biermap_entry->bitIndex[j]/8))) & bitmask ){
    							// bit is set, I keep the msg
    							break;
    						}else{
    							// bit is not set, I do not send
    							openqueue_freePacketBuffer(&openqueue_vars.queue[i]);
    							break;
    						}
    					}
    				}
    			}
    		}
    	}
    }

    if (elimination==TRUE){
    	openqueue_freePacketBuffer(msg);
    	return;
    }


    // send to upper layer if I am the destination
    if (idmanager_isMyAddress(&ipv6_inner_header.dest)){
    	msg->owner = COMPONENT_BIER_TO_IPHC;
    	// do not send the packet up before the end of the timeframe
    }else{
    	// set metadata
    	msg->owner        = COMPONENT_BIER;
    	msg->l2_frameType = IEEE154_TYPE_DATA;

    	// set l2-security attributes
    	msg->l2_securityLevel   = IEEE802154_SECURITY_LEVEL;
    	msg->l2_keyIdMode       = IEEE802154_SECURITY_KEYIDMODE;
    	msg->l2_keyIndex        = IEEE802154_SECURITY_K2_KEY_INDEX;
    	bier_send_internal(msg);
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
	uint8_t bitmask;
	biermap_entry_t *biermap_entry;
	OpenQueueEntry_t *dup;

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

	// Handle the bitmap..
	// find the right biermap entry :
	for(i=0; i< BIER_MAX_TRACKS; i++){
		if(bier_vars.biermap.trackID[i]==msg->l2_trackID){
			biermap_entry = &bier_vars.biermap.biermap_entry[i];
			break;
		}
	}


	// transmit for every bit set
	for(i=0; i<biermap_entry->length; i++){
		// if bitindex is not too big
		if(biermap_entry->bitIndex[i] < 8*msg->l2_bierBitmapLength){
			bitmask = (uint8_t)1 << (7-biermap_entry->bitIndex[i]);
			// if bit is set
			if( (*(msg->l2_bierBitmap+ (biermap_entry->bitIndex[i]/8))) & bitmask ){
				// duplicate
				dup = openqueue_getFreePacketBuffer(COMPONENT_BIER);
				if(dup != NULL){
					packetfunctions_duplicatePacket(dup, msg);
					// reset bit before sending
					*(dup->l2_bierBitmap+ (biermap_entry->bitIndex[i]/8)) &= ~bitmask;
					dup->l2_bundleID = biermap_entry->bundleID[i];
					// change owner to IEEE802154E fetches it from queue
					dup->owner  = COMPONENT_BIER_TO_IEEE802154E;

				}else{
					// TODO : error to serial
				}
			}
		}else{
			// TODO : error to serial ?
		}
	}
    openqueue_freePacketBuffer(msg);
    return E_SUCCESS;
}
