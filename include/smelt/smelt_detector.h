
#ifndef __FUSE_DETECTOR_H__
#define __FUSE_DETECTOR_H__

#include <iostream>

#include <TFile.h>
#include <TTree.h>

#include "include/event/ingot/align_event.h"

namespace forgerib {

template<typename Event>
int SmeltDetector(
	const std::vector<long long> vme_entries,
	const char *vme_path,
	const char *output_path,
	const char *detector_name,
	bool report
) {
	TFile vme_file(vme_path, "read");
	TTree *vme_tree = (TTree*)vme_file.Get("tree");
	if (!vme_tree) {
		std::cerr << "Error: Get tree from " << vme_path << " failed.\n";
		return -2;
	}
	Event vme_event;
	SetupInput(vme_tree, vme_event);

	TFile opf(output_path, "recreate");
	TTree opt("tree", "fused T1 DSSD");
	Event event;
	SetupOutput(&opt, event);


	if (report) {
		printf("Fusing %s   0%%", detector_name);
		fflush(stdout);
	}
	long long total = (long long)vme_entries.size();
	long long last_percentage = 0;
	for (long long entry = 0; entry < total; ++entry) {
		if (report && entry * 100ll / total > last_percentage) {
			last_percentage = entry * 100 / total;
			printf("\b\b\b\b%3lld%%", last_percentage);
			fflush(stdout);
		}
		// fuse vme events
		if (vme_entries[entry] >= 0) {
			vme_tree->GetEntry(vme_entries[entry]);
			event = vme_event;
		} else {
			Reset(event);
		}
		opt.Fill();
	}
	if (report) printf("\b\b\b\b100%%\n");

	opf.cd();
	opt.Write();
	opf.Close();
	vme_file.Close();
	return 0;
}

}


#endif
