#pragma once

#include <string>

#include <TTree.h>

namespace forgerib {

struct RawSiliconEvent {
	int energy;
	double time;
	bool cv;
};


struct Rawssd_with_trace_Event
{
	int energy;
	double time;
	bool cv;
	int length;
	uint16_t samples[200];
	bool out_of_range;
};


void SetupInput(
	TTree *tree,
	RawSiliconEvent &event,
	const std::string &prefix = ""
);

void SetupOutput(TTree *tree, RawSiliconEvent &event);


void SetupInput_trace(
	TTree *tree,
	Rawssd_with_trace_Event &event,
	const std::string &prefix = ""
);

void SetupOutput_trace(TTree *tree, Rawssd_with_trace_Event &event);
}
