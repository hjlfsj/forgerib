#pragma once

#include <string>

#include <TTree.h>

namespace forgerib {

constexpr int kDssdMaxHits = 32;

struct DssdEvent {
	int front_num;
	int front_strip[kDssdMaxHits];
	double front_energy[kDssdMaxHits];
	double front_time[kDssdMaxHits];
	int back_num;
	int back_strip[kDssdMaxHits];
	double back_energy[kDssdMaxHits];
	double back_time[kDssdMaxHits];
};

void SetupInput(TTree *tree, DssdEvent &event, const std::string &prefix = "");

void SetupOutput(TTree *tree, DssdEvent &event);

void Reset(DssdEvent &event);

void Update(DssdEvent &event, bool front, int strip, double energy, double time);

} // namespace forgerib