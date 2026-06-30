#ifndef __TAFD_EVENT_H__
#define __TAFD_EVENT_H__

#include <string>
#include <TTree.h>

namespace forgerib {

struct TafdEvent {
	int flag;
	bool valid[6];
	double front_energy[6], back_energy[6];
	int front_strip[6], back_strip[6];
	double front_time[6];
};


void SetupInput(TTree *tree, TafdEvent &event, const std::string &prefix = "");

void SetupOutput(TTree *tree, TafdEvent &event);

void Reset(TafdEvent &event);
}

#endif
