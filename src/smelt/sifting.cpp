#include "include/smelt/sifting.h"

#include <iostream>
#include <iomanip>
#include <algorithm>

#include <TGraph.h>
#include <TString.h>
#include <TFile.h>
#include <TTree.h>
#include <TH1F.h>
#include <TF1.h>

#include "include/event/ingot/align_event.h"

namespace forgerib {

int Sift(
	const std::vector<double> &xia_times,
	const std::vector<long long> &xia_entries,
	const std::vector<double> &vme_times,
	const std::vector<long long> &vme_entries,
	const char *output_path,
	const double window,
	const size_t range,
	const size_t vme_window,
	const size_t xia_window,
	bool report
) {
	// output file
	TFile opf(output_path, "recreate");
	// offset-group graph in first, second. third match
	TGraph offset_vs_group[3];
	// window-group graph, in first, second, third match
	TGraph window_vs_group[3];
	// output tree
	TTree opt("tree", "alignment result");
	AlignEvent event;
	SetupOutput(&opt, event);

	std::vector<double> vme_dt;
	for (size_t i = 1; i < vme_window; ++i) {
		vme_dt.push_back(vme_times[i] - vme_times[i-1]);
	}
    size_t anchor_xia = -1;
	size_t anchor_vme = -1;
	bool found = false;
	for (anchor_xia = 2; anchor_xia < xia_window; ++anchor_xia) {
		double xia_dt0 = xia_times[anchor_xia-1] - xia_times[anchor_xia-2];
		double xia_dt1 = xia_times[anchor_xia] - xia_times[anchor_xia-1];
		for (anchor_vme = 0; anchor_vme < vme_window-2; ++anchor_vme) {
			if (
				fabs(vme_dt[anchor_vme]-xia_dt0) <  window
				&& fabs(vme_dt[anchor_vme+1]-xia_dt1) < window
			) {
				found = true;
				anchor_vme += 2;
				break;
			}
		}
		if (found) break;
	}
	if (!found) {
		std::cerr << "Error: Could not find initial anchor in XIA window "
			<< xia_window << ", VME window " << vme_window << ".\n";
		return -1;
	}

	if (report) {
		std::cout << "Anchor at XIA entry " << anchor_xia << ", VME entry " << anchor_vme << "\n";
	}

	size_t align_events = 0;
	// fill first three events
	for (int i = 2; i >= 0; --i) {
		event.vme_entry = vme_entries[anchor_vme-i];
		event.vme_time = vme_times[anchor_vme-i];
		event.xia_entry = xia_entries[anchor_xia-i];
		event.xia_time = xia_times[anchor_xia-i];
		opt.Fill();
		++align_events;
	}

	if (report) {
		printf("Sifting   0%%");
		fflush(stdout);
	}
	size_t last_percentage = 0;
	// search residual events
	bool is_lookhead = true;
	while (anchor_xia < xia_times.size()-1 && anchor_vme < vme_times.size()-1 && is_lookhead) {
		if (report && anchor_xia * 100 / xia_times.size() > last_percentage) {
			last_percentage = anchor_xia * 100 / xia_times.size();
			printf("\b\b\b\b%3lu%%", last_percentage);
			fflush(stdout);
		}
		bool match = false;
		for (size_t i = anchor_xia+1; i <= anchor_xia+range; ++i) {
			double xia_dt = xia_times[i] - xia_times[anchor_xia];
			for (size_t j = anchor_vme+1; j <= anchor_vme+range; ++j) {
				double vme_dt = vme_times[j] - vme_times[anchor_vme];
				if (fabs(xia_dt - vme_dt) < window) {
					event.vme_entry = vme_entries[j];
					event.vme_time = vme_times[j];
					event.xia_entry = xia_entries[i];
					event.xia_time = xia_times[i];
					opt.Fill();
					++align_events;
					anchor_vme = j;
					anchor_xia = i;
					match = true;
					break;
				}
			}
			if (match) break;
		}
		if (!match && !(anchor_xia+vme_window < xia_times.size()-1 && anchor_vme+xia_window < vme_times.size()-1))
		{
			is_lookhead = false;
		}
		found = false;
		while (!match && !found && anchor_xia+vme_window < xia_times.size()-1 && anchor_vme+xia_window < vme_times.size()-1)
		{
			std::cout << "Matching stalled at XIA idx " << anchor_xia << ". Re-anchoring..." << std::endl;
			size_t ref_xia = anchor_xia + 1;
	        size_t ref_vme = anchor_vme + 1;
			found = false;
			for (anchor_xia = ref_xia+2; anchor_xia < xia_window+ref_xia; ++anchor_xia)
			{
				double xia_dt0 = xia_times[anchor_xia-1] - xia_times[anchor_xia-2];
				double xia_dt1 = xia_times[anchor_xia] - xia_times[anchor_xia-1];
				for (anchor_vme = ref_vme; anchor_vme < vme_window+ref_vme-2; ++anchor_vme)
				{
					double vme_dt0 = vme_times[anchor_vme+1] - vme_times[anchor_vme];
					double vme_dt1 = vme_times[anchor_vme+2] - vme_times[anchor_vme+1];
					if (fabs(vme_dt0-xia_dt0) <  window && fabs(vme_dt1-xia_dt1) < window)
					{
						found = true;
						anchor_vme += 2;
						break;
					}
				}
				if (found)
				{
					for (int i = 2; i >= 0; --i)
					{
						event.vme_entry = vme_entries[anchor_vme-i];
						event.vme_time = vme_times[anchor_vme-i];
						event.xia_entry = xia_entries[anchor_xia-i];
						event.xia_time = xia_times[anchor_xia-i];
						opt.Fill();
						++align_events;
					}
				std::cerr << "Find next anchor: " <<".\n";
				// return -1;
				// break;
				std::cout << "Anchor at XIA entry " << anchor_xia << ", VME entry " << anchor_vme << "\n";
				break;
				}
			}

			if (!found)
			{
				std::cerr << "Could not find anchor in XIA window "
				<< xia_window << ", VME window " << vme_window << ".\n";
				// return -1;
				// break;
				std::cout << "Anchor at XIA entry " << anchor_xia << ", VME entry " << anchor_vme << "\n";
				std::cout << "try next window" << std::endl;
			}
		}

	}
	if (report) printf("\b\b\b\b100%%\n");

	opf.cd();
	opt.Write();
	opf.Close();

	if (report) {
		std::cout
			<< "XIA align rate "
			<< align_events << " / " << xia_times.size() << " "
			<< double(align_events) / double(xia_times.size()) << "\n"
			<< "VME align rate "
			<< align_events << " / " << vme_times.size() << " "
			<< double(align_events) / double(vme_times.size()) << "\n";
	}
	return 0;
}


}
