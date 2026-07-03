#include "include/smelt/smelt_ssd.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <TFile.h>
#include <TH1F.h>
#include <TTree.h>

#include "include/event/ingot/silicon_event.h"
#include "include/event/ore/raw_ssd_event.h"

namespace forgerib {

namespace {

constexpr size_t kSlotNum = 1000;

void ResetSiliconEvent(Silicon_trace_Event &silicon) {
	silicon.valid = false;
	silicon.time = 0.0;
	silicon.energy = 0;
	silicon.length = 0;
	silicon.out_of_range = false;
}

void UpdateSiliconEvent(
	const Rawssd_with_trace_Event &raw,
	Silicon_trace_Event &silicon
) {
	if (!silicon.valid || raw.energy > silicon.energy) {
		silicon.valid = true;
		silicon.time = raw.time;
		silicon.energy = raw.energy;
		silicon.length = raw.length;
		for (int i = 0; i < 200; i++) {
			silicon.samples[i] = raw.samples[i];
		}
		silicon.out_of_range = raw.out_of_range;
	}
}

int SmeltWithTrigger(
	const std::vector<double> &trigger_time,
	const char *path,
	const char *output_path,
	const double window,
	bool report
) {
	TFile opf(output_path, "recreate");
	TH1F forge_window("fw", "forge window", 200, -window, window);
	TTree opt("tree", "forged t0s");
	Silicon_trace_Event silicon;
	SetupOutput_trace(&opt, silicon);

	TFile ipf(path, "read");
	TTree *ipt = (TTree*)ipf.Get("tree");
	if (!ipt) {
		std::cerr << "Error: Get tree from " << path << " failed.\n";
		return -1;
	}
	Rawssd_with_trace_Event raw;
	SetupInput_trace(ipt, raw);

	Silicon_trace_Event slots[kSlotNum];
	for (size_t i = 0; i < kSlotNum; ++i) {
		ResetSiliconEvent(slots[i]);
	}
	size_t tofill_entry = 0;

	long long total = ipt->GetEntriesFast();
	long long last_percentage = 0;
	if (report) {
		printf("Forging t0s with trigger   0%%");
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
				silicon = slots[fill%kSlotNum];
				opt.Fill();
				ResetSiliconEvent(slots[fill%kSlotNum]);
			}
			tofill_entry = min_time_entry - kSlotNum + 1;
		}
		UpdateSiliconEvent(raw, slots[min_time_entry%kSlotNum]);
	}
	for (size_t fill = tofill_entry; fill < trigger_time.size(); ++fill) {
		silicon = slots[fill%kSlotNum];
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

int SmeltWithoutTrigger(
	const char *path,
	const char *output_path,
	const double window,
	bool report
) {
	TFile opf(output_path, "recreate");
	TH1F forge_window("fw", "forge window", 200, -window, window);
	TTree opt("tree", "forged t0s");
	Silicon_trace_Event silicon;
	SetupOutput_trace(&opt, silicon);

	TFile ipf(path, "read");
	TTree *ipt = (TTree*)ipf.Get("tree");
	if (!ipt) {
		std::cerr << "Error: Get tree from " << path << " failed.\n";
		return -1;
	}
	Rawssd_with_trace_Event raw;
	SetupInput_trace(ipt, raw);

	ResetSiliconEvent(silicon);
	double ref_time = -1.0;
	long long total_entries = ipt->GetEntriesFast();
	long long last_percentage = 0;
	if (report) {
		printf("Forging t0s without trigger   0%%");
		fflush(stdout);
	}
	for (long long entry = 0; entry < total_entries; ++entry) {
		if (report && entry * 100ll / total_entries > last_percentage) {
			last_percentage = entry * 100ll / total_entries;
			printf("\b\b\b\b%3lld%%", last_percentage);
			fflush(stdout);
		}
		ipt->GetEntry(entry);
		if (!silicon.valid) {
			UpdateSiliconEvent(raw, silicon);
			ref_time = raw.time;
		} else if (raw.time - ref_time < window) {
			forge_window.Fill(raw.time - ref_time);
			UpdateSiliconEvent(raw, silicon);
		} else {
			opt.Fill();
			ResetSiliconEvent(silicon);
			UpdateSiliconEvent(raw, silicon);
			ref_time = raw.time;
		}
	}
	if (silicon.valid) opt.Fill();
	if (report) printf("\b\b\b\b100%%\n");

	opf.cd();
	forge_window.Write();
	opt.Write();
	ipf.Close();
	opf.Close();
	return 0;
}

int SmeltSilicon_trace(
	const std::vector<double> &trigger_time,
	const char *path,
	const char *output_path,
	const double window,
	bool report
) {
	if (trigger_time.empty()) {
		return SmeltWithoutTrigger(path, output_path, window, report);
        // printf("Without trigger\n");
	}
	return SmeltWithTrigger(trigger_time, path, output_path, window, report);
}

} // namespace

int SmeltT0s_trace(
	const std::vector<double> &trigger_time,
	const char *path,
	const char *output_path,
	const double window,
	bool report
) {
	return SmeltSilicon_trace(trigger_time, path, output_path, window, report);
}

}