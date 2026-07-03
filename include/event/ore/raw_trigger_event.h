#ifndef __RAW_TRIGGER_EVENT_H__
#define __RAW_TRIGGER_EVENT_H__

#include <string>
#include <TTree.h>

namespace forgerib {

constexpr int kTriggerBeam = 0;
constexpr int kTriggerVme = 1;
constexpr int kTriggerT0D1 = 2;
constexpr int kTriggerT0D2 = 3;
constexpr int kTriggerT0S = 4;
constexpr int kTriggerMain = 5;
constexpr int kTriggerPPAC3 = 6;

struct RawTriggerEvent {
	int type;
	double time;
	long long external_time;
	bool cv;
};

void SetupInput(TTree* tree, RawTriggerEvent &event, const std::string &prefix = "");

void SetupOutput(TTree* tree, RawTriggerEvent &event);

}

#endif