#ifndef __SILICON_EVENT_H__
#define __SILICON_EVENT_H__

#include <string>

#include <TTree.h>

namespace forgerib {

struct SiliconEvent {
	bool valid;
	double time;
	int energy;
};

struct Silicon_trace_Event {
	bool valid;
	double time;
	int energy;
	int length;
	uint16_t samples[200];
	bool out_of_range;
};

void SetupInput(TTree *tree, SiliconEvent &event, const std::string &prefix = "");

void SetupOutput(TTree *tree, SiliconEvent &event);

void SetupOutput_trace(TTree *tree, Silicon_trace_Event &event);

void SetupInput_trace(TTree *tree, Silicon_trace_Event &event, const std::string &prefix = "");

}

#endif