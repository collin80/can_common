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

private:
  int callbacksActive; //bitfield letting the code know which callbacks to actually try to use (for object oriented callbacks only)

};

#endif

