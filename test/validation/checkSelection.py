"""Checks validateSelection_cfg.py output against an independent FWLite Z->mumu selection.
Usage: python3 checkSelection.py sel.root miniaod.root"""
import sys
import numpy as np
import uproot
import ROOT
from DataFormats.FWLite import Events, Handle

a = uproot.open(sys.argv[1])['Events'].arrays(library='np')
ok = True


def report(name, cond, detail):
    global ok
    ok &= bool(cond)
    print(('PASS ' if cond else 'FAIL ') + name + ': ' + detail)


n = len(a['Selection_goodVertex'])
for ch in ['Zmumu', 'Zee', 'Wmunu', 'Wenu', 'Hadronic', 'GammaJets']:
    comb = a['Selection_' + ch]
    parts = a['Selection_goodVertex'] & a['Selection_metFilters'] & a['Selection_' + ch + 'Trigger'] & a['Selection_' + ch + 'Sel']
    report(ch + ' flag = AND of components', np.array_equal(comb, parts),
           'pass %d / %d (trigger %d, sel %d)' % (comb.sum(), n, a['Selection_' + ch + 'Trigger'].sum(), a['Selection_' + ch + 'Sel'].sum()))
for ch in ['Zmumu', 'Zee']:
    m = np.concatenate(list(a[ch + 'Boson_mass']))
    nb = np.array([len(x) for x in a[ch + 'Boson_mass']])
    report(ch + ' boson mass window and count', np.all((m >= 71) & (m <= 111)) and np.array_equal(nb == 1, a['Selection_' + ch + 'Sel']),
           'n=%d mean mass %.2f' % (len(m), m.mean() if len(m) else 0))

# Independent Z->mumu (FWLite): trigger, good PV, MET filters, OS tight muons 26/15, mass closest to mZ in [71, 111].
# PyROOT turns the Selector enum into an int, which picks the bitmask overload passed(uint64_t); pass the mask.
TIGHT_ISO_MASK = (1 << int(ROOT.reco.Muon.CutBasedIdTight)) | (1 << int(ROOT.reco.Muon.PFIsoTight))
hm, hv, ht = Handle('std::vector<pat::Muon>'), Handle('std::vector<reco::Vertex>'), Handle('edm::TriggerResults')
flt = ['Flag_goodVertices', 'Flag_globalSuperTightHalo2016Filter', 'Flag_EcalDeadCellTriggerPrimitiveFilter',
       'Flag_BadPFMuonFilter', 'Flag_BadPFMuonDzFilter', 'Flag_hfNoisyHitsFilter', 'Flag_eeBadScFilter', 'Flag_ecalBadCalibFilter']
ref = []
for e in Events(sys.argv[2]):
    if len(ref) == n:
        break
    e.getByLabel(('TriggerResults', '', 'HLT'), ht)
    tn = e.object().triggerNames(ht.product())
    trig = any(ht.product().accept(i) for i in range(tn.size()) if tn.triggerName(i).startswith('HLT_IsoMu24_v'))
    e.getByLabel(('TriggerResults', '', 'PAT'), ht)
    tn = e.object().triggerNames(ht.product())
    mf = all(ht.product().accept(tn.triggerIndex(f)) for f in flt)
    e.getByLabel('offlineSlimmedPrimaryVertices', hv)
    v = hv.product()
    pv = v.size() > 0 and not v[0].isFake() and v[0].ndof() > 4 and abs(v[0].z()) < 24 and v[0].position().rho() < 2
    e.getByLabel('slimmedMuons', hm)
    mus = [m for m in hm.product() if m.pt() > 15 and abs(m.eta()) < 2.4 and m.passed(TIGHT_ISO_MASK)]
    best = None
    for i in range(len(mus)):
        for j in range(i + 1, len(mus)):
            a1, b1 = mus[i], mus[j]
            if a1.charge() * b1.charge() >= 0 or max(a1.pt(), b1.pt()) < 26 or min(a1.pt(), b1.pt()) < 15:
                continue
            mass = (a1.p4() + b1.p4()).M()
            if 71 <= mass <= 111 and (best is None or abs(mass - 91.1876) < abs(best - 91.1876)):
                best = mass
    ref.append(trig and mf and pv and best is not None)
ref = np.array(ref)
report('Zmumu vs independent FWLite selection', np.array_equal(ref, a['Selection_Zmumu']),
       'ref %d, ours %d, disagreements %d' % (ref.sum(), a['Selection_Zmumu'].sum(), (ref != a['Selection_Zmumu']).sum()))
sys.exit(0 if ok else 1)
