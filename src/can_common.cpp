#include <can_common.h>

CAN_FRAME::CAN_FRAME()
{
	id = 0;
	fid = 0;
	rtr = 0;
	priority = 15;
	extended = false;
	timestamp = 0;
	length = 0;
	data.value = 0;
}

CAN_FRAME_FD::CAN_FRAME_FD()
{
	id = 0;
	fid = 0;
	rrs = 0;
	fdMode = 0;
	priority = 15;
	extended = false;
	timestamp = 0;
	length = 0;
	for (int i = 0; i < 8; i++) data.uint64[i] = 0;
}

/*
A base class for things that listen to CAN_COMMON
*/
CANListener::CANListener()
{
}

// Child implementations must be non-blocking, as this function may be called from ISR context
void CANListener::gotFrame(CAN_FRAME& frame)
{
}

// Child implementations must be non-blocking, as this function may be called from ISR context
void CANListener::sentFrame(CAN_FRAME& frame)
{
}

/*
Some functions in CAN_COMMON are declared = 0 which means you HAVE to reimplement them or the compiler will puke. Some functions are optional
and they've got blank or very simple implementations below.
*/

CAN_COMMON::CAN_COMMON(int numFilt)
{
    numFilters = numFilt;
    memset(cbCANFrame, 0, 4 * numFilters);
	memset(cbCANFrameFD, 0, 4 * numFilters);

    cbGeneral = NULL;
	cbGeneralFD = NULL;
    enablePin = 255;
	busSpeed = 0;
	fd_DataSpeed = 0;
	faulted = false;
    rxFault = false;
    txFault = false;
	fdSupported = false;
    for (int i = 0; i < SIZE_LISTENERS; i++) listener[i] = 0;
}

/**
 * \brief Returns whether a serious fault has occurred.
 *
 * \ret  Bool indicating whether interface has any sort of fault or not
 */
bool CAN_COMMON::isFaulted()
{
	return faulted;
}

/**
 * \brief Returns whether a serious reception fault has occurred.
 *
 * \ret  Bool indicating whether interface has a fault in receiving frames
 */
bool CAN_COMMON::hasRXFault()
{
	return rxFault;
}

/**
 * \brief Returns whether a serious frame transmission fault has occurred.
 *
 * \ret  Bool indicating whether interface has a fault in transmitting frames
 */
bool CAN_COMMON::hasTXFault()
{
	return txFault;
}

/**
 * \brief Returns whether this interface supports CANFD or not. 
 *
 * \ret  Bool indicating whether interface can support CANFD or not.
 *
 * \note This is used for outside the class, internally fdSupported is directly used
 */
bool CAN_COMMON::supportsFDMode()
{
	return fdSupported;
}

bool CAN_COMMON::sendFrame(CAN_FRAME& txFrame)
{
	CANListener *thisListener;
	bool		lvalue;

	if ((lvalue = _sendFrame(txFrame)))
	{	// If tx succeeded, let the listeners know
		for (int i = 0; i < SIZE_LISTENERS; i++)
		{
			if (thisListener = listener[i])
			{
				thisListener->sentFrame(txFrame);
			}
		}
	}
	return lvalue;
}

uint32_t CAN_COMMON::get_rx_buff(CAN_FRAME &msg)
{
	CANListener *thisListener;
	uint32_t	lvalue;

	if ((lvalue = _get_rx_buff(msg)))
	{	// If rx succeeded, let the listeners know
		for (int i = 0; i < SIZE_LISTENERS; i++)
		{
			if (thisListener = listener[i])
			{
				thisListener->gotFrame(msg);
			}
		}
	}
	return lvalue;
}

uint32_t CAN_COMMON::begin()
{
	return init(CAN_DEFAULT_BAUD);
}

uint32_t CAN_COMMON::beginFD()
{
	if (fdSupported) return initFD(CAN_DEFAULT_BAUD, CAN_DEFAULT_FD_RATE);
	return 0;
}

uint32_t CAN_COMMON::begin(uint32_t baudrate)
{
	return init(baudrate);
}

uint32_t CAN_COMMON::beginFD(uint32_t baudrate, uint32_t fastRate)
{
	if (fdSupported) return initFD(baudrate, fastRate);
	return 0;
}

uint32_t CAN_COMMON::begin(uint32_t baudrate, uint8_t enPin) 
{
	enablePin = enPin;
    return init(baudrate);
}

uint32_t CAN_COMMON::beginFD(uint32_t baudrate, uint32_t fastRate, uint8_t enPin)
{
	enablePin = enPin;
	if (fdSupported) return initFD(baudrate, fastRate);
	return 0;
}

uint32_t CAN_COMMON::getBusSpeed()
{
	return busSpeed;
}

uint32_t CAN_COMMON::getDataSpeedFD()
{
	if (fdSupported) return fd_DataSpeed;
	return 0;
}

boolean CAN_COMMON::attachListener(CANListener *listener)
{
	for (int i = 0; i < SIZE_LISTENERS; i++)
	{
		if (this->listener[i] == NULL)
		{
			this->listener[i] = listener;
			return true;			
		}
	}
	return false;
}

boolean CAN_COMMON::detachListener(CANListener *listener)
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
 * \param mailbox Which mailbox (0-31) to assign callback to.
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
	cbCANFrame[mailBox] = NULL;
}

void CAN_COMMON::removeCallback()
{
	for (int i = 0; i < numFilters; i++)
	{
		cbCANFrame[i] = NULL;
		cbCANFrameFD[i] = NULL;
	}
}

void CAN_COMMON::removeCallback(uint8_t mailbox)
{
	if (mailbox >= numFilters) return;
	cbCANFrame[mailbox] = NULL;
}

void CAN_COMMON::removeGeneralCallback()
{
	cbGeneral = NULL;
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

//try to put a standard CAN frame into a CAN_FRAME_FD structure
bool CAN_COMMON::canToFD(CAN_FRAME &source, CAN_FRAME_FD &dest)
{
  dest.id = source.id;
  dest.fid = source.fid;
  dest.rrs = source.rtr;
  dest.priority = source.priority;
  dest.extended = source.extended;
  dest.fdMode = false;
  dest.timestamp = source.timestamp;
  dest.length = source.length;
  dest.data.uint64[0] = source.data.uint64;
  return true;
}

//Try to do inverse - turn a CANFD frame into a standard CAN_FRAME struct
bool CAN_COMMON::fdToCan(CAN_FRAME_FD &source, CAN_FRAME &dest)
{
  if (source.length > 8) return false;
  if (source.fdMode > 0) return false;
  dest.id = source.id;
  dest.fid = source.fid;
  dest.rtr = source.rrs;
  dest.priority = source.priority;
  dest.extended = source.extended;
  dest.timestamp = source.timestamp;
  dest.length = source.length;
  dest.data.uint64 = source.data.uint64[0];
  return true;
}

//these next few functions would normally be pure abstract but they're implemented here
//so that not every class needs to implement FD functionality to work.
uint32_t CAN_COMMON::get_rx_buffFD(CAN_FRAME_FD &msg)
{
       return 0;
}
uint32_t CAN_COMMON::set_baudrateFD(uint32_t nominalSpeed, uint32_t dataSpeed)
{
       return 0;
}

bool CAN_COMMON::sendFrameFD(CAN_FRAME_FD& txFrame)
{
       return false;
}

uint32_t CAN_COMMON::initFD(uint32_t nominalRate, uint32_t dataRate)
{
       return 0;
}



void setGeneralCallbackFD( void (*cb)(CAN_FRAME_FD *) ) {}
void setCallbackFD(uint8_t mailbox, void (*cb)(CAN_FRAME_FD *)) {}
void removeGeneralCallbackFD() {}
void removeCallbackFD(uint8_t mailbox) {}
uint32_t set_baudrateFD(uint32_t nominalSpeed, uint32_t dataSpeed) {}
uint32_t initFD(uint32_t nominalRate, uint32_t dataRate) {}