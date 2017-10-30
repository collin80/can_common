#include <can_common.h>

CANListener::CANListener()
{
	callbacksActive = 0; //none. Bitfield were bits 0-7 are the mailboxes and bit 8 is the general callback
}

//an empty version so that the linker doesn't complain that no implementation exists.
void CANListener::gotFrame(CAN_FRAME */*frame*/, int /*mailbox*/)
{
  
}

void CANListener::attachMBHandler(uint8_t mailBox)
{
	if ( mailBox < CANMB_NUMBER )
	{
		callbacksActive |= (1<<mailBox);
	}
}

void CANListener::detachMBHandler(uint8_t mailBox)
{
	if ( mailBox < CANMB_NUMBER )
	{
		callbacksActive &= ~(1<<mailBox);
	}  
}

void CANListener::attachGeneralHandler()
{
	callbacksActive |= 256;
}

void CANListener::detachGeneralHandler()
{
	callbacksActive &= ~256;
}

void CANListener::initialize()
{
   callbacksActive = 0;
}

bool CANListener::isCallbackActive(int callback)
{
    return (callbacksActive & (1 << callback))?true:false;
}
