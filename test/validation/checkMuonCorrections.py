"""Compares validateMuonCorrections_cfg.py output with the MUO POG Python reference implementation (MuonScaRe.py).
The resolution variation with k = sqrt(k_data^2 - k_mc^2) (as in MuonScaRe.cc) is compared with that formula written
out here, since MuonScaRe.py uses k = k_mc. The validation config must use highPtThreshold 200 (the Python cut).
Usage: python3 checkMuonCorrections.py mu.root <muon_scalesmearing.json.gz>"""
import sys

import numpy as np
import uproot

sys.path.insert(0, '/cvmfs/cms.cern.ch/rsync/cms-nanoAOD/jsonpog-integration/examples')
import correctionlib  # noqa: E402
import MuonScaRe as ref  # noqa: E402

a = uproot.open(sys.argv[1])['Events'].arrays(library='np')
cset = correctionlib.CorrectionSet.from_file(sys.argv[2])
import awkward as ak  # noqa: E402

counts = np.array([len(x) for x in a['Mu_pt']])
# The reference's non-nested code path is broken (get_rndm uses an undefined evtNr_f), so use per-event arrays.
jag = lambda k: ak.unflatten(np.concatenate(list(a[k])).astype(float), counts)
pt, eta, phi = jag('Mu_pt'), jag('Mu_eta'), jag('Mu_phi')
q, nL = ak.values_astype(jag('Mu_charge'), np.int64), jag('Mu_nTrackerLayers')
evt, lumi = a['event'].astype(np.uint64), a['luminosityBlock'].astype(np.uint64)

ptScaleMC = ref.pt_scale(0, pt, eta, phi, q, cset, nested=True)
ptMC = ref.pt_resol(ptScaleMC, eta, phi, nL, evt, lumi, cset, nested=True)
expected = dict(MCnominal=ptMC, Datanominal=ref.pt_scale(1, pt, eta, phi, q, cset, nested=True),
                MCscaleUp=ref.pt_scale_var(ptMC, eta, phi, q, 'up', cset, nested=True),
                MCscaleDown=ref.pt_scale_var(ptMC, eta, phi, q, 'dn', cset, nested=True),
                MCresolutionUpPython=ref.pt_resol_var(ptScaleMC, ptMC, eta, 'up', cset, nested=True),
                MCresolutionDownPython=ref.pt_resol_var(ptScaleMC, ptMC, eta, 'dn', cset, nested=True))


def resolVarCpp(ptNo, ptRes, eta, sign):
    """pt_resol_var of MuonScaRe.cc, on flat arrays."""
    ae = np.abs(eta)
    kd, km = cset['k_data'].evaluate(ae, 'nom'), cset['k_mc'].evaluate(ae, 'nom')
    k = np.where(km < kd, np.sqrt(np.maximum(kd ** 2 - km ** 2, 0)), 0.0)
    kUnc = cset['k_mc'].evaluate(ae, 'stat')
    with np.errstate(divide='ignore', invalid='ignore'):
        v = ptNo * (1 + (k + sign * kUnc) * (ptRes / ptNo - 1) / k)
    v = np.where(k == 0, ptRes, v)
    return np.where((v / ptNo > 2) | (v / ptNo < 0.1) | (v < 0), ptNo, v)


fl = lambda x: ak.to_numpy(ak.flatten(x)).astype(float)
for name, sign in (('Up', 1), ('Down', -1)):
    expected['MCresolution%sCpp' % name] = ak.unflatten(
        resolVarCpp(fl(ptScaleMC), fl(ptMC), fl(eta), sign), counts)
flat = lambda k: np.concatenate(list(a[k])).astype(float)
pt = flat('Mu_pt')
ok = True
inWindow = (pt > 26) & (pt < 200)
for name, exp in expected.items():
    got = flat('MuCorr%s_pt' % name)
    rel = np.abs(got / ak.to_numpy(ak.flatten(exp)).astype(float) - 1)
    changed = np.mean(np.abs(got / pt - 1) > 1e-9)
    good = len(got) > 100 and rel.max() < 1e-9 and changed > 0
    ok &= good
    print('%s %-17s muons %d (26<pt<200: %d), changed %.2f, max |ours/ref - 1| %.1e' %
          ('PASS' if good else 'FAIL', name, len(got), inWindow.sum(), changed, rel.max()))
sys.exit(0 if ok else 1)
