"""MUO POG Run-3 high-pT muon momentum tables, from the MUO POG wiki (docs/guidelines/corrections.md, "High pT:
Momentum Scale" and "High pT: Momentum Resolution"). Checked against the wiki source by
test/validation/checkHighPtTables.py.

kappa, kappaUnc: GE kappa_b [c/TeV], rows phi in [-180,-60), [-60,60), [60,180] degrees, columns eta in
[-2.4,-2.1), [-2.1,-1.2), [-1.2,0), [0,1.2), [1.2,2.1), [2.1,2.4).
sigma*: sigma(p) = c0 + c1 p + c2 p^2 + c3 p^3, p in GeV. smear*: nominal MC smearing factor (0: none).
"""
import FWCore.ParameterSet.Config as cms

ERAS = {
    '2022preEE': dict(
        kappa=[-0.16, -0.03, -0.05, 0.00, 0.07, -0.06,
               0.11, -0.01, 0.06, 0.02, 0.05, -0.06,
               0.17, 0.16, -0.04, -0.01, 0.04, -0.00],
        kappaUnc=[0.10, 0.05, 0.04, 0.04, 0.06, 0.11,
                  0.10, 0.06, 0.04, 0.03, 0.05, 0.10,
                  0.11, 0.04, 0.04, 0.03, 0.06, 0.09],
        sigmaBarrel=[0.01152, 5.95e-5, -2.92e-8, 5.14e-12],
        sigmaEndcap=[0.01405, 5.28e-5, -1.9e-8, 3.01e-12],
        smearBarrel=0.0, smearEndcap=0.32),
    '2022postEE': dict(
        kappa=[-0.12, -0.03, 0.013, 0.029, -0.04, -0.28,
               0.24, 0.10, -0.006, -0.047, -0.14, -0.48,
               0.28, 0.07, -0.028, 0.018, 0.07, 0.05],
        kappaUnc=[0.05, 0.03, 0.022, 0.023, 0.03, 0.05,
                  0.05, 0.03, 0.022, 0.022, 0.03, 0.05,
                  0.05, 0.03, 0.022, 0.022, 0.03, 0.05],
        sigmaBarrel=[0.0126, 5.89e-5, -2.85e-8, 4.92e-12],
        sigmaEndcap=[0.0150, 4.81e-5, -1.42e-8, 1.95e-12],
        smearBarrel=0.32, smearEndcap=0.46),
    '2023preBPix': dict(
        kappa=[-0.21, -0.01, 0.010, -0.045, 0.01, 0.00,
               0.08, -0.04, 0.070, 0.030, -0.13, -0.36,
               0.27, 0.05, 0.054, 0.020, 0.00, -0.04],
        kappaUnc=[0.06, 0.04, 0.027, 0.030, 0.04, 0.07,
                  0.07, 0.04, 0.025, 0.027, 0.04, 0.06,
                  0.07, 0.04, 0.026, 0.027, 0.04, 0.06],
        sigmaBarrel=[0.0172, 6.15e-5, -3.14e-8, 5.82e-12],
        sigmaEndcap=[0.01424, 5.31e-5, -1.92e-8, 3.21e-12],
        smearBarrel=0.32, smearEndcap=0.46),
    '2023postBPix': dict(
        kappa=[-0.25, 0.07, 0.02, -0.02, 0.08, 0.12,
               -0.05, 0.00, 0.05, -0.05, -0.17, -0.33,
               0.24, 0.09, -0.01, 0.03, 0.04, -0.20],
        kappaUnc=[0.08, 0.05, 0.04, 0.04, 0.06, 0.09,
                  0.08, 0.05, 0.03, 0.04, 0.05, 0.09,
                  0.09, 0.05, 0.03, 0.04, 0.06, 0.07],
        sigmaBarrel=[0.0118, 6.14e-5, -3.12e-8, 5.74e-12],
        sigmaEndcap=[0.0141, 5.38e-5, -2.0e-8, 3.42e-12],
        smearBarrel=0.0, smearEndcap=0.568),
}


def highPtPSet(era, ptMin, kappaTarget):
    """The MuonCorrectionProducer `highPt` PSet for `era` (a key of ERAS)."""
    if era not in ERAS:
        raise ValueError('high-pT muon tables: unknown era %s (known: %s)' % (era, sorted(ERAS)))
    t = ERAS[era]
    return cms.PSet(kappa=cms.vdouble(*t['kappa']), kappaUnc=cms.vdouble(*t['kappaUnc']),
                    sigmaBarrel=cms.vdouble(*t['sigmaBarrel']), sigmaEndcap=cms.vdouble(*t['sigmaEndcap']),
                    smearBarrel=cms.double(t['smearBarrel']), smearEndcap=cms.double(t['smearEndcap']),
                    ptMin=cms.double(ptMin), kappaTarget=cms.string(kappaTarget))
