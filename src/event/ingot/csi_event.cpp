#include "include/event/ingot/csi_event.h"

namespace forgerib {

void SetupInput_trace(TTree *tree, Csi_trace_Event &event, const std::string &prefix) {
	tree->SetBranchAddress((prefix+"time").c_str(), event.time);
	tree->SetBranchAddress((prefix+"energy").c_str(), event.energy);
	tree->SetBranchAddress((prefix+"samples").c_str(), event.samples);
	tree->SetBranchAddress((prefix+"hit_num").c_str(), &event.hit_num);
	tree->SetBranchAddress((prefix+"index").c_str(), event.index);
}

void SetupOutput_trace(TTree *tree, Csi_trace_Event &event) {
	tree->Branch("hit_num", &event.hit_num, "hit_num/I");
	tree->Branch("time", event.time, "t[hit_num]/D");
	tree->Branch("energy", event.energy, "e[hit_num]/I");
	tree->Branch("samples", event.samples, "samples[hit_num][5000]/s");
	tree->Branch("index", event.index, "index[hit_num]/I");
}

void Reset_trace(Csi_trace_Event &event) {
	event.hit_num = 0;
}


} // namespace forgerib