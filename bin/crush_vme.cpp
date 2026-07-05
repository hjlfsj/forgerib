#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <string>

#include <TFile.h>
#include <TString.h>
#include <TTree.h>

#include "external/cxxopts.hpp"
#include "external/toml.hpp"
#include "include/config.h"
#include "include/util.h"
#include "include/event/ingot/beam_event.h"
#include "include/event/ingot/csi_event.h"
#include "include/event/ingot/dssd_event.h"
#include "include/event/ingot/ppac_event.h"
#include "include/event/ingot/tafd_event.h"

namespace {

using namespace forgerib;

constexpr int kAdcModuleNum = 10;
constexpr int kAdcChannelNum = 32;
constexpr int kMadcModuleNum = 2;
constexpr int kMadcChannelNum = 32;
constexpr int kGdcBankNum = 2;
constexpr int kGdcChannelNum = 128;
constexpr int kGdcMultiNum = 5;
constexpr int kSdcChannelNum = 32;

struct ChannelRef {
	int module = -1;
	int channel = -1;

	bool Valid() const {
		return module >= 0 && channel >= 0;
	}
};

struct VmeBranches {
	int adc[kAdcModuleNum][kAdcChannelNum] = {};
	int madc[kMadcModuleNum][kMadcChannelNum] = {};
	int gdc[kGdcBankNum][kGdcChannelNum][kGdcMultiNum] = {};
	int gmulti[kGdcBankNum][kGdcChannelNum] = {};
	unsigned long long sdc[kSdcChannelNum] = {};
};

struct TafdThreshold {
	std::array<double, 16> front = {};
	std::array<double, 8> back = {};
};

struct DssdThreshold {
	std::array<double, 32> front = {};
	std::array<double, 32> back = {};
};

struct VmeThreshold {
	std::array<double, 15> t0csi = {};
	std::array<TafdThreshold, 6> tafd = {};
	std::array<double, 12> tafcsi = {};
	DssdThreshold t1du;
	DssdThreshold t1dd;
	std::array<double, 13> t1csid = {};
	std::array<double, 4> t1csiu = {};
};

constexpr std::array<ChannelRef, 12> kTafCsi = {{
	{0, 15}, {0, 14}, {0, 13}, {0, 12}, {0, 11}, {0, 1},
	{0, 9}, {0, 8}, {0, 2}, {0, 6}, {0, 0}, {0, 4}
}};

constexpr std::array<int, 15> kPpacGdcChannels = {{
	16, 17, 24, 26,
	20, 21, 22, 23,
	31, 30, 29, 28,
	27, 19, 18
}};

ChannelRef TafdFrontChannel(int det, int strip) {
	return {det / 2, strip + (det % 2) * 16};
}

ChannelRef TafdBackChannel(int det, int strip) {
	return {3 + det / 4, strip + (det % 4) * 8};
}

ChannelRef T1FrontChannel(bool up, int strip) {
	if (up) return {5, strip};
	return {7, (strip + 16) % 32};
}

ChannelRef T1BackChannel(bool up, int strip) {
	return {up ? 6 : 8, strip};
}

ChannelRef T0CsiChannel(int index) {
	return {9, index < 14 ? index : 15};
}

ChannelRef T1dGaggChannel(int index) {
	return {4, index + 16};
}

ChannelRef T1uCsiChannel(int index) {
	return {0, index + 16};
}

template <size_t N>
int LoadThresholdArray(
	const toml::table &table,
	const char *key,
	std::array<double, N> &thresholds,
	const std::string &path,
	const std::string &scope
) {
	const auto *array = table[key].as_array();
	const std::string full_key = scope.empty()
		? std::string(key)
		: scope + "." + key;
	if (!array) {
		std::cerr << "Error: Missing required threshold array "
			<< full_key << " in " << path << ".\n";
		return -1;
	}
	if (array->size() != N) {
		std::cerr << "Error: Threshold array " << full_key
			<< " in " << path << " must have "
			<< N << " values, got " << array->size() << ".\n";
		return -1;
	}
	for (size_t i = 0; i < N; ++i) {
		auto value = (*array)[i].template value<double>();
		if (!value) {
			std::cerr << "Error: Threshold " << full_key << "[" << i
				<< "] in " << path << " must be numeric.\n";
			return -1;
		}
		thresholds[i] = *value;
	}
	return 0;
}

int LoadVmeThreshold(const std::string &path, VmeThreshold &threshold) {
	try {
		threshold = VmeThreshold();
		toml::table table = toml::parse_file(path);
		if (LoadThresholdArray(table, "t0csi", threshold.t0csi, path, "")) return -1;
		for (int det = 0; det < 6; ++det) {
			const std::string name = "tafd" + std::to_string(det);
			const auto *tafd_table = table[name].as_table();
			if (!tafd_table) {
				std::cerr << "Error: Missing required threshold table "
					<< name << " in " << path << ".\n";
				return -1;
			}
			if (LoadThresholdArray(*tafd_table, "front", threshold.tafd[det].front, path, name)) {
				return -1;
			}
			if (LoadThresholdArray(*tafd_table, "back", threshold.tafd[det].back, path, name)) {
				return -1;
			}
		}
		if (LoadThresholdArray(table, "tafcsi", threshold.tafcsi, path, "")) return -1;
		{
			const auto *t1du_table = table["t1du"].as_table();
			if (!t1du_table) {
				std::cerr << "Error: Missing required threshold table t1du in "
					<< path << ".\n";
				return -1;
			}
			if (LoadThresholdArray(*t1du_table, "front", threshold.t1du.front, path, "t1du")) {
				return -1;
			}
			if (LoadThresholdArray(*t1du_table, "back", threshold.t1du.back, path, "t1du")) {
				return -1;
			}
		}
		{
			const auto *t1dd_table = table["t1dd"].as_table();
			if (!t1dd_table) {
				std::cerr << "Error: Missing required threshold table t1dd in "
					<< path << ".\n";
				return -1;
			}
			if (LoadThresholdArray(*t1dd_table, "front", threshold.t1dd.front, path, "t1dd")) {
				return -1;
			}
			if (LoadThresholdArray(*t1dd_table, "back", threshold.t1dd.back, path, "t1dd")) {
				return -1;
			}
		}
		if (LoadThresholdArray(table, "t1csid", threshold.t1csid, path, "")) return -1;
		if (LoadThresholdArray(table, "t1csiu", threshold.t1csiu, path, "")) return -1;
	} catch (const toml::parse_error &err) {
		std::cerr << "Error: Parsing " << path << " failed:\n" << err << "\n";
		return -1;
	}
	return 0;
}

void ResetTafdEvent(TafdEvent &event) {
	event.flag = 0;
	for (int i = 0; i < 6; ++i) {
		event.valid[i] = false;
		event.front_energy[i] = 0.0;
		event.back_energy[i] = 0.0;
		event.front_strip[i] = -1;
		event.back_strip[i] = -1;
		event.front_time[i] = 0.0;
	}
}

void ResetDssdEvent(DssdEvent &event) {
	event.front_num = 0;
	event.back_num = 0;
	for (int i = 0; i < 8; ++i) {
		event.front_strip[i] = -1;
		event.back_strip[i] = -1;
		event.front_energy[i] = 0.0;
		event.back_energy[i] = 0.0;
		event.front_time[i] = 0.0;
		event.back_time[i] = 0.0;
	}
}

template<int N>
void ResetCsiEvent(CsiEvent<N> &event) {
	event.flag = 0;
	for (int i = 0; i < N; ++i) {
		event.valid[i] = false;
		event.time[i] = 0.0;
		event.energy[i] = 0;
	}
}

void ResetBeamEvent(BeamEvent &event) {
	event.flag = 0;
	event.valid = false;
	event.tof = 0.0;
}

void ResetPpacEvent(PpacEvent &event) {
	event.flag = 0;
	for (int i = 0; i < 15; ++i) {
		event.valid[i] = false;
		event.time[i] = 0.0;
		event.energy[i] = 0;
	}
}

double GdcTime(const VmeBranches &input, int bank, int channel) {
	return double(input.gdc[bank][channel][0] - input.gdc[bank][0][0]) / 10.0;     // reference time
}

int AdcEnergy(const VmeBranches &input, const ChannelRef &ref) {
	if (!ref.Valid()) return 0;
	if (ref.module >= kAdcModuleNum || ref.channel >= kAdcChannelNum) return 0;
	return input.adc[ref.module][ref.channel];
}

int MadcEnergy(const VmeBranches &input, const ChannelRef &ref) {
	if (!ref.Valid()) return 0;
	if (ref.module >= kMadcModuleNum || ref.channel >= kMadcChannelNum) return 0;
	return input.madc[ref.module][ref.channel];
}

void InsertDssdHit(
	int strip,
	int energy,
	double time,
	int &num,
	int strips[8],
	double energies[8],
	double times[8]
) {
	int pos = 0;
	while (pos < num && energies[pos] >= energy) ++pos;
	if (pos >= 8) return;
	int limit = std::min(num, 7);
	for (int i = limit; i > pos; --i) {
		strips[i] = strips[i-1];
		energies[i] = energies[i-1];
		times[i] = times[i-1];
	}
	strips[pos] = strip;
	energies[pos] = energy;
	times[pos] = time;
	if (num < 8) ++num;
}

bool BetterCandidate(int energy, int strip, int best_energy, int best_strip) {
	return energy > best_energy || (energy == best_energy && strip < best_strip);
}

void FillTafdEvent(
	const VmeBranches &input,
	const VmeThreshold &threshold,
	TafdEvent &event
) {
	ResetTafdEvent(event);
	for (int det = 0; det < 6; ++det) {
		int best_front_energy = -1;
		int best_front_strip = std::numeric_limits<int>::max();
		double best_front_time = 0.0;
		for (int strip = 0; strip < 16; ++strip) {
			ChannelRef ref = TafdFrontChannel(det, strip);
			int energy = AdcEnergy(input, ref);
			int time_channel = 32 + det * 16 + strip;
			if (energy < threshold.tafd[det].front[strip] || energy >= 4096) continue;
			if (input.gmulti[0][time_channel] <= 0) continue;
			if (BetterCandidate(energy, strip, best_front_energy, best_front_strip)) {
				best_front_energy = energy;
				best_front_strip = strip;
				best_front_time = GdcTime(input, 0, time_channel);
			}
		}

		int best_back_energy = -1;
		int best_back_strip = std::numeric_limits<int>::max();
		for (int strip = 0; strip < 8; ++strip) {
			ChannelRef ref = TafdBackChannel(det, strip);
			int energy = AdcEnergy(input, ref);
			if (energy < threshold.tafd[det].back[strip] || energy >= 4096) continue;
			if (BetterCandidate(energy, strip, best_back_energy, best_back_strip)) {
				best_back_energy = energy;
				best_back_strip = strip;
			}
		}

		if (best_front_energy > 0 && best_back_energy > 0) {
			event.valid[det] = true;
			event.flag |= 1 << det;
			event.front_energy[det] = best_front_energy;
			event.back_energy[det] = best_back_energy;
			event.front_strip[det] = best_front_strip;
			event.back_strip[det] = best_back_strip;
			event.front_time[det] = best_front_time;
		}
	}
}

void FillDssdEvent(
	const VmeBranches &input,
	DssdEvent &event,
	const DssdThreshold &threshold,
	bool up
) {
	ResetDssdEvent(event);
	const int time_bank = 1;
	const int time_base = up ? 0 : 64;

	for (int strip = 0; strip < 32; ++strip) {
		ChannelRef front = T1FrontChannel(up, strip);
		int front_energy = AdcEnergy(input, front);
		if (front_energy >= threshold.front[strip] && input.gmulti[time_bank][time_base + strip] > 0) {
			InsertDssdHit(
				strip,
				front_energy,
				GdcTime(input, time_bank, time_base + strip),
				event.front_num,
				event.front_strip,
				event.front_energy,
				event.front_time
			);
		}

		ChannelRef back = T1BackChannel(up, strip);
		int back_energy = AdcEnergy(input, back);
		if (back_energy >= threshold.back[strip]) {
			InsertDssdHit(
				strip,
				back_energy,
				0.0,
				event.back_num,
				event.back_strip,
				event.back_energy,
				event.back_time
			);
		}
	}
}

template<int N>
void UpdateCsiHit(CsiEvent<N> &event, int index, int energy, double threshold) {
	if (index < 0 || index >= N || energy < threshold || energy >= 8192) return;
	if (!event.valid[index] || energy > event.energy[index]) {
		event.valid[index] = true;
		event.flag |= 1ull << index;
		event.energy[index] = energy;
		event.time[index] = 0.0;
	}
}

void FillT0CsiEvent(
	const VmeBranches &input,
	const VmeThreshold &threshold,
	CsiEvent<36> &event
) {
	ResetCsiEvent(event);
	for (int i = 0; i < 15; ++i) {
		UpdateCsiHit(event, i, AdcEnergy(input, T0CsiChannel(i)), threshold.t0csi[i]);
	}
}

void FillTafCsiEvent(
	const VmeBranches &input,
	const VmeThreshold &threshold,
	CsiEvent<12> &event
) {
	ResetCsiEvent(event);
	for (int i = 0; i < 12; ++i) {
		UpdateCsiHit(event, i, MadcEnergy(input, kTafCsi[i]), threshold.tafcsi[i]);
	}
}

void FillT1DCsiEvent(
	const VmeBranches &input,
	const VmeThreshold &threshold,
	CsiEvent<13> &event
) {
	ResetCsiEvent(event);
	for (int i = 0; i < 12; ++i) {
		UpdateCsiHit(event, i, AdcEnergy(input, T1dGaggChannel(i)), threshold.t1csid[i]);
	}
	UpdateCsiHit(event, 12, MadcEnergy(input, {0, 20}), threshold.t1csid[12]);
}

void FillT1UCsiEvent(
	const VmeBranches &input,
	const VmeThreshold &threshold,
	CsiEvent<4> &event
) {
	ResetCsiEvent(event);
	for (int i = 0; i < 4; ++i) {
		UpdateCsiHit(event, i, MadcEnergy(input, T1uCsiChannel(i)), threshold.t1csiu[i]);
	}
}

void FillBeamEvent(const VmeBranches &input, BeamEvent &event) {
	ResetBeamEvent(event);
	const int t1_channel = 97;
	const int t2_channel = 98;
	double t1 = 0.0;
	double t2 = 0.0;
	if (input.gmulti[1][t1_channel] > 0) {
		event.flag |= 1 << 0;
		t1 = GdcTime(input, 1, t1_channel);
	}
	if (input.gmulti[1][t2_channel] > 0) {
		event.flag |= 1 << 1;
		t2 = GdcTime(input, 1, t2_channel);
	}
	if ((event.flag & 0x3) == 0x3) {
		event.valid = true;
		event.tof = t2 - t1;
	}
}

void FillPpacEvent(const VmeBranches &input, PpacEvent &event) {
	ResetPpacEvent(event);
	for (int bit = 0; bit < 15; ++bit) {
		int channel = kPpacGdcChannels[bit];
		if (input.gmulti[0][channel] <= 0) continue;
		event.flag |= 1 << bit;
		event.valid[bit] = true;
		event.time[bit] = GdcTime(input, 0, channel);
	}
}

bool HasTafdTrigger(const TafdEvent &event) {
	for (int i = 0; i < 6; ++i) {
		if (event.valid[i]) return true;
	}
	return false;
}

template<int N>
bool HasCsiTrigger(const CsiEvent<N> &event) {
	for (int i = 0; i < N; ++i) {
		if (event.valid[i]) return true;
	}
	return false;
}

bool HasDssdTrigger(const DssdEvent &event) {
	return event.front_num > 0 || event.back_num > 0;
}

} // namespace

int main(int argc, char **argv) {
	cxxopts::Options options("crush_vme", "map VME ROOT file to crushed detector ROOT files");
	options.add_options()
		("h,help", "Print usage")
		(
			"run",
			"VME run number.",
			cxxopts::value<int>(),
			"run"
		)
		(
			"c,config",
			"Config file path.",
			cxxopts::value<std::string>()->default_value("config.toml"),
			"path"
		)
		(
			"threshold",
			"Threshold file path.",
			cxxopts::value<std::string>()->default_value("vme_threshold.toml"),
			"path"
		)
		;
	options.parse_positional({"run"});
	auto parse_result = options.parse(argc, argv);
	if (parse_result.count("help") || !parse_result.count("run")) {
		std::cout << options.help() << std::endl;
		return 0;
	}

	const int vme_run = parse_result["run"].as<int>();
	const std::string config_path = parse_result["config"].as<std::string>();
	const std::string threshold_path = parse_result["threshold"].as<std::string>();

	AppConfig config;
	if (LoadConfig(config_path, config)) {
		return -1;
	}
	VmeThreshold threshold;
	if (LoadVmeThreshold(threshold_path, threshold)) {
		return -1;
	}
	const std::string output_dir = JoinPath(config.workspace, config.paths.grit);

	TString input_path = TString::Format(
		"%s/%s%04d.root",
		config.paths.raw_vme.c_str(),
		config.prefix.raw_vme.c_str(),
		vme_run
	);
	TFile input_file(input_path, "read");
	TTree *input_tree = static_cast<TTree*>(input_file.Get("tree"));
	if (!input_tree) {
		std::cerr << "Error: Get tree from " << input_path << " failed.\n";
		return -1;
	}

	VmeBranches input;
	input_tree->SetBranchAddress("adc", input.adc);
	input_tree->SetBranchAddress("madc", input.madc);
	input_tree->SetBranchAddress("gdc", input.gdc);
	input_tree->SetBranchAddress("gmulti", input.gmulti);
	input_tree->SetBranchAddress("sdc", input.sdc);

	TString tafd_path = TString::Format("%s/tafd_vme_%04d.root", output_dir.c_str(), vme_run);
	TFile tafd_file(tafd_path, "recreate");
	TTree tafd_tree("tree", "tafd vme");
	TafdEvent tafd_event;
	SetupOutput(&tafd_tree, tafd_event);

	TString t1du_path = TString::Format("%s/t1du_vme_%04d.root", output_dir.c_str(), vme_run);
	TFile t1du_file(t1du_path, "recreate");
	TTree t1du_tree("tree", "t1du vme");
	DssdEvent t1du_event;
	SetupOutput(&t1du_tree, t1du_event);

	TString t1dd_path = TString::Format("%s/t1dd_vme_%04d.root", output_dir.c_str(), vme_run);
	TFile t1dd_file(t1dd_path, "recreate");
	TTree t1dd_tree("tree", "t1dd vme");
	DssdEvent t1dd_event;
	SetupOutput(&t1dd_tree, t1dd_event);

	TString t0csi_path = TString::Format("%s/t0csi_vme_%04d.root", output_dir.c_str(), vme_run);
	TFile t0csi_file(t0csi_path, "recreate");
	TTree t0csi_tree("tree", "t0csi vme");
	CsiEvent<36> t0csi_event;
	SetupOutput(&t0csi_tree, t0csi_event);

	TString tafcsi_path = TString::Format("%s/tafcsi_vme_%04d.root", output_dir.c_str(), vme_run);
	TFile tafcsi_file(tafcsi_path, "recreate");
	TTree tafcsi_tree("tree", "tafcsi vme");
	CsiEvent<12> tafcsi_event;
	SetupOutput(&tafcsi_tree, tafcsi_event);

	TString t1dcsi_path = TString::Format("%s/t1csid_vme_%04d.root", output_dir.c_str(), vme_run);
	TFile t1dcsi_file(t1dcsi_path, "recreate");
	TTree t1dcsi_tree("tree", "t1dcsi vme");
	CsiEvent<13> t1dcsi_event;
	SetupOutput(&t1dcsi_tree, t1dcsi_event);

	TString t1ucsi_path = TString::Format("%s/t1csiu_vme_%04d.root", output_dir.c_str(), vme_run);
	TFile t1ucsi_file(t1ucsi_path, "recreate");
	TTree t1ucsi_tree("tree", "t1ucsi vme");
	CsiEvent<4> t1ucsi_event;
	SetupOutput(&t1ucsi_tree, t1ucsi_event);

	TString beam_path = TString::Format("%s/beam_vme_%04d.root", output_dir.c_str(), vme_run);
	TFile beam_file(beam_path, "recreate");
	TTree beam_tree("tree", "beam vme");
	BeamEvent beam_event;
	SetupOutput(&beam_tree, beam_event);

	TString ppac_path = TString::Format("%s/ppac_vme_%04d.root", output_dir.c_str(), vme_run);
	TFile ppac_file(ppac_path, "recreate");
	TTree ppac_tree("tree", "ppac vme");
	PpacEvent ppac_event;
	SetupOutput(&ppac_tree, ppac_event);

	TString trigger_path = TString::Format("%s/trigger_vme_%04d.root", output_dir.c_str(), vme_run);
	TFile trigger_file(trigger_path, "recreate");
	TTree trigger_tree("tree", "trigger vme");
	bool trigger_valid = true;
	long long trigger_time = 0;
	trigger_tree.Branch("valid", &trigger_valid, "v/O");
	trigger_tree.Branch("time", &trigger_time, "time/L");

	TString trigger_t1_path = TString::Format(
		"%s/trigger_vme_t1_%04d.root",
		output_dir.c_str(),
		vme_run
	);
	TFile trigger_t1_file(trigger_t1_path, "recreate");
	TTree trigger_t1_tree("tree", "trigger vme t1");
	bool trigger_t1_valid = false;
	long long trigger_t1_time = 0;
	trigger_t1_tree.Branch("valid", &trigger_t1_valid, "v/O");
	trigger_t1_tree.Branch("time", &trigger_t1_time, "time/L");

	const long long total = input_tree->GetEntries();
	long long last_percentage = 0;
	printf("Mapping VME   0%%");
	fflush(stdout);
	for (long long entry = 0; entry < total; ++entry) {
		if (entry * 100ll / total > last_percentage) {
			last_percentage = entry * 100ll / total;
			printf("\b\b\b\b%3lld%%", last_percentage);
			fflush(stdout);
		}

		input_tree->GetEntry(entry);

		FillTafdEvent(input, threshold, tafd_event);
		FillDssdEvent(input, t1du_event, threshold.t1du, true);
		FillDssdEvent(input, t1dd_event, threshold.t1dd, false);
		FillT0CsiEvent(input, threshold, t0csi_event);
		FillTafCsiEvent(input, threshold, tafcsi_event);
		FillT1DCsiEvent(input, threshold, t1dcsi_event);
		FillT1UCsiEvent(input, threshold, t1ucsi_event);
		FillBeamEvent(input, beam_event);
		FillPpacEvent(input, ppac_event);
		trigger_time = static_cast<long long>(input.sdc[1]);
		trigger_t1_time = trigger_time;
		trigger_t1_valid =
			HasTafdTrigger(tafd_event)
			|| HasCsiTrigger(tafcsi_event)
			|| HasCsiTrigger(t0csi_event)
			|| HasDssdTrigger(t1du_event)
			|| HasDssdTrigger(t1dd_event)
			|| HasCsiTrigger(t1dcsi_event)
			|| HasCsiTrigger(t1ucsi_event);   // has real event

		tafd_tree.Fill();
		t1du_tree.Fill();
		t1dd_tree.Fill();
		t0csi_tree.Fill();
		tafcsi_tree.Fill();
		t1dcsi_tree.Fill();
		t1ucsi_tree.Fill();
		beam_tree.Fill();
		ppac_tree.Fill();
		trigger_tree.Fill();
		trigger_t1_tree.Fill();
	}
	printf("\b\b\b\b100%%\n");

	tafd_file.cd();
	tafd_tree.Write();
	tafd_file.Close();

	t1du_file.cd();
	t1du_tree.Write();
	t1du_file.Close();

	t1dd_file.cd();
	t1dd_tree.Write();
	t1dd_file.Close();

	t0csi_file.cd();
	t0csi_tree.Write();
	t0csi_file.Close();

	tafcsi_file.cd();
	tafcsi_tree.Write();
	tafcsi_file.Close();

	t1dcsi_file.cd();
	t1dcsi_tree.Write();
	t1dcsi_file.Close();

	t1ucsi_file.cd();
	t1ucsi_tree.Write();
	t1ucsi_file.Close();

	beam_file.cd();
	beam_tree.Write();
	beam_file.Close();

	ppac_file.cd();
	ppac_tree.Write();
	ppac_file.Close();

	trigger_file.cd();
	trigger_tree.Write();
	trigger_file.Close();

	trigger_t1_file.cd();
	trigger_t1_tree.Write();
	trigger_t1_file.Close();

	input_file.Close();
	return 0;
}
