#include <iostream>
#include <map>

#include <TFile.h>
#include <TString.h>
#include <TTree.h>

#include "external/cxxopts.hpp"

#include "include/config.h"
#include "include/util.h"
#include "include/event/ore/raw_trigger_event.h"
#include "include/smelt/sifting.h"

using namespace forgerib;

int ReadXiaTimes(
	const char *path,
	std::vector<double> &times,
	std::vector<double> &external_times,
	std::vector<long long> &entries,
	bool report = false
) {
	TFile ipf(path, "read");
	TTree *ipt = static_cast<TTree*>(ipf.Get("tree"));
	if (!ipt) {
		std::cerr << "Error: Get tree from " << path << " failed.\n";
		return -1;
	}
	forgerib::RawTriggerEvent trigger_event;
	forgerib::SetupInput(ipt, trigger_event);

	times.clear();
	external_times.clear();
	entries.clear();
	if (report) {
		printf("Reading XIA times   0%%");
		fflush(stdout);
	}
	long long total = ipt->GetEntries();
	long long last_percentage = 0;
	for (long long entry = 0; entry < total; ++entry) {
		if (report && entry * 100ll / total > last_percentage) {
			last_percentage = entry * 100ll / total;
			printf("\b\b\b\b%3lld%%", last_percentage);
			fflush(stdout);
		}
		ipt->GetEntry(entry);
		if (trigger_event.type == 1) {
			times.push_back(trigger_event.time);
			external_times.push_back(trigger_event.external_time);
			entries.push_back(entry);
		}
	}
	if (report) {
		printf("\b\b\b\b100%%\n");
	}
	return 0;
}

int ReadVmeTimes(
	const char *path,
	std::vector<double> &times,
	std::vector<long long> &entries,
	bool report = false
) {
	TFile ipf(path, "read");
	TTree *ipt = static_cast<TTree*>(ipf.Get("tree"));
	if (!ipt) {
		std::cerr << "Error: get tree from " << path << " failed.\n";
		return -1;
	}
	unsigned long long sdc[32];
	ipt->SetBranchAddress("sdc", sdc);

	times.clear();
	entries.clear();
	long long bit_flip_offset = 0;
	long long last_timestamp = 0;
	long long total = ipt->GetEntries();
	long long last_percentage = 0;
	if (report) {
		printf("Reading VME trigger times   0%%");
		fflush(stdout);
	}
	for (long long entry = 0; entry < total; ++entry) {
		if (report && entry * 100ll / total > last_percentage) {
			last_percentage = entry * 100ll / total;
			printf("\b\b\b\b%3lld%%", last_percentage);
			fflush(stdout);
		}

		ipt->GetEntry(entry);
		if (sdc[1] == 0) continue;
		long long timestamp = (sdc[1] + bit_flip_offset) * 200;
		if (timestamp < last_timestamp) {
			bit_flip_offset += 1ll << 32;
			timestamp += (1ll << 32) * 200;
		}
		last_timestamp = timestamp;

		times.push_back(double(timestamp));
		entries.push_back(entry);
	}
	if (report) printf("\b\b\b\b100%%\n");
	ipf.Close();
	return 0;
}


int main(int argc, char **argv) {
	cxxopts::Options options("sift", "sift trigger alignment");
	options.add_options()
		("h,help", "Print usage")
		(
			"c,config",
			"Config file path.",
			cxxopts::value<std::string>()->default_value("config.toml"),
			"path"
		)
		(
			"x,xia_run",
			"XIA run number, required.",
			cxxopts::value<int>(),
			"run"
		)
		(
			"v,vme_run",
			"VME run number, required.",
			cxxopts::value<int>(),
			"run"
		)
		(
			"e,external",
			"Use XIA's external timestamp.",
			cxxopts::value<bool>()->default_value("false")
		);
	auto parse_result = options.parse(argc, argv);

	if (
		parse_result.count("help")
		|| !parse_result.count("xia_run")
		|| !parse_result.count("vme_run")
	) {
		std::cout << options.help() << std::endl;
		return 0;
	}
	int xia_run = parse_result["xia_run"].as<int>();
	int vme_run = parse_result["vme_run"].as<int>();
	bool external = parse_result["external"].as<bool>();
	const std::string config_path = parse_result["config"].as<std::string>();

	AppConfig config;
	if (LoadConfig(config_path, config)) {
		return -1;
	}
	const std::string grit_dir = JoinPath(config.workspace, config.paths.grit);
	const std::string vme_dir = JoinPath(config.workspace, config.paths.raw_vme);
	const std::string grain_dir = JoinPath(config.workspace, config.paths.grain);
	const std::string vme_prefix = config.prefix.raw_vme;

	std::vector<double> xia_times;
	std::vector<double> xia_external_times;
	std::vector<long long> xia_entries;
	TString xia_filename = TString::Format(
		"%s/trigger_%04d.root",
		grit_dir.c_str(),
		xia_run
	);
	if (ReadXiaTimes(
		xia_filename.Data(),
		xia_times,
		xia_external_times,
		xia_entries,
		true
	)) {
		std::cerr << "Error: Read XIA times failed.\n";
		return -2;
	}

	std::vector<double> vme_times;
	std::vector<long long> vme_entries;
	TString vme_filename = TString::Format(
		"%s/%s%04d.root",
		vme_dir.c_str(),
		vme_prefix.c_str(),
		vme_run
	);
	if (ReadVmeTimes(
		vme_filename,
		vme_times,
		vme_entries,
		true
	)) {
		std::cerr << "Error: Read VME trigger times failed.\n";
		return -2;
	}

	TString output_file = TString::Format(
		"%s/grain_%04d.root",
		grain_dir.c_str(),
		xia_run
	);

	int result = Sift(
		(external ? xia_external_times : xia_times),
		xia_entries,
		vme_times,
		vme_entries,
		output_file,
		10000, // window 50*200ns
		100,    //lookhead 10,100
		1000,
		1000,
		true
	);

	if (result) {
		std::cerr << "Error: Sift failed.\n";
		return -1;
	}
	return 0;
}
