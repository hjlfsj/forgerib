#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <TFile.h>
#include <TF1.h>
#include <TH1D.h>
#include <TSpectrum.h>
#include <TTree.h>

#include "external/toml.hpp"

namespace {

constexpr int kAdcModuleNum = 10;
constexpr int kAdcChannelNum = 32;
constexpr int kAdcSaturation = 4096;
constexpr int kNumBins = 4096;
constexpr double kBinLow = 0.0;
constexpr double kBinHigh = 4096.0;

constexpr double kAlphaEnergyLow = 5.486;
constexpr double kAlphaEnergyHigh = 5.805;

struct ChannelRef {
	int module = -1;
	int channel = -1;
};

struct VmeBranches {
	int adc[kAdcModuleNum][kAdcChannelNum] = {};
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

int AdcEnergy(const VmeBranches &input, const ChannelRef &ref) {
	if (ref.module < 0 || ref.module >= kAdcModuleNum) return -1;
	if (ref.channel < 0 || ref.channel >= kAdcChannelNum) return -1;
	return input.adc[ref.module][ref.channel];
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

struct CalibChannel {
	std::string detector;
	std::string face;
	int strip;
	ChannelRef adc_ref;
	double threshold;
	TH1D *hist;

	double peak_low_ch = 0.0;
	double peak_low_err = 0.0;
	double peak_high_ch = 0.0;
	double peak_high_err = 0.0;
	double k = 0.0;
	double b = 0.0;
	bool valid = false;
};

void RegisterTafdChannels(
	const VmeThreshold &threshold,
	std::vector<CalibChannel> &channels
) {
	for (int det = 0; det < 6; ++det) {
		std::string det_name = "tafd" + std::to_string(det);
		for (int strip = 0; strip < 16; ++strip) {
			CalibChannel ch;
			ch.detector = det_name;
			ch.face = "front";
			ch.strip = strip;
			ch.adc_ref = TafdFrontChannel(det, strip);
			ch.threshold = threshold.tafd[det].front[strip];
			ch.hist = new TH1D(
				(det_name + "_front_" + std::to_string(strip)).c_str(),
				(det_name + " front strip " + std::to_string(strip)).c_str(),
				kNumBins, kBinLow, kBinHigh
			);
			ch.hist->SetDirectory(nullptr);
			channels.push_back(ch);
		}
		for (int strip = 0; strip < 8; ++strip) {
			CalibChannel ch;
			ch.detector = det_name;
			ch.face = "back";
			ch.strip = strip;
			ch.adc_ref = TafdBackChannel(det, strip);
			ch.threshold = threshold.tafd[det].back[strip];
			ch.hist = new TH1D(
				(det_name + "_back_" + std::to_string(strip)).c_str(),
				(det_name + " back strip " + std::to_string(strip)).c_str(),
				kNumBins, kBinLow, kBinHigh
			);
			ch.hist->SetDirectory(nullptr);
			channels.push_back(ch);
		}
	}
}

void RegisterT1Channels(
	const char *det_name,
	bool up,
	const DssdThreshold &dssd_threshold,
	std::vector<CalibChannel> &channels
) {
	for (int strip = 0; strip < 32; ++strip) {
		CalibChannel ch_front;
		ch_front.detector = det_name;
		ch_front.face = "front";
		ch_front.strip = strip;
		ch_front.adc_ref = T1FrontChannel(up, strip);
		ch_front.threshold = dssd_threshold.front[strip];
		ch_front.hist = new TH1D(
			(std::string(det_name) + "_front_" + std::to_string(strip)).c_str(),
			(std::string(det_name) + " front strip " + std::to_string(strip)).c_str(),
			kNumBins, kBinLow, kBinHigh
		);
		ch_front.hist->SetDirectory(nullptr);
		channels.push_back(ch_front);

		CalibChannel ch_back;
		ch_back.detector = det_name;
		ch_back.face = "back";
		ch_back.strip = strip;
		ch_back.adc_ref = T1BackChannel(up, strip);
		ch_back.threshold = dssd_threshold.back[strip];
		ch_back.hist = new TH1D(
			(std::string(det_name) + "_back_" + std::to_string(strip)).c_str(),
			(std::string(det_name) + " back strip " + std::to_string(strip)).c_str(),
			kNumBins, kBinLow, kBinHigh
		);
		ch_back.hist->SetDirectory(nullptr);
		channels.push_back(ch_back);
	}
}

bool CalibrateChannel(CalibChannel &ch, int idx, int total) {
	TH1D *h = ch.hist;
	int entries = static_cast<int>(h->GetEntries());
	if (entries < 100) {
		std::cout << "  [" << idx << "/" << total << "] "
			<< ch.detector << "_" << ch.face << "_" << ch.strip
			<< "  entries=" << entries << "  -> SKIP (too few entries)" << std::endl;
		return false;
	}

	const double kPeakSigma = 4.0;
	TSpectrum spec(2, 2);
	int n_found = spec.Search(h, kPeakSigma, "", 0.10);

	if (n_found < 2) {
		n_found = spec.Search(h, kPeakSigma, "", 0.05);
	}

	if (n_found < 2) {
		n_found = spec.Search(h, kPeakSigma, "", 0.03);
	}

	if (n_found < 2) {
		std::cout << "  [" << idx << "/" << total << "] "
			<< ch.detector << "_" << ch.face << "_" << ch.strip
			<< "  entries=" << entries
			<< "  found " << n_found << " peak(s) -> SKIP" << std::endl;
		return false;
	}

	double *pos = spec.GetPositionX();
	double p1 = pos[0];
	double p2 = pos[1];
	if (p1 > p2) std::swap(p1, p2);

	double fit_range = 5.0;
	double fit_low1 = std::max(p1 - fit_range, kBinLow);
	double fit_high1 = std::min(p1 + fit_range, kBinHigh);
	double fit_low2 = std::max(p2 - fit_range, kBinLow);
	double fit_high2 = std::min(p2 + fit_range, kBinHigh);

	std::string gname1 = ch.hist->GetName() + std::string("_g1");
	TF1 g1(gname1.c_str(), "gaus", fit_low1, fit_high1);
	g1.SetParameter(1, p1);
	h->Fit(&g1, "RQ");

	std::string gname2 = ch.hist->GetName() + std::string("_g2");
	TF1 g2(gname2.c_str(), "gaus", fit_low2, fit_high2);
	g2.SetParameter(1, p2);
	h->Fit(&g2, "RQ");

	ch.peak_low_ch = g1.GetParameter(1);
	ch.peak_low_err = g1.GetParError(1);
	ch.peak_high_ch = g2.GetParameter(1);
	ch.peak_high_err = g2.GetParError(1);

	if (ch.peak_high_ch <= ch.peak_low_ch) {
		std::cout << "  [" << idx << "/" << total << "] "
			<< ch.detector << "_" << ch.face << "_" << ch.strip
			<< "  peak order error -> SKIP" << std::endl;
		return false;
	}

	double dE = kAlphaEnergyHigh - kAlphaEnergyLow;
	double dC = ch.peak_high_ch - ch.peak_low_ch;
	ch.k = dE / dC;
	ch.b = kAlphaEnergyLow - ch.k * ch.peak_low_ch;
	ch.valid = true;

	std::cout << "  [" << idx << "/" << total << "] "
		<< ch.detector << "_" << ch.face << "_" << ch.strip
		<< "  entries=" << entries
		<< "  peak1=" << std::fixed << std::setprecision(1) << ch.peak_low_ch
		<< "±" << std::setprecision(1) << ch.peak_low_err
		<< "  peak2=" << std::setprecision(1) << ch.peak_high_ch
		<< "±" << std::setprecision(1) << ch.peak_high_err
		<< "  k=" << std::setprecision(6) << ch.k
		<< "  b=" << std::setprecision(4) << ch.b
		<< std::endl;

	return true;
}

void WriteResults(const std::string &path, const std::vector<CalibChannel> &channels) {
	std::ofstream out(path);
	out << "# VME Alpha Calibration Results\n";
	out << "# Alpha energies: " << kAlphaEnergyLow << " MeV, "
		<< kAlphaEnergyHigh << " MeV\n";
	out << "# E = k * ADC_channel + b   (MeV)\n";
	out << "#\n";
	out << std::left
		<< std::setw(12) << "detector"
		<< std::setw(8) << "face"
		<< std::setw(7) << "strip"
		<< std::setw(12) << "peak_low_ch"
		<< std::setw(10) << "peak_low_e"
		<< std::setw(12) << "peak_high_ch"
		<< std::setw(10) << "peak_high_e"
		<< std::setw(16) << "k(MeV/ch)"
		<< std::setw(12) << "b(MeV)"
		<< std::setw(8) << "valid"
		<< std::setw(10) << "entries"
		<< "\n";

	out << std::fixed << std::setprecision(4);
	for (const auto &ch : channels) {
		out << std::left
			<< std::setw(12) << ch.detector
			<< std::setw(8) << ch.face
			<< std::setw(7) << ch.strip;
		if (ch.valid) {
			out << std::setw(12) << std::setprecision(1) << ch.peak_low_ch
				<< std::setw(10) << std::setprecision(1) << ch.peak_low_err
				<< std::setw(12) << std::setprecision(1) << ch.peak_high_ch
				<< std::setw(10) << std::setprecision(1) << ch.peak_high_err
				<< std::setw(16) << std::setprecision(6) << ch.k
				<< std::setw(12) << std::setprecision(4) << ch.b
				<< std::setw(8) << "yes";
		} else {
			out << std::setw(12) << "-"
				<< std::setw(10) << "-"
				<< std::setw(12) << "-"
				<< std::setw(10) << "-"
				<< std::setw(16) << "-"
				<< std::setw(12) << "-"
				<< std::setw(8) << "no";
		}
		out << std::setw(10) << ch.hist->GetEntries() << "\n";
	}

	out.close();
	std::cout << "\nResults written to " << path << std::endl;
}

} // namespace

int main(int argc, char **argv) {
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0]
			<< " <vme_run_number> [threshold_path] [data_dir]\n";
		std::cerr << "Example: " << argv[0]
			<< " 255 vme_threshold.toml /data/disk4/vme/root_file\n";
		return -1;
	}

	int vme_run = std::stoi(argv[1]);
	const std::string threshold_path = (argc > 2)
		? argv[2]
		: "vme_threshold.toml";
	const std::string data_dir = (argc > 3)
		? argv[3]
		: "/data/disk4/vme/root_file";

	std::cout << "Run number: " << vme_run << std::endl;
	std::cout << "Threshold file: " << threshold_path << std::endl;
	std::cout << "Data directory: " << data_dir << std::endl;

	VmeThreshold threshold;
	if (LoadVmeThreshold(threshold_path, threshold)) {
		std::cerr << "Failed to load threshold file.\n";
		return -1;
	}
	std::cout << "Thresholds loaded successfully.\n";

	std::vector<CalibChannel> channels;
	RegisterTafdChannels(threshold, channels);
	RegisterT1Channels("t1du", true, threshold.t1du, channels);
	RegisterT1Channels("t1dd", false, threshold.t1dd, channels);
	std::cout << "Registered " << channels.size() << " calibration channels.\n";
	std::cout << "  TAFD: 6 x (16 front + 8 back) = 144\n";
	std::cout << "  T1DU: 1 x (32 front + 32 back) = 64\n";
	std::cout << "  T1DD: 1 x (32 front + 32 back) = 64\n";

	char input_path[512];
	snprintf(input_path, sizeof(input_path), "%s/data%04d.root",
		data_dir.c_str(), vme_run);
	std::cout << "Opening: " << input_path << std::endl;

	TFile input_file(input_path, "read");
	if (input_file.IsZombie()) {
		std::cerr << "Error: Cannot open " << input_path << "\n";
		return -1;
	}

	TTree *tree = static_cast<TTree*>(input_file.Get("tree"));
	if (!tree) {
		std::cerr << "Error: No tree found in " << input_path << "\n";
		return -1;
	}

	VmeBranches input;
	tree->SetBranchAddress("adc", input.adc);

	long long total = tree->GetEntries();
	std::cout << "Total events: " << total << std::endl;
	std::cout << "Filling histograms...\n";

	long long last_pct = 0;
	for (long long entry = 0; entry < total; ++entry) {
		long long pct = entry * 100ll / total;
		if (pct > last_pct) {
			last_pct = pct;
			printf("\r  %3lld%%", last_pct);
			fflush(stdout);
		}

		tree->GetEntry(entry);

		for (auto &ch : channels) {
			int adc = AdcEnergy(input, ch.adc_ref);
			if (adc > ch.threshold + 100 && adc < kAdcSaturation) {
				ch.hist->Fill(adc);
			}
		}
	}
	printf("\r  100%%\n");
	std::cout << "Histogram filling complete.\n\n";

	std::cout << "Calibrating channels...\n";
	int n_valid = 0;
	int total_ch = static_cast<int>(channels.size());
	for (int i = 0; i < total_ch; ++i) {
		if (CalibrateChannel(channels[i], i + 1, total_ch)) {
			++n_valid;
		}
	}

	std::cout << "\nValid calibrations: " << n_valid
		<< " / " << total_ch << std::endl;

	char output_path[256];
	snprintf(output_path, sizeof(output_path),
		"calib_vme_alpha_%04d.txt", vme_run);
	WriteResults(output_path, channels);

	char hist_path[256];
	snprintf(hist_path, sizeof(hist_path),
		"calib_vme_alpha_%04d.root", vme_run);
	TFile hist_file(hist_path, "recreate");
	for (auto &ch : channels) {
		ch.hist->Write();
	}
	hist_file.Close();
	std::cout << "Histograms saved to " << hist_path << std::endl;

	for (auto &ch : channels) {
		delete ch.hist;
	}

	input_file.Close();
	return 0;
}