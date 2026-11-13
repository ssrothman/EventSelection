"""Checks validateHighPtMuons_cfg.py output with an independent implementation of the high-pT muon recipe:
regime, TuneP momentum and originalPt per mode; exact kappa_b shifts (data and MC, both targets, scale variations) reusing the
nominal smearing's standard-normal numbers; and the nominal and systematic smearing pulls ~ N(0, 1).
Usage: python3 checkHighPtMuons.py hp.root <era>"""
import sys

import numpy as np
import uproot
from SRothman.EventSelection.muonHighPtRun3_cff import ERAS

a = uproot.open(sys.argv[1])['Events'].arrays(library='np')
T = ERAS[sys.argv[2]]
flat = lambda k: np.concatenate(list(a[k])).astype(float)
pt, eta, phi, q, tp = (flat('Mu_' + k) for k in ('pt', 'eta', 'phi', 'charge', 'tunePPt'))
ok = True


def report(name, cond, detail=''):
    global ok
    ok &= bool(cond)
    print(('PASS ' if cond else 'FAIL ') + name + (': ' + detail if detail else ''))


def got(v):
    return flat('MuCorr%s_pt' % v), flat('MuCorr%s_regime' % v)


inAcc = np.abs(eta) < 2.4
eb = np.clip(np.searchsorted([-2.1, -1.2, 0.0, 1.2, 2.1], eta, side='right'), 0, 5)
deg = np.degrees(phi)
pb = np.where(deg < -60, 0, np.where(deg < 60, 1, 2))
kappa = np.where(inAcc, np.array(T['kappa'])[pb * 6 + eb], 0.0)
dkappa = np.where(inAcc, np.array(T['kappaUnc'])[pb * 6 + eb], 0.0)
barrel = np.abs(eta) < 1.2
f = np.where(barrel, T['smearBarrel'], T['smearEndcap'])


def sigma(p):
    c = np.where(barrel[:, None], np.array(T['sigmaBarrel'])[None, :], np.array(T['sigmaEndcap'])[None, :])
    return c[:, 0] + c[:, 1] * p + c[:, 2] * p ** 2 + c[:, 3] * p ** 3


def shift(x, k):
    return 1.0 / (1.0 / x + q * k / 1000.0)


hybridHigh = pt >= 200
tuneP = np.where(tp > 0, tp, pt)
corrected = hybridHigh & (tuneP >= 200) & inAcc
print('muons %d, hybrid high regime %d, corrected (TuneP >= 200, |eta| < 2.4) %d, smeared (f > 0) %d'
      % (len(pt), hybridHigh.sum(), corrected.sum(), (corrected & (f > 0)).sum()))
report('there are corrected high-pT muons with and without smearing',
       corrected.sum() > 50 and (corrected & (f > 0)).sum() > 20 and (corrected & (f == 0)).sum() > 0 if T['smearBarrel'] == 0
       else corrected.sum() > 50)

# originalPt is a float userFloat, so it equals the input pt rounded to float.
for v in ('PF', 'TuneP', 'Hybrid', 'MCnominal', 'DataTargetData'):
    report('%s originalPt is the input pt' % v, np.array_equal(flat('MuCorr%s_originalPt' % v), pt.astype(np.float32)))
report('MCnominal changes pt where originalPt does not', (got('MCnominal')[0] != pt).sum() > 50)

for v, exp_pt, exp_reg in (('PF', pt, 0 * pt), ('TuneP', tuneP, 1 + 0 * pt),
                           ('Hybrid', np.where(hybridHigh, tuneP, pt), hybridHigh)):
    p, r = got(v)
    report('%s momentum and regime' % v, np.array_equal(r, exp_reg) and np.allclose(p, exp_pt, rtol=1e-12, atol=0))

close = lambda x, y, sel: np.max(np.abs(x[sel] / y[sel] - 1)) if sel.any() else 0.0
untouched = hybridHigh & ~corrected
for v in ('MCnominal', 'MCscaleUp', 'MCresolution', 'MCtargetData', 'DataTargetMC', 'DataTargetData'):
    p, r = got(v)
    report('%s: regime as hybrid; high-regime muons outside the correction unchanged' % v,
           np.array_equal(r, hybridHigh) and close(p, tuneP, untouched) < 1e-12)

# Data
p, _ = got('DataTargetMC')
report('data, kappaTarget mc: TuneP pt unchanged', close(p, tuneP, corrected) < 1e-12)
p, _ = got('DataTargetData')
d = close(p, shift(tuneP, -kappa), corrected)
report('data, kappaTarget data: q/pt - kappa_b', d < 1e-9 and close(p, tuneP, corrected) > 1e-4, 'max rel diff %.1e' % d)

# MC nominal: kappa_b shift, then smearing; recover the standard-normal number g of each smeared muon.
nom, _ = got('MCnominal')
pk = shift(tuneP, kappa)
smeared = corrected & (f > 0)
noSmear = corrected & (f == 0)
report('MC nominal, no-smearing region: exactly q/pt + kappa_b', close(nom, pk, noSmear) < 1e-9,
       'n=%d' % noSmear.sum())
with np.errstate(divide='ignore', invalid='ignore'):
    g = (nom / pk - 1) / (sigma(pk * np.cosh(eta)) * f)
gs = g[smeared]
report('MC nominal smearing pull ~ N(0,1)', abs(gs.mean()) < 4 / np.sqrt(len(gs)) and abs(gs.std() - 1) < 0.15,
       'n=%d mean %.3f std %.3f' % (len(gs), gs.mean(), gs.std()))
gg = np.where(smeared, g, 0.0)


def expectWithG(base):
    return base * (1 + gg * sigma(base * np.cosh(eta)) * f)


for v, k in (('MCscaleUp', kappa + dkappa), ('MCtargetData', 0.0 * kappa), ('MCtargetDataScaleUp', dkappa)):
    p, _ = got(v)
    d = close(p, expectWithG(shift(tuneP, k)), corrected)
    report('%s: shift then the same smearing' % v, d < 1e-9 and close(p, nom, corrected) > 1e-6, 'max rel diff %.1e' % d)
p, _ = got('MCscaleDown')
report('MCscaleDown', close(p, expectWithG(shift(tuneP, kappa - dkappa)), corrected) < 1e-9)

# Systematic: an extra, independent smearing with factor 0.46 on top of the nominal.
p, _ = got('MCresolution')
with np.errstate(divide='ignore', invalid='ignore'):
    g2 = ((p / nom - 1) / (sigma(nom * np.cosh(eta)) * 0.46))[corrected]
report('resolution systematic pull ~ N(0,1), independent of the nominal',
       abs(g2.mean()) < 4 / np.sqrt(len(g2)) and abs(g2.std() - 1) < 0.15 and abs(np.corrcoef(g2[f[corrected] > 0], gs)[0, 1]) < 0.2,
       'n=%d mean %.3f std %.3f corr %.2f' % (len(g2), g2.mean(), g2.std(), np.corrcoef(g2[f[corrected] > 0], gs)[0, 1]))
p, _ = got('MCnominal')
medium = ~hybridHigh & (pt > 26)
report('MC nominal: medium-regime muons (26 < pt < 200) get the MuonScaRe corrections', np.mean(p[medium] != pt[medium]) > 0.9)
sys.exit(0 if ok else 1)
