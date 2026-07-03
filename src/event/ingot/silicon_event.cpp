#include "include/event/ingot/silicon_event.h"

namespace forgerib {

void SetupInput(TTree *tree, SiliconEvent &event, const std::string &prefix) {
	tree->SetBranchAddress((prefix+"valid").c_str(), &event.valid);
	tree->SetBranchAddress((prefix+"time").c_str(), &event.time);
	tree->SetBranchAddress((prefix+"energy").c_str(), &event.energy);
}

void SetupOutput(TTree *tree, SiliconEvent &event) {
	tree->Branch("valid", &event.valid, "valid/O");
	tree->Branch("time", &event.time, "time/D");
	tree->Branch("energy", &event.energy, "energy/I");
}

void SetupOutput_trace(TTree *tree, Silicon_trace_Event &event) {
	tree->Branch("valid", &event.valid, "valid/O");
	tree->Branch("time", &event.time, "time/D");
	tree->Branch("energy", &event.energy, "energy/I");
	tree->Branch("length", &event.length, "length/I");
	tree->Branch("samples", event.samples, "samples[200]/s");
	tree->Branch("out_of_range", &event.out_of_range, "out_of_range/O");
}

void SetupInput_trace(TTree *tree, Silicon_trace_Event &event, const std::string &prefix) {
	tree->SetBranchAddress((prefix+"valid").c_str(), &event.valid);
	tree->SetBranchAddress((prefix+"time").c_str(), &event.time);
	tree->SetBranchAddress((prefix+"energy").c_str(), &event.energy);
	tree->SetBranchAddress((prefix+"length").c_str(), &event.length);
	tree->SetBranchAddress((prefix+"samples").c_str(), event.samples);
	tree->SetBranchAddress((prefix+"out_of_range").c_str(), &event.out_of_range);
}

}