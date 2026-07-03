#include <iostream>
#include <memory>

#include <TString.h>
#include <TFile.h>
#include <TTree.h>

#include "external/cxxopts.hpp"
#include "include/event/ore/raw_ssd_event.h"
#include "include/event/ore/decode_event.h"
#include "include/crush/raw_reader.h"
#include "include/config.h"
#include "include/util.h"

namespace {

void DecodeT0sWithTrace(
	const char *raw_path,
	const char *raw_prefix,
	const char *output_path,
	int run,
	bool report
) {
	TString filename = TString::Format(
		"%s/t0s_trace_%04d.root",
		output_path,
		run
	);
	TFile opf(filename, "recreate");
	TTree opt("tree", "decode");
	forgerib::Rawssd_with_trace_Event t0s;
	forgerib::SetupOutput_trace(&opt, t0s);

	if (report) {
		printf("Decoding T0s with traces   0%%");
		fflush(stdout);
	}
	size_t last_percentage = 0;

	std::unique_ptr<forgerib::RawReader> reader(
		new forgerib::RawReader(raw_path, raw_prefix, run, 2, 6, 100)
	);
	forgerib::DecodeEvent event;
	event.used = true;
	forgerib::DecodeTraces traces;

	size_t total_bytes = reader->FileSize();
	size_t bytes = 0;
	while (bytes < total_bytes) {
		if (report && bytes * 100lu / total_bytes > last_percentage) {
			last_percentage = bytes * 100 / total_bytes;
			printf("\b\b\b\b%3lu%%", last_percentage);
			fflush(stdout);
		}

		int b = reader->Read(&event, nullptr, nullptr, nullptr, &traces);
		if (b >= 16) bytes += b;
		else break;

		if (event.channel != 8) continue;

		t0s.energy = event.energy;
		t0s.time = event.time;
		t0s.cv = event.cfd_valid;
		// t0s.length = traces.length > 200 ? 200 : traces.length;
        t0s.length = 200;

		for (int i = 0; i < t0s.length; i++) {
			t0s.samples[i] = traces.samples[i];
		}
		t0s.out_of_range = traces.out_of_range;

		opt.Fill();
	}
	if (report) printf("\b\b\b\b100%%\n");

	opf.cd();
	opt.Write();
	opf.Close();
}

} // namespace

int main(int argc, char **argv) {
	cxxopts::Options options("crush_t0s_trace", "crush T0s traces into ROOT output");
	options.add_options()
		("h,help", "Print usage")
		(
			"c,config",
			"Config file path.",
			cxxopts::value<std::string>()->default_value("config.toml"),
			"path"
		)
		(
			"run",
			"Run number.",
			cxxopts::value<int>(),
			"run"
		);
	options.parse_positional({"run"});
	auto parse_result = options.parse(argc, argv);
	if (parse_result.count("help") || !parse_result.count("run")) {
		std::cout << options.help() << "\n";
		return 0;
	}
	const int run = parse_result["run"].as<int>();
	const std::string config_path = parse_result["config"].as<std::string>();

	forgerib::AppConfig config;
	if (forgerib::LoadConfig(config_path, config)) {
		return -1;
	}
	const std::string output_dir = forgerib::JoinPath(
		config.workspace,
		config.paths.grit
	);

	DecodeT0sWithTrace(
		config.paths.raw_xia2.c_str(),
		config.prefix.raw_xia2.c_str(),
		output_dir.c_str(),
		run,
		true
	);
	return 0;
}