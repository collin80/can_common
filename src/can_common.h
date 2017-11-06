#ifndef _CAN_COMMON_
#define _CAN_COMMON_

#include <Arduino.h>

/** Define the typical baudrate for CAN communication. */
#ifdef CAN_BPS_500K
#undef CAN_BPS_1000K 
#undef CAN_BPS_800K                 
#undef CAN_BPS_500K                 
#undef CAN_BPS_250K                 
#undef CAN_BPS_125K                  
#undef CAN_BPS_50K                   
#undef CAN_BPS_33333				  
#undef CAN_BPS_25K                   
#undef CAN_BPS_10K                   
#undef CAN_BPS_5K                   
#endif

#define CAN_BPS_1000K	1000000
#define CAN_BPS_800K	800000
#define CAN_BPS_500K	500000
#define CAN_BPS_250K	250000
#define CAN_BPS_125K	125000
#define CAN_BPS_50K		50000
#define CAN_BPS_33333	33333
#define CAN_BPS_25K		25000
#define CAN_BPS_10K		10000
#define CAN_BPS_5K		5000

#define SIZE_LISTENERS	4 //number of classes that can register as listeners with this class
#define CAN_DEFAULT_BAUD	CAN_BPS_250K

//This structure presupposes little endian mode. If you use it on a big endian processor you're going to have a bad time.
typedef union {
  uint64_t value;
	struct {
		uint32_t low;
		uint32_t high;
	};
	struct {
		uint16_t s0;
		uint16_t s1;
		uint16_t s2;
		uint16_t s3;
  };
	uint8_t bytes[8];
	uint8_t byte[8]; //alternate name so you can omit the s if you feel it makes more sense
} BytesUnion;

typedef struct
{
  uint32_t id;		// EID if ide set, SID otherwise
  uint32_t fid;		// family ID
  uint8_t rtr;		// Remote Transmission Request
  uint8_t priority;	// Priority but only important for TX frames and then only for special uses.
  uint8_t extended;	// Extended ID flag
  uint16_t time;      // CAN timer value when mailbox message was received.
  uint8_t length;		// Number of data bytes
  BytesUnion data;	// 64 bits - lots of ways to access it.
} CAN_FRAME;

class CANListener
{
public:
  CANListener();

  virtual void gotFrame(CAN_FRAME *frame, int mailbox);

  void attachMBHandler(uint8_t mailBox);
  void detachMBHandler(uint8_t mailBox);
  void attachGeneralHandler();
  void detachGeneralHandler();
  void initialize();
  bool isCallbackActive(int callback);
  void setNumFilters(int numFilt);

private:
  int callbacksActive; //bitfield letting the code know which callbacks to actually try to use (for object oriented callbacks only)
  int numFilters; //filters, mailboxes, whichever, how many do we have?

};

/*Abstract function that mostly just sets an interface that all descendants must implement */
class CAN_COMMON
{
public:

    CAN_COMMON(int numFilt);

    //Public API that needs to be re-implemented by subclasses
	virtual int _setFilterSpecific(uint8_t mailbox, uint32_t id, uint32_t mask, bool extended) = 0;
    virtual int _setFilter(uint32_t id, uint32_t mask, bool extended) = 0;
	virtual uint32_t init(uint32_t ul_baudrate) = 0;
    virtual uint32_t beginAutoSpeed() = 0;
    virtual uint32_t set_baudrate(uint32_t ul_baudrate) = 0;
    virtual void setListenOnlyMode(bool state) = 0;
	virtual void enable() = 0;
	virtual void disable() = 0;
	virtual bool sendFrame(CAN_FRAME& txFrame) = 0;
	virtual bool rx_avail() = 0;
	virtual uint16_t available() = 0; //like rx_avail but returns the number of waiting frames
	virtual uint32_t get_rx_buff(CAN_FRAME &msg) = 0;

    //Public API common to all subclasses - don't need to be re-implemented
    //wrapper for syntactic sugar reasons
    //note to my dumb self - functions cannot be both virtual and have multiple versions where the parameter list is different
    //you have to pick one or the other.
	inline uint32_t read(CAN_FRAME &msg) { return get_rx_buff(msg); }
    int watchFor(); //allow anything through
	int watchFor(uint32_t id); //allow just this ID through (automatic determination of extended status)
    int watchFor(uint32_t id, uint32_t mask); //allow a range of ids through
    int watchFor(uint32_t id, uint32_t mask, bool ext);
	int watchForRange(uint32_t id1, uint32_t id2); //try to allow the range from id1 to id2 - automatically determine base ID and mask
	uint32_t begin();
	uint32_t begin(uint32_t baudrate);
    uint32_t begin(uint32_t baudrate, uint8_t enPin);
	uint32_t getBusSpeed();
	int setRXFilter(uint8_t mailbox, uint32_t id, uint32_t mask, bool extended);
    int setRXFilter(uint32_t id, uint32_t mask, bool extended);
    boolean attachObj(CANListener *listener);
	boolean detachObj(CANListener *listener);
    void setGeneralCallback( void (*cb)(CAN_FRAME *) );
    void attachCANInterrupt( void (*cb)(CAN_FRAME *) ) {setGeneralCallback(cb);}
	void setCallback(uint8_t mailbox, void (*cb)(CAN_FRAME *));
	void attachCANInterrupt(uint8_t mailBox, void (*cb)(CAN_FRAME *));
	void detachCANInterrupt(uint8_t mailBox);

protected:
	CANListener *listener[SIZE_LISTENERS];
    void (*cbGeneral)(CAN_FRAME *); //general callback if no per-mailbox or per-filter entries matched
    void (*cbCANFrame[16])(CAN_FRAME *); //array of function pointers - disgusting syntax though.
    uint32_t busSpeed;
    int numFilters;
    int enablePin;
};

#endif

