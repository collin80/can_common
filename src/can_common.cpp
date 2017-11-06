#include <can_common.h>

CANListener::CANListener()
{
	callbacksActive = 0; //none. Bitfield were bits 0-7 are the mailboxes and bit 8 is the general callback
    numFilters = 8;
}

//an empty version so that the linker doesn't complain that no implementation exists.
void CANListener::gotFrame(CAN_FRAME */*frame*/, int /*mailbox*/)
{
  
}

void CANListener::attachMBHandler(uint8_t mailBox)
{
	if ( mailBox < numFilters )
	{
		callbacksActive |= (1<<mailBox);
	}
}

void CANListener::detachMBHandler(uint8_t mailBox)
{
	if ( mailBox < numFilters )
	{
		callbacksActive &= ~(1<<mailBox);
	}  
}

void CANListener::attachGeneralHandler()
{
	callbacksActive |= (1 << numFilters);
}

void CANListener::detachGeneralHandler()
{
	callbacksActive &= ~(1 << numFilters);
}

void CANListener::initialize()
{
   callbacksActive = 0;
}

bool CANListener::isCallbackActive(int callback)
{
    return (callbacksActive & (1 << callback))?true:false;
}



/*
Some functions in CAN_COMMON are declared = 0 which means you HAVE to reimplement them or the compiler will puke. Some functions are optional
and they've got blank or very simple implementations below.
*/

CAN_COMMON::CAN_COMMON(int numFilt)
{
    numFilters = numFilt;
    //Official entry for the worst, most convoluted looking C++ line ever written.
    //Dynamically allocate enough space for the function pointers with a hideous malloc call.
    //cbCANFrame = malloc(4 * numFilters);
    memset(cbCANFrame, 0, 4 * numFilters);
    cbGeneral = 0;
    enablePin = 255;
    for (int i = 0; i < SIZE_LISTENERS; i++) listener[i] = 0;
}

uint32_t CAN_COMMON::begin()
{
	return init(CAN_DEFAULT_BAUD);
}

uint32_t CAN_COMMON::begin(uint32_t baudrate)
{
	return init(baudrate);
}

uint32_t CAN_COMMON::begin(uint32_t baudrate, uint8_t enPin) 
{
	enablePin = enPin;
    return init(baudrate);
}

uint32_t CAN_COMMON::getBusSpeed()
{
	return busSpeed;
}

boolean CAN_COMMON::attachObj(CANListener *listener)
{
	for (int i = 0; i < SIZE_LISTENERS; i++)
	{
		if (this->listener[i] == NULL)
		{
			this->listener[i] = listener;
			listener->initialize();
			return true;			
		}
	}
	return false;
}

boolean CAN_COMMON::detachObj(CANListener *listener)
{
	for (int i = 0; i < SIZE_LISTENERS; i++)
	{
		if (this->listener[i] == listener)
		{
			this->listener[i] = NULL;			
			return true;			
		}
	}
	return false;  
}

/**
 * \brief Set up a general callback that will be used if no callback was registered for receiving mailbox
 *
 * \param cb A function pointer to a function with prototype "void functionname(CAN_FRAME *frame);"
 *
 * \note If this function is used to set up a callback then no buffering of frames will ever take place.
 */
void CAN_COMMON::setGeneralCallback(void (*cb)(CAN_FRAME *))
{
	cbGeneral = cb;
}

/**
 * \brief Set up a callback function for given mailbox
 *
 * \param mailbox Which mailbox (0-7) to assign callback to.
 * \param cb A function pointer to a function with prototype "void functionname(CAN_FRAME *frame);"
 *
 */
void CAN_COMMON::setCallback(uint8_t mailbox, void (*cb)(CAN_FRAME *))
{
	if ( mailbox >= numFilters ) return;
	cbCANFrame[mailbox] = cb;
}

void CAN_COMMON::attachCANInterrupt(uint8_t mailBox, void (*cb)(CAN_FRAME *)) 
{
	setCallback(mailBox, cb);
}

void CAN_COMMON::detachCANInterrupt(uint8_t mailBox)
{
	if ( mailBox >= numFilters ) return;
	cbCANFrame[mailBox] = 0;
}

int CAN_COMMON::setRXFilter(uint8_t mailbox, uint32_t id, uint32_t mask, bool extended)
{
    _setFilterSpecific(mailbox, id, mask, extended);
}

int CAN_COMMON::setRXFilter(uint32_t id, uint32_t mask, bool extended)
{
    _setFilter(id, mask, extended);
}

int CAN_COMMON::watchFor() 
{
    setRXFilter(0, 0, false);
	return setRXFilter(0, 0, true);

}

//Let a single frame ID through. Automatic determination of extended. Also automatically sets mask
int CAN_COMMON::watchFor(uint32_t id)
{
	if (id > 0x7FF) return setRXFilter(id, 0x1FFFFFFF, true);
	else return setRXFilter(id, 0x7FF, false);
}

//Allow a range of IDs through based on mask. Still auto determines extended.
int CAN_COMMON::watchFor(uint32_t id, uint32_t mask)
{
	if (id > 0x7FF) return setRXFilter(id, mask, true);
	else return setRXFilter(id, mask, false);
}

//A bit more complicated. Makes sure that the range from id1 to id2 is let through. This might open
//the floodgates if you aren't careful.
//There are undoubtedly better ways to calculate the proper values for the filter but this way seems to work.
//It'll be kind of slow if you try to let a huge span through though.
int CAN_COMMON::watchForRange(uint32_t id1, uint32_t id2)
{
	uint32_t id = 0;
	uint32_t mask = 0;
	uint32_t temp;

	if (id1 > id2) 
	{   //looks funny I know. In place swap with no temporary storage. Neato!
		id1 = id1 ^ id2;
		id2 = id1 ^ id2; //note difference here.
		id1 = id1 ^ id2;
	}

	id = id1;

	if (id2 <= 0x7FF) mask = 0x7FF;
	else mask = 0x1FFFFFFF;

	/* Here is a quick overview of the theory behind these calculations.
	   We start with mask set to 11 or 29 set bits (all 1's)
	   and id set to the lowest ID in the range.
	   From there we go through every single ID possible in the range. For each ID
	   we AND with the current ID. At the end only bits that never changed and were 1's
	   will still be 1's. This yields the ID we can match against to let these frames through
	   The mask is calculated by finding the bitfield difference between the lowest ID and
	   the current ID. This calculation will be 1 anywhere the bits were different. We invert
	   this so that it is 1 anywhere the bits where the same. Then we AND with the current Mask.
	   At the end the mask will be 1 anywhere the bits never changed. This is the perfect mask.
	*/
	for (uint32_t c = id1; c <= id2; c++)
	{
		id &= c;
		temp = (~(id1 ^ c)) & 0x1FFFFFFF;
		mask &= temp;
	}
	//output of the above crazy loop is actually the end result.
	if (id > 0x7FF) return setRXFilter(id, mask, true);
	else return setRXFilter(id, mask, false);
}

