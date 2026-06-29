#pragma once

#include <string>

#include <TTree.h>

namespace forgerib {

struct RawCsiEvent {
	int index;
	int energy;
	double time;
	bool cv;
};

struct RawCsi_trace_Event
{
	int index;
	int energy;
	double time;
	bool cv;
	int length;
	uint16_t samples[1000];
	bool out_of_range;
};


void SetupInput(TTree *tree, RawCsiEvent &event, const std::string &prefix = "");

void SetupOutput(TTree *tree, RawCsiEvent &event);

void SetupInput_trace(TTree *tree, RawCsi_trace_Event &event, const std::string &prefix = "");

void SetupOutput_trace(TTree *tree, RawCsi_trace_Event &event);

}
