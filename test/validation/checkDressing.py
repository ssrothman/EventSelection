"""Checks validateDressing_cfg.py output.
Usage: python3 checkDressing.py fsr|cone dressing.root miniaod.root
 fsr:  dressed - undressed = the lowest-dR/ET^2 FsrPhoton linked to the lepton's index in slimmedMuons/Electrons
 cone: dressed = undressed + an independent FWLite recomputation (PF photons within 0.1 of the nearest tight lepton,
       outside every electron's footprint, PUPPI-weighted)"""
import sys

import numpy as np
import uproot

mode, nano, mini = sys.argv[1:4]
a = uproot.open(nano)['Events'].arrays(library='np')
ok = True


def report(name, cond, detail):
    global ok
    ok &= bool(cond)
    print(('PASS ' if cond else 'FAIL ') + name + ': ' + detail)


def p4(pt, eta, phi, m):
    px, py, pz = pt * np.cos(phi), pt * np.sin(phi), pt * np.sinh(eta)
    return np.stack([px, py, pz, np.sqrt(px ** 2 + py ** 2 + pz ** 2 + m ** 2)], axis=-1)


def table(name, e):
    get = lambda v: a['%s_%s' % (name, v)][e].astype(float)
    # FsrPhoton stores no mass (photons).
    return p4(get('pt'), get('eta'), get('phi'), get('mass') if '%s_mass' % name in a else 0.0)


def compare(name, refs):
    """refs[e] = expected dressed p4 for each lepton in event e."""
    worst, n, changed = 0.0, 0, 0
    for e, ref in enumerate(refs):
        got = table(name + 'Dressed', e)
        und = table(name + 'Undressed', e)
        if len(got) != len(ref):
            report(name, False, 'event %d: %d dressed vs %d expected' % (e, len(got), len(ref)))
            return
        for g, r, u in zip(got, ref, und):
            n += 1
            changed += not np.allclose(g, u, rtol=1e-6, atol=1e-6)
            worst = max(worst, np.max(np.abs(g - r)) / max(r[3], 1e-9))
    # Tables are float: allow 1e-5 of the lepton energy.
    report('%s %s dressing' % (mode, name), n > 0 and worst < 1e-5 and changed > 0,
           'leptons %d, dressed (changed) %d, max |diff|/E %.1e' % (n, changed, worst))


if mode == 'fsr':
    for name, coll, idxCol in (('tightMuons', 'AllMuons', 'muonIdx'), ('tightElectrons', 'AllElectrons', 'electronIdx')):
        refs = []
        for e in range(len(a['event'])):
            und = table(name + 'Undressed', e)
            allp = table(coll, e)
            fsr = table('FsrPhoton', e) if 'FsrPhoton_pt' in a else np.zeros((0, 4))
            owner, dret = a['FsrPhoton_' + idxCol][e], a['FsrPhoton_dROverEt2'][e]
            ref = []
            for u in und:
                i = int(np.argmin(np.abs(allp - u).sum(axis=1)))
                cand = np.nonzero(owner == i)[0]
                ref.append(u + (fsr[cand[np.argmin(dret[cand])]] if len(cand) else 0))
            refs.append(ref)
        compare(name, refs)
else:
    import ROOT
    from DataFormats.FWLite import Events, Handle
    hpf, hel = Handle('std::vector<pat::PackedCandidate>'), Handle('std::vector<pat::Electron>')
    evIndex = {int(n): i for i, n in enumerate(a['event'])}
    refs = {'tightMuons': [None] * len(evIndex), 'tightElectrons': [None] * len(evIndex)}
    for ev in Events(mini):
        n = ev.eventAuxiliary().event()
        if n not in evIndex:
            continue
        i = evIndex[n]
        ev.getByLabel('packedPFCandidates', hpf)
        ev.getByLabel('slimmedElectrons', hel)
        pf = hpf.product()
        foot = set()
        for el in hel.product():
            for r in el.associatedPackedPFCandidates():
                foot.add(r.key())
        ph = [(k, pf[k]) for k in range(pf.size()) if pf[k].pdgId() == 22 and pf[k].pt() > 0.0 and k not in foot
              and pf[k].puppiWeight() > 0]
        for name in refs:
            und = table(name + 'Undressed', i)
            eta = np.arcsinh(und[:, 2] / np.hypot(und[:, 0], und[:, 1])) if len(und) else []
            phi = np.arctan2(und[:, 1], und[:, 0]) if len(und) else []
            dressed = und.copy()
            for k, c in ph:
                if not len(und):
                    break
                dR = np.hypot(eta - c.eta(), np.remainder(phi - c.phi() + np.pi, 2 * np.pi) - np.pi)
                j = int(np.argmin(dR))
                if dR[j] < 0.1:
                    w = c.puppiWeight()
                    dressed[j] += w * np.array([c.px(), c.py(), c.pz(), c.energy()])
            refs[name][i] = dressed
    for name, r in refs.items():
        compare(name, r)
sys.exit(0 if ok else 1)
