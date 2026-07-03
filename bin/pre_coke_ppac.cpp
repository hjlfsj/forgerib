#include <TFile.h>
#include <TTree.h>
#include <TString.h>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include "include/event/ore/raw_trigger_event.h"
#include "include/event/ore/raw_ppac_event.h"

using namespace forgerib;

int main(int argc, char** argv)
{
    int run = -1;
    if (argc > 1) run = atoi(argv[1]);

    TString trig_path, ppac_path, out_path;
    if (run >= 0) {
        trig_path = Form("/data/disk1/ribll2026_www_data/grit/trigger_%04d.root", run);
        ppac_path = Form("/data/disk1/ribll2026_www_data/grit/ppac_%04d.root", run);
        out_path = Form("/data/disk1/ribll2026_www_data/grit/trigger_ppac_%04d.root", run);
    } else {
        trig_path = "trigger.root";
        ppac_path = "ppac.root";
        out_path = "trigger_ppac.root";
    }

    TFile* f_trig = TFile::Open(trig_path, "READ");
    if (!f_trig || f_trig->IsZombie()) {
        printf("Error: cannot open %s\n", trig_path.Data());
        return 1;
    }
    TTree* t_trig = (TTree*)f_trig->Get("tree");
    if (!t_trig) {
        printf("Error: tree not found in %s\n", trig_path.Data());
        return 1;
    }
    RawTriggerEvent trig_evt;
    SetupInput(t_trig, trig_evt);
    Long64_t n_trig = t_trig->GetEntries();
    Long64_t i = 0;
    if (n_trig > 0) t_trig->GetEntry(0);

    TFile* f_ppac = TFile::Open(ppac_path, "READ");
    if (!f_ppac || f_ppac->IsZombie()) {
        printf("Error: cannot open %s\n", ppac_path.Data());
        return 1;
    }
    TTree* t_ppac = (TTree*)f_ppac->Get("tree");
    if (!t_ppac) {
        printf("Error: tree not found in %s\n", ppac_path.Data());
        return 1;
    }
    RawPpacEvent ppac_evt;
    SetupInput(t_ppac, ppac_evt);
    Long64_t n_ppac = t_ppac->GetEntries();
    
    std::vector<RawTriggerEvent> ppac_valid;
    ppac_valid.reserve(n_ppac / 5);


    for (Long64_t k = 0; k < n_ppac; ++k) {
        t_ppac->GetEntry(k);
        if (ppac_evt.index == 0 && ppac_evt.channel == 4) {
            RawTriggerEvent tmp;
            tmp.type = 6;
            tmp.time = ppac_evt.time;
            tmp.external_time = -1;
            tmp.cv = ppac_evt.cv;
            ppac_valid.push_back(tmp);
        }

        if (k * 100 / n_ppac != (k - 1) * 100 / n_ppac) {
            printf("\rReading PPAC events   %lld%%", k * 100 / n_ppac);
            fflush(stdout);
        }
        
    }
    Long64_t j = 0;
    Long64_t n_ppac_valid = ppac_valid.size();

    TFile* f_out = TFile::Open(out_path, "RECREATE");
    TTree* t_out = new TTree("tree", "merged events");
    RawTriggerEvent out_evt;
    SetupOutput(t_out, out_evt);

    Long64_t total = n_trig + n_ppac_valid;
    Long64_t processed = 0;
    int last_pct = -1;
    char bar[51];
    bar[50] = '\0';

    auto update_progress = [&]() {
        int pct = total == 0 ? 100 : int(processed * 100 / total);
        if (pct == last_pct) return;
        last_pct = pct;
        int fill = pct / 2;
        for (int k = 0; k < 50; ++k) bar[k] = k < fill ? '=' : ' ';
        if (fill > 0 && fill < 50) bar[fill-1] = '>';
        printf("\r[%s] %3d%%  %lld / %lld", bar, pct, processed, total);
        fflush(stdout);
    };

    while (i < n_trig && j < n_ppac_valid) {
        if (trig_evt.time <= ppac_valid[j].time) {
            out_evt = trig_evt;
            t_out->Fill();
            ++i; ++processed;
            if (i < n_trig) t_trig->GetEntry(i);
        } else {
            out_evt = ppac_valid[j];
            t_out->Fill();
            ++j; ++processed;
        }
        update_progress();
    }
    while (i < n_trig) {
        out_evt = trig_evt;
        t_out->Fill();
        ++i; ++processed;
        if (i < n_trig) t_trig->GetEntry(i);
        update_progress();
    }
    while (j < n_ppac_valid) {
        out_evt = ppac_valid[j];
        t_out->Fill();
        ++j; ++processed;
        update_progress();
    }

    printf("\nDone. Output: %s\n", out_path.Data());

    t_out->Write();
    f_out->Close();
    f_trig->Close();
    f_ppac->Close();

    delete f_out;
    delete f_trig;
    delete f_ppac;

    return 0;
}