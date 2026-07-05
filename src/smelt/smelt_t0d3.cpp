#include "include/smelt/smelt_t0d3.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <TFile.h>
#include <TH1F.h>
#include <TTree.h>

#include "include/event/ingot/dssd_event.h"
#include "include/event/ore/raw_dssd_event.h"

namespace forgerib {

namespace {

constexpr size_t kSlotNum = 1000;
constexpr int T0D3_Front_Threshold = 150;
constexpr int T0D3_Back_Threshold = 150;

void ResetDssdEvent(DssdEvent &dssd) {
	dssd.front_num = 0;
	dssd.back_num = 0;
}

void InsertHit(
	int strip,
	int energy,
	double time,
	int &num,
	int strips[kDssdMaxHits],
	double energies[kDssdMaxHits],
	double times[kDssdMaxHits]
) {
	int pos = 0;
	while (pos < num && energies[pos] >= energy) ++pos;
	if (pos >= kDssdMaxHits) return;
	int limit = std::min(num, kDssdMaxHits-1);
	for (int i = limit; i > pos; --i) {
		strips[i] = strips[i-1];
		energies[i] = energies[i-1];
		times[i] = times[i-1];
	}
	strips[pos] = strip;
	energies[pos] = energy;
	times[pos] = time;
	if (num < kDssdMaxHits) ++num;
}

void FillHit(const RawDssdEvent &raw, DssdEvent &dssd) {
	if (raw.side == kDssdSideFront) {
		if (raw.energy < T0D3_Front_Threshold) return;
		InsertHit(raw.strip, raw.energy, raw.time, dssd.front_num, dssd.front_strip, dssd.front_energy, dssd.front_time);
	} else if (raw.side == kDssdSideBack) {
		if (raw.energy < T0D3_Back_Threshold) return;
		InsertHit(raw.strip, raw.energy, raw.time, dssd.back_num, dssd.back_strip, dssd.back_energy, dssd.back_time);
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
	TTree opt("tree", "forged t0d3");
	DssdEvent dssd;
	SetupOutput(&opt, dssd);

	TFile ipf(path, "read");
	TTree *ipt = (TTree*)ipf.Get("tree");
	if (!ipt) {
		std::cerr << "Error: Get tree from " << path << " failed.\n";
		return -1;
	}
	RawDssdEvent raw;
	SetupInput(ipt, raw);

	DssdEvent slots[kSlotNum];
	for (size_t i = 0; i < kSlotNum; ++i) {
		ResetDssdEvent(slots[i]);
	}
	size_t tofill_entry = 0;

	long long total = ipt->GetEntriesFast();
	long long last_percentage = 0;
	if (report) {
		printf("Forging T0D3 with trigger   0%%");
		fflush(stdout);
	}
	for (long long entry = 0; entry < total; ++entry) {
		if (report && entry * 100ll / total > last_percentage) {
			last_percentage = entry * 100 / total;
			printf("\b\b\b\b%3lld%%", last_percentage);
			fflush(stdout);
		}
		ipt->GetEntry(entry);
		if (raw.time > 1e15) continue;
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
		// printf(
		// 	"Side: %d, strip: %d, time: %f, entry %ld, tofill %ld, slot_fn %d, slot_bn %d\n",
		// 	raw.side, raw.strip, raw.time, min_time_entry, tofill_entry,
		// 	slots[min_time_entry%kSlotNum].front_num, slots[min_time_entry%kSlotNum].back_num
		// );
		if (min_time_entry < tofill_entry) continue;
		if (min_time_entry - tofill_entry >= kSlotNum) {
			for (size_t fill = tofill_entry; fill <= min_time_entry-kSlotNum; ++fill) {
				dssd = slots[fill%kSlotNum];
				opt.Fill();
				ResetDssdEvent(slots[fill%kSlotNum]);
			}
			tofill_entry = min_time_entry - kSlotNum + 1;
		}
		FillHit(raw, slots[min_time_entry%kSlotNum]);
	}
	for (size_t fill = tofill_entry; fill < trigger_time.size(); ++fill) {
		dssd = slots[fill%kSlotNum];
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
	TTree opt("tree", "forged t0d3");
	DssdEvent dssd;
	SetupOutput(&opt, dssd);

	TFile ipf(path, "read");
	TTree *ipt = (TTree*)ipf.Get("tree");
	if (!ipt) {
		std::cerr << "Error: Get tree from " << path << " failed.\n";
		return -1;
	}
	RawDssdEvent raw;
	SetupInput(ipt, raw);

	ResetDssdEvent(dssd);
	double ref_time = -1.0;
	long long total_entries = ipt->GetEntriesFast();
	long long last_percentage = 0;
	if (report) {
		printf("Forging T0D3 without trigger   0%%");
		fflush(stdout);
	}
	for (long long entry = 0; entry < total_entries; ++entry) {
		if (report && entry * 100ll / total_entries > last_percentage) {
			last_percentage = entry * 100ll / total_entries;
			printf("\b\b\b\b%3lld%%", last_percentage);
			fflush(stdout);
		}
		ipt->GetEntry(entry);
		if (dssd.front_num == 0 && dssd.back_num == 0) {
			FillHit(raw, dssd);
			ref_time = raw.time;
		} else if (std::fabs(raw.time - ref_time) < window) {
			forge_window.Fill(raw.time - ref_time);
			FillHit(raw, dssd);
		} else {
			opt.Fill();
			ResetDssdEvent(dssd);
			FillHit(raw, dssd);
			ref_time = raw.time;
		}
	}
	if (dssd.front_num > 0 || dssd.back_num > 0) opt.Fill();
	if (report) printf("\b\b\b\b100%%\n");

	opf.cd();
	forge_window.Write();
	opt.Write();
	ipf.Close();
	opf.Close();
	return 0;
}

} // namespace

int SmeltT0d3(
	const std::vector<double> &trigger_time,
	const char *path,
	const char *output_path,
	const double window,
	bool report
) {
	if (trigger_time.empty()) {
		return SmeltWithoutTrigger(path, output_path, window, report);
	}
	return SmeltWithTrigger(trigger_time, path, output_path, window, report);
}

}