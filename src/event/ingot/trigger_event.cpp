#include "include/event/ingot/trigger_event.h"

namespace forgerib {

void SetupInput(TTree *tree, TriggerEvent &event, const std::string &prefix) {
	tree->SetBranchAddress((prefix+"flag").c_str(), &event.flag);
	tree->SetBranchAddress((prefix+"valid").c_str(), event.valid);
	tree->SetBranchAddress((prefix+"time").c_str(), event.time);
	tree->SetBranchAddress((prefix+"external_time").c_str(), event.external_time);
	tree->SetBranchAddress((prefix+"vme_entry").c_str(), &event.vme_entry);
}

void SetupOutput(TTree *tree, TriggerEvent &event) {
	tree->Branch("flag", &event.flag, "flag/I");
	tree->Branch("valid", event.valid, "valid[7]/O");
	tree->Branch("time", event.time, "time[7]/D");
	tree->Branch("external_time", event.external_time, "ets[7]/L");
	tree->Branch("vme_entry", &event.vme_entry, "ve/L");
}

void Reset(TriggerEvent &event) {
	event.flag = 0;
	event.vme_entry = -1;
	for (int i = 0; i < 7; ++i) {
		event.valid[i] = false;
		event.time[i] = 0.0;
		event.external_time[i] = 0;
	}
}

}
