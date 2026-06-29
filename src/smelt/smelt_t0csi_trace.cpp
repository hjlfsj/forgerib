#include "include/smelt/smelt_t0csi.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <TFile.h>
#include <TH1F.h>
#include <TTree.h>

#include "include/event/ingot/csi_event.h"
#include "include/event/ore/raw_csi_event.h"

namespace forgerib {
namespace {

constexpr size_t kSlotNum = 100;

void ResetCsiEvent(Csi_trace_Event &csi) {
	csi.hit_num =0;
}

void UpdateCsiEvent(const RawCsi_trace_Event &raw, Csi_trace_Event &csi) {
	if (raw.index < 0 || raw.index >= 36) return;
	csi.energy[csi.hit_num] = raw.energy;
	csi.index[csi.hit_num] = raw.index;
	csi.time[csi.hit_num] = raw.time;
	for (int i=0; i<1000; i++) {csi.samples[csi.hit_num][i] = raw.samples[i];}
	csi.hit_num += 1;

	}

int SmeltWithTrigger(
	const std::vector<double> &trigger_time,
	const char *path,
	const char *output_path,
	const double window,
	bool report
) {
	// std::cout << "Start." << std::endl;
	// std::cout << trigger_time.size() << std::endl;
	// std::cout << path << "\n" << output_path << std::endl;
	// std::cout << window << ", " << report << "\n";
	// fflush(stdout);
	TFile opf(output_path, "recreate");
	TH1F forge_window("fw", "forge window", 200, -window, window);
	TTree opt("tree", "forged t0csi");
	Csi_trace_Event csi;
	SetupOutput_trace(&opt, csi);

	TFile ipf(path, "read");
	TTree *ipt = (TTree*)ipf.Get("tree");
	if (!ipt) {
		std::cerr << "Error: Get tree from " << path << " failed.\n";
		return -1;
	}
	RawCsi_trace_Event raw;
	SetupInput_trace(ipt, raw);

	std::vector<Csi_trace_Event> slots;
	slots.resize(kSlotNum);
	for (size_t i = 0; i < kSlotNum; ++i) {
		ResetCsiEvent(slots[i]);
	}
	// std::cout << std::hex << slots << ", " << &(slots[1]) << std::endl;
	Csi_trace_Event test;
	size_t tofill_entry = 0;

	long long total = ipt->GetEntriesFast();
	long long last_percentage = 0;
	if (report) {
		printf("Forging t0csi with trigger   0%%");
		fflush(stdout);
	}
	for (long long entry = 0; entry < total; ++entry) {
		if (report && entry * 100ll / total > last_percentage) {
			last_percentage = entry * 100 / total;
			printf("\b\b\b\b%3lld%%", last_percentage);
			fflush(stdout);
		}
		ipt->GetEntry(entry);
		double min_time = 2*window;
		size_t min_time_entry = 0;
		for (
			auto iter = std::lower_bound(
				trigger_time.begin()+tofill_entry,
				trigger_time.end(),
				raw.time - window
			);
			iter != trigger_time.end();
			++iter
		) {
			if (*iter > raw.time + window) break;
			forge_window.Fill(raw.time - *iter);
			if (std::fabs(*iter - raw.time) < min_time) {
				min_time = std::fabs(*iter - raw.time);
				min_time_entry = iter - trigger_time.begin();
			}
		}
		if (min_time > window) continue;
		if (min_time_entry < tofill_entry) continue;
		if (min_time_entry - tofill_entry >= kSlotNum) {
			for (size_t fill = tofill_entry; fill <= min_time_entry-kSlotNum; ++fill) {
				csi = slots[fill%kSlotNum];
				opt.Fill();
				ResetCsiEvent(slots[fill%kSlotNum]);
			}
			tofill_entry = min_time_entry - kSlotNum + 1;
		}
		UpdateCsiEvent(raw, slots[min_time_entry%kSlotNum]);
	}
	for (size_t fill = tofill_entry; fill < trigger_time.size(); ++fill) {
		csi = slots[fill%kSlotNum];
		opt.Fill();
	}
	if (report) printf("\b\b\b\b100%%\n");

	opf.cd();
	forge_window.Write();
	opt.Write();
	ipf.Close();
	opf.Close();
	return 0;
}



}

int SmeltT0Csi_trace(
	const std::vector<double> &trigger_time,
	const char *path,
	const char *output_path,
	const double window,
	bool report)

{
	return SmeltWithTrigger(trigger_time, path, output_path, window, report);
}

}

