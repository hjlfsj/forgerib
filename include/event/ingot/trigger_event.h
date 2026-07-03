#ifndef __TRIGGER_EVENT_H__
#define __TRIGGER_EVENT_H__

#include <string>
#include <TTree.h>

namespace forgerib {

struct TriggerEvent {
	int flag;
	bool valid[7];
	double time[7];
	long long external_time[7];
	long long vme_entry;
};

void SetupInput(TTree *tree, TriggerEvent &event, const std::string &prefix = "");
void SetupOutput(TTree *tree, TriggerEvent &event);
void Reset(TriggerEvent &event);
}

#endif
