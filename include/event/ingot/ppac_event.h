#ifndef __PPAC_EVENT_H__
#define __PPAC_EVENT_H__

#include <string>
#include <TTree.h>

namespace forgerib {

struct PpacEvent {
	int flag;
	bool valid[15];
	double time[15];
	int energy[15];
	// bool cv[15];
};

void SetupInput(TTree *tree, PpacEvent &event, const std::string &prefix="");

void SetupOutput(TTree *tree, PpacEvent &event);

void Reset(PpacEvent &event);

}

#endif