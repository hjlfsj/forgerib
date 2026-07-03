#include "include/event/ore/raw_ssd_event.h"

namespace forgerib {

void SetupInput(
	TTree *tree,
	RawSiliconEvent &event,
	const std::string &prefix
) {
	tree->SetBranchAddress((prefix+"energy").c_str(), &event.energy);
	tree->SetBranchAddress((prefix+"time").c_str(), &event.time);
	tree->SetBranchAddress((prefix+"cv").c_str(), &event.cv);
}

void SetupOutput(TTree *tree, RawSiliconEvent &event) {
	tree->Branch("energy", &event.energy, "e/I");
	tree->Branch("time", &event.time, "t/D");
	tree->Branch("cv", &event.cv, "cv/O");
}


void SetupInput_trace(
	TTree *tree,
	Rawssd_with_trace_Event &event,
	const std::string &prefix
) {
	tree->SetBranchAddress((prefix+"energy").c_str(), &event.energy);
	tree->SetBranchAddress((prefix+"time").c_str(), &event.time);
	tree->SetBranchAddress((prefix+"cv").c_str(), &event.cv);
	tree->SetBranchAddress((prefix+"length").c_str(), &event.length);
	tree->SetBranchAddress((prefix+"samples").c_str(), event.samples);
	tree->SetBranchAddress((prefix+"out_of_range").c_str(), &event.out_of_range);
}

void SetupOutput_trace(TTree *tree, Rawssd_with_trace_Event &event) {
	tree->Branch("energy", &event.energy, "e/I");
	tree->Branch("time", &event.time, "t/D");
	tree->Branch("cv", &event.cv, "cv/O");
	tree->Branch("length", &event.length, "l/I");
	tree->Branch("samples", event.samples, "samples[200]/s");
	tree->Branch("out_of_range", &event.out_of_range, "out_of_range/O");
}
}
