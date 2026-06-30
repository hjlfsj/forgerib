#include "include/event/ingot/tafd_event.h"

namespace forgerib {

void SetupInput(TTree *tree, TafdEvent &event, const std::string &prefix) {
	tree->SetBranchAddress((prefix+"flag").c_str(), &event.flag);
	tree->SetBranchAddress((prefix+"valid").c_str(), event.valid);
	tree->SetBranchAddress((prefix+"front_energy").c_str(), event.front_energy);
	tree->SetBranchAddress((prefix+"back_energy").c_str(), event.back_energy);
	tree->SetBranchAddress((prefix+"front_strip").c_str(), event.front_strip);
	tree->SetBranchAddress((prefix+"back_strip").c_str(), event.back_strip);
	tree->SetBranchAddress((prefix+"front_time").c_str(), event.front_time);
}

void SetupOutput(TTree *tree, TafdEvent &event) {
	tree->Branch("flag", &event.flag, "flag/I");
	tree->Branch("valid", event.valid, "v[6]/O");
	tree->Branch("front_energy", event.front_energy, "fe[6]/D");
	tree->Branch("back_energy", event.back_energy, "be[6]/D");
	tree->Branch("front_strip", event.front_strip, "fs[6]/I");
	tree->Branch("back_strip", event.back_strip, "bs[6]/I");
	tree->Branch("front_time", event.front_time, "ft[6]/D");
}

void Reset(TafdEvent &event) {
	event.flag = 0;
	for (int i = 0; i < 6; ++i) event.valid[i] = false;
}

}
