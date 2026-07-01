#ifndef __DECODE_EVENT_H__
#define __DECODE_EVENT_H__

#include <stdint.h>
#include <unistd.h>

namespace forgerib {

struct DecodeEvent {
	int module;
	int channel;
	int energy;
	double time;
	int64_t timestamp;
	double cfd;
	bool cfd_valid;
	bool used;
};

struct DecodeTraces {
	size_t length;
	uint16_t samples[1000];
	bool out_of_range;
};

}

#endif // __DECODE_EVENT_