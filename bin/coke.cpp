#include <cstdio>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <map>
#include <set>
#include <vector>

#include <TFile.h>
#include <TString.h>
#include <TTree.h>
#include <TH1F.h>

#include "external/cxxopts.hpp"
#include "include/util.h"
#include "include/config.h"
#include "include/event/ore/raw_trigger_event.h"
#include "include/event/ingot/align_event.h"
#include "include/event/ingot/trigger_event.h"

using namespace forgerib;

constexpr size_t kSlotNum = 1000;

int ReadAlignEvents(
	const char *path,
	std::map<long long, long long> &entry_map,
	bool report = false
) {
	TFile ipf(path, "read");
	TTree *ipt = static_cast<TTree*>(ipf.Get("tree"));
	if (!ipt) {
		std::cerr << "Error: Get tree from " << path << " failed.\n";
		return -1;
	}
	forgerib::AlignEvent event;
	forgerib::SetupInput(ipt, event);

	if (report) {
		printf("Reading align events   0%%");
		fflush(stdout);
	}
	long long total = ipt->GetEntries();
	long long last_percentage = 0;
	for (long long entry = 0; entry < total; ++entry) {
		if (report && total > 0 && entry * 100ll / total > last_percentage) {
			last_percentage = entry * 100ll / total;
			printf("\b\b\b\b%3lld%%", last_percentage);
			fflush(stdout);
		}
		ipt->GetEntry(entry);
		entry_map.insert(std::make_pair(event.xia_entry, event.vme_entry));
	}
	if (report) {
		printf("\b\b\b\b100%%\n");
        printf("Align valid: %ld\n", entry_map.size());
	}
	ipf.Close();
	return 0;
}

int ReadVmeValid(
	const char *path,
	std::set<long long> &vme_valid,
	bool report = false
) {
	TFile ipf(path, "read");
	TTree *ipt = static_cast<TTree*>(ipf.Get("tree"));
	if (!ipt) {
		std::cerr << "Error: Get tree from " << path << " failed.\n";
		return -1;
	}
	bool valid;
	ipt->SetBranchAddress("valid", &valid);

	vme_valid.clear();
	if (report) {
		printf("Reading VME t1   0%%");
		fflush(stdout);
	}
	long long total = ipt->GetEntries();
	long long last_percentage = 0;
	for (long long entry = 0; entry < total; ++entry) {
		if (report && total > 0 && entry * 100ll / total > last_percentage) {
			last_percentage = entry * 100ll / total;
			printf("\b\b\b\b%3lld%%", last_percentage);
			fflush(stdout);
		}
		ipt->GetEntry(entry);
		if (valid) vme_valid.insert(entry);
	}
	if (report) {
		printf("\b\b\b\b100%%\n");
        printf("VME T1 valid: %ld\n", vme_valid.size());
	}
	ipf.Close();
	return 0;
}

void UpdateTriggerEvent(
	const RawTriggerEvent &raw,
	TriggerEvent &trigger,
	const long long vme_entry = -1
) {
	if (raw.type < 0 || raw.type >= 6) return;
	trigger.flag |= 1 << raw.type;
	trigger.valid[raw.type] = true;
	trigger.time[raw.type] = raw.time;
	trigger.external_time[raw.type] = raw.external_time;
	trigger.vme_entry = vme_entry >= 0 ? vme_entry : trigger.vme_entry;
}

int Coke(
	const char *grain_path,
	const char *vme_trigger_path,
	const char *input_path,
	const char *full_output_path,
	const char *t1_output_path,
	const double window,
	bool report = false
) {
	std::map<long long, long long> entry_map;
	if (ReadAlignEvents(
		grain_path,
		entry_map,
		true
	)) {
		std::cerr << "Error: Read sift events failed.\n";
		return -1;
	}

	std::set<long long> vme_valid;
	if (ReadVmeValid(
		vme_trigger_path,
		vme_valid,
		true
	)) {
		std::cerr << "Error: Read VME T1 trigger failed.\n";
		return -2;
	}

	TFile ipf(input_path, "read");
	TTree *ipt = (TTree*)ipf.Get("tree");
	if (!ipt) {
		std::cerr << "Error: Get tree from "
			<< input_path << " failed.\n";
		return -3;
	}
	RawTriggerEvent raw;
	SetupInput(ipt, raw);

	std::vector<double> triggers;

	if (report) {
		printf("Reading trigger   0%%");
		fflush(stdout);
	}
	long long total = ipt->GetEntries();
	long long last_percentage = 0;
	for (long long entry = 0; entry < total; ++entry) {
		if (report && total > 0 && entry * 100ll / total > last_percentage) {
			last_percentage = entry * 100ll / total;
			printf("\b\b\b\b%3lld%%", last_percentage);
			fflush(stdout);
		}
		ipt->GetEntry(entry);
		if (raw.type != forgerib::kTriggerMain) continue;
		triggers.push_back(raw.time);
	}
	if (report) printf("\b\b\b\b100%%\n");

	TFile opf(full_output_path, "recreate");
	TH1F forge_window("fw", "forge window", 200, -window, window);
	TTree opt("tree", "coke main trigger");
	TriggerEvent trigger;
	SetupOutput(&opt, trigger);

	TFile t1_opf(t1_output_path, "recreate");
	TTree t1_opt("tree", "coke t1 trigger");
	SetupOutput(&t1_opt, trigger);

	TriggerEvent slots[kSlotNum];
	for (size_t i = 0; i < kSlotNum; ++i) {
		Reset(slots[i]);
	}
	size_t tofill_entry = 0;

	last_percentage = 0;
	if (report) {
		printf("Forging trigger   0%%");
		fflush(stdout);
	}
	for (long long entry = 0; entry < total; ++entry) {
		if (report && entry * 100ll / total > last_percentage) {
			last_percentage = entry * 100ll / total;
			printf("\b\b\b\b%3lld%%", last_percentage);
			fflush(stdout);
		}
		ipt->GetEntry(entry);
		// search trigger
		double min_time = 2*window;
		size_t min_time_entry = 0;
		for (
			auto iter = std::lower_bound(
				triggers.begin()+tofill_entry,
				triggers.end(),
				raw.time - window
			);
			iter != triggers.end();
			++iter
		) {
			if (*iter > raw.time + window) break;
			forge_window.Fill(raw.time - *iter);
			if (fabs(*iter - raw.time) < min_time) {
				min_time = fabs(*iter - raw.time);
				min_time_entry = iter - triggers.begin();
			}
		}
		if (min_time > window) continue;
		//printf("Type: %d, time: %f, entry %ld, tofill %ld, slot_flag %x\n", raw.type, raw.time, min_time_entry, tofill_entry, slots[min_time_entry%kSlotNum].flag);
		if (min_time_entry < tofill_entry) continue;
		if (min_time_entry - tofill_entry >= kSlotNum) {
			for (size_t fill = tofill_entry; fill <= min_time_entry-kSlotNum; ++fill) {
				trigger = slots[fill%kSlotNum];
				opf.cd();
				opt.Fill();
				if (vme_valid.count(trigger.vme_entry)) {
					t1_opf.cd();
					t1_opt.Fill();
				}
				Reset(slots[fill%kSlotNum]);
			}
			tofill_entry = min_time_entry - kSlotNum+1;
		}
		long long vme_entry = -1;
		if (raw.type == kTriggerVme) {
			auto found = entry_map.find(entry);
			if (found != entry_map.end()) {
				vme_entry = found->second;
			}
		}
		UpdateTriggerEvent(raw, slots[min_time_entry%kSlotNum], vme_entry);
	}
	for (size_t fill = tofill_entry; fill < triggers.size(); ++fill) {
		trigger = slots[fill%kSlotNum];
		opt.Fill();
	}
	if (report) printf("\b\b\b\b100%%\n");

	opf.cd();
	forge_window.Write();
	opt.Write();
	opf.Close();
	t1_opf.cd();
	t1_opt.Write();
	t1_opf.Close();
	ipf.Close();
	return 0;
}


int main(int argc, char **argv) {
	cxxopts::Options options("coke", "coke trigger events into ingot output");
	options.add_options()
		("h,help", "Print usage")
		(
			"r,run",
			"XIA and VME run number.",
			cxxopts::value<int>(),
			"run"
		)
		(
			"x,xia-run",
			"XIA run number, overwrite run.",
			cxxopts::value<int>(),
			"run"
		)
		(
			"v,vme-run",
			"VME run number, overwrite run.",
			cxxopts::value<int>(),
			"run"
		)
		(
			"c,config",
			"Config file path.",
			cxxopts::value<std::string>()->default_value("config.toml"),
			"path"
		);
	auto parse_result = options.parse(argc, argv);

	if (
		parse_result.count("help")
		|| (
			!parse_result.count("run")
			&& !(
				parse_result.count("xia-run")
				&& parse_result.count("vme-run")
			)
		)
	) {
		std::cout << options.help() << "\n";
		return 0;
	}

	const int xia_run = parse_result.count("xia-run")
		? parse_result["xia-run"].as<int>()
		: parse_result["run"].as<int>();
	const int vme_run = parse_result.count("vme-run")
		? parse_result["vme-run"].as<int>()
		: parse_result["run"].as<int>();
	const std::string config_path = parse_result["config"].as<std::string>();

	forgerib::AppConfig config;
	if (forgerib::LoadConfig(config_path, config)) return -1;

	const std::string grain_dir = JoinPath(config.workspace, config.paths.grain);
	const std::string grit_dir = JoinPath(config.workspace, config.paths.grit);
	const std::string output_dir = JoinPath(config.workspace, config.paths.ingot);
	TString grain_path = TString::Format(
		"%s/grain_%04d.root",
		grain_dir.c_str(),
		xia_run
	);
	TString trigger_path = TString::Format(
		"%s/trigger_%04d.root",
		grit_dir.c_str(),
		xia_run
	);
	TString vme_trigger_path = TString::Format(
		"%s/trigger_vme_t1_%04d.root",
		grit_dir.c_str(),
		vme_run
	);
	TString full_output_path = TString::Format(
		"%s/trigger_%04d.root",
		output_dir.c_str(),
		xia_run
	);
	TString t1_output_path = TString::Format(
		"%s/trigger_t1_%04d.root",
		output_dir.c_str(),
		xia_run
	);
	return Coke(
		grain_path.Data(),
		vme_trigger_path.Data(),
		trigger_path.Data(),
		full_output_path.Data(),
		t1_output_path.Data(),
		1000.0,
		true
	);
}