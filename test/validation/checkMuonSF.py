"""Recomputes the muon SF event weights of validateMuonSF_cfg.py output independently (lepton-to-muon matching, HLT
matching from the MiniAOD with FWLite, correctionlib in Python, the triggering muon's trigger SF) and compares every column.
Usage: python3 checkMuonSF.py sf.root <analysis yaml> default|hybrid <MiniAOD input file>"""
import fnmatch
import os
import sys

import correctionlib
import numpy as np
import uproot
import yaml

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import muonRecipes  # noqa: E402

a = uproot.open(sys.argv[1])['Events'].arrays(library='np')
with open(sys.argv[2]) as f:
    cfg = muonRecipes.RECIPES[sys.argv[3]](yaml.safe_load(f))
sets = {}
ok = True


def hltObjects(path):
    """{(run, lumi, event): (accepted path names, [(eta, phi, paths whose last filter the object passed)])}."""
    import ROOT  # noqa: F401
    from DataFormats.FWLite import Events, Handle
    out, hT, hO = {}, Handle('edm::TriggerResults'), Handle('std::vector<pat::TriggerObjectStandAlone>')
    for e in Events(path):
        e.getByLabel(('TriggerResults', '', cfg['inputs']['hltProcess']), hT)
        e.getByLabel(cfg['inputs']['triggerObjects'], hO)
        names = e.object().triggerNames(hT.product())
        allNames = [names.triggerName(i) for i in range(hT.product().size())]
        accepted = {n for i, n in enumerate(allNames) if hT.product().accept(i)}
        objs = []
        for o in hO.product():
            o.unpackPathNames(names)
            objs.append((o.eta(), o.phi(), {n for n in accepted if o.hasPathName(n, True, True)}))
        a_ = e.eventAuxiliary()
        out[(a_.run(), a_.luminosityBlock(), a_.event())] = (accepted, objs)
    return out


hlt = hltObjects(sys.argv[4])


def corr(t):
    if t['file'] not in sets:
        sets[t['file']] = correctionlib.CorrectionSet.from_file(t['file'])
    return sets[t['file']][t['key']]


def evaluate(t, pt, eta, what):
    c = corr(t)
    args = [{'eta': eta, 'abseta': abs(eta), 'pt': pt, 'p': pt * np.cosh(eta), 'scale_factors': what}[i.name] for i in c.inputs]
    try:
        return c.evaluate(*args)
    except Exception as e:
        if t['outOfRange'] == 'skip' and 'bounds' in str(e):
            return None
        raise


def hltMatched(t, eta, phi, key):
    accepted, objs = hlt[key]
    fired = {n for n in accepted if any(fnmatch.fnmatchcase(n, g) for g in t['paths'])}
    dphi = lambda a, b: (a - b + np.pi) % (2 * np.pi) - np.pi
    return any(np.hypot(eta - oe, dphi(phi, op)) < 0.1 and fired & ps for oe, op, ps in objs)


def triggering(terms, muons, key):
    """Index of the highest-pt muon matched to its regime's trigger term, or None."""
    cands = [(m, t) for m in range(len(muons)) for t in terms if t['trigger'] and (t['regime'] == 'high') == muons[m][3]]
    matched = [m for m, t in cands if hltMatched(t, muons[m][1], muons[m][2], key)]
    return max(matched, key=lambda m: muons[m][0]) if matched else None


def weight(terms, muons, trig, shifted=None, unc=None, sign=0):
    w = 1.0
    for i, (pt, eta, phi, high) in enumerate(muons):
        for t in terms:
            if (t['regime'] == 'high') != high or (t['trigger'] and i != trig):
                continue
            sf = evaluate(t, pt, eta, 'nominal')
            if sf is None:
                continue
            if t['name'] == shifted:
                sf += sign * evaluate(t, pt, eta, unc)
            w *= sf
    return w


for ch, terms in cfg['selection']['reco']['muonScaleFactors'].items():
    cols = ['nominal'] + ['%s_%s%s' % (t['name'], u, s) for t in terms for u in ('stat', 'syst') for s in ('Up', 'Down')]
    maxdiff, nSel, nHigh, nMixed, nTrigNotFirst, nNoMatch = 0.0, 0, 0, 0, 0, 0
    for i in range(len(a['event'])):
        leps = list(zip(a[ch + 'Lep_pt'][i], a[ch + 'Lep_eta'][i], a[ch + 'Lep_phi'][i], a[ch + 'Lep_charge'][i]))
        if not a['Selection_%s' % ch][i]:
            leps = []
        mus = list(zip(a['CorrMu_pt'][i], a['CorrMu_eta'][i], a['CorrMu_phi'][i], a['CorrMu_charge'][i], a['CorrMu_regime'][i]))
        matched = []
        for pt, eta, phi, q in leps:
            dphi = lambda p: (p - phi + np.pi) % (2 * np.pi) - np.pi
            cands = [(np.hypot(m[1] - eta, dphi(m[2])), m) for m in mus if m[3] == q]
            dr, m = min(cands, key=lambda x: x[0])
            assert dr < 0.1
            matched.append((m[0], m[1], m[2], bool(m[4])))
        nSel += bool(matched)
        regimes = {h for _, _, _, h in matched}
        key = (a['run'][i], a['luminosityBlock'][i], a['event'][i])
        trig = triggering(terms, matched, key) if matched else None
        nNoMatch += bool(matched) and trig is None
        nTrigNotFirst += trig is not None and len(matched) > 1 and any(
            hltMatched(t, m[1], m[2], key) for j, m in enumerate(matched) if j != trig for t in terms
            if t['trigger'] and (t['regime'] == 'high') == m[3])
        nHigh += True in regimes
        nMixed += len(regimes) == 2
        for c in cols:
            if c == 'nominal':
                exp = weight(terms, matched, trig)
            else:
                name, rest = c.split('_')
                unc, s = rest[:4], (1 if rest.endswith('Up') else -1)
                exp = weight(terms, matched, trig, name, unc, s)
            got = a['%sMuonSF_%s' % (ch, c)][i]
            maxdiff = max(maxdiff, abs(got - exp) / exp)
    nontrivial = np.mean(a['%sMuonSF_nominal' % ch][a['Selection_%s' % ch]] != 1.0) if nSel else 0
    good = nSel >= 10 and maxdiff < 1e-6 and nontrivial > 0.99 and (sys.argv[3] == 'default' or nHigh > 10)
    ok &= good
    print('%s %s: %d events with selected muons (%d with a high-regime muon, %d mixed, %d with another HLT-matched muon '
          'besides the triggering one, %d with none matched), %d columns, weight != 1 in %.3f, max rel diff %.1e'
          % ('PASS' if good else 'FAIL', ch, nSel, nHigh, nMixed, nTrigNotFirst, nNoMatch, len(cols), nontrivial, maxdiff))
    noSel = ~a['Selection_%s' % ch]
    ones = all(np.all(a['%sMuonSF_%s' % (ch, c)][noSel] == 1.0) for c in cols)
    ok &= ones
    print('%s %s: unit weights without selected leptons' % ('PASS' if ones else 'FAIL', ch))
sys.exit(0 if ok else 1)
