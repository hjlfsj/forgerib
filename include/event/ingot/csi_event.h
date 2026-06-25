#ifndef __CSI_EVENT_H__
#define __CSI_EVENT_H__

#include <string>

#include <TTree.h>

namespace forgerib {

template<int N>
struct CsiEvent {
	unsigned long long flag;
	bool valid[N];
	double time[N];
	int energy[N];
};

template<int N>
void SetupInput(
	TTree *tree,
	CsiEvent<N> &event,
	const std::string &prefix = ""
) {
	tree->SetBranchAddress((prefix+"flag").c_str(), &event.flag);
	tree->SetBranchAddress((prefix+"valid").c_str(), event.valid);
	tree->SetBranchAddress((prefix+"time").c_str(), event.time);
	tree->SetBranchAddress((prefix+"energy").c_str(), event.energy);
}

template<int N>
void SetupOutput(
	TTree *tree,
	CsiEvent<N> &event
) {
	tree->Branch("flag", &event.flag, "flag/l");
	tree->Branch(
		"valid",
		event.valid,
		("v[" + std::to_string(N) + "]/O").c_str()
	);
	tree->Branch(
		"time",
		event.time,
		("t[" + std::to_string(N) + "]/D").c_str()
	);
	tree->Branch(
		"energy",
		event.energy,
		("e[" + std::to_string(N) + "]/I").c_str()
	);
}

template<int N>
void Reset(CsiEvent<N> &event) {
	event.flag = 0;
	for (int i = 0; i < N; ++i) event.valid[i] = false;
}

template<int N>
void Update(
	CsiEvent<N> &event,
	const int index,
	const int energy,
	const double time
) {
	event.flag |= 1ll << index;
	event.valid[index] = true;
	event.energy[index] = energy;
	event.time[index] = time;
}

// with trace
struct Csi_trace_Event {
	// bool valid[21];
	double time[21];
	int energy[21];
	uint16_t samples[21][5000];
	int index[21];
	int hit_num;
};

void SetupInput_trace(TTree *tree, Csi_trace_Event &event, const std::string &prefix = "");

void SetupOutput_trace(TTree *tree, Csi_trace_Event &event);

void Reset_trace(Csi_trace_Event &event);

void Update_trace(Csi_trace_Event &event
					, const int index
					, const int energy
					, const double time
					, const int length
					, const  u_int16_t *samples
					, const bool out_of_range);

} // namespace forgerib

#endif
