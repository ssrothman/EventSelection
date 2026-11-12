"""In-memory variants of an analysis configuration for the muon validations."""
MUO = '/cvmfs/cms-griddata.cern.ch/cat/metadata/MUO/Run3-22EFGSep23-Summer22EE-NanoAODv12/latest/'
HIGH_TRIGGERS = ['HLT_IsoMu24_v*', 'HLT_Mu50_v*', 'HLT_CascadeMu100_v*', 'HLT_HighPtTkMu100_v*']
HYBRID_CUT = ("pt > 15 && abs(eta) < 2.4 && ((userInt('highPtRegime') == 0 && passed('CutBasedIdTight') && "
              "passed('PFIsoTight')) || (userInt('highPtRegime') == 1 && passed('CutBasedIdGlobalHighPt') && "
              "passed('TkIsoLoose')))")


def term(name, regime, f, key, trigger):
    return dict(name=name, regime=regime, file=MUO + f, key=key, trigger=trigger, paths=HIGH_TRIGGERS if trigger else [],
                outOfRange='skip' if trigger else 'error')


def hybrid(cfg):
    """Medium recipe below 200 GeV (PF pt), high-pT recipe above (TuneP pt), with corrections and both SF sets."""
    reco = cfg['selection']['reco']
    reco['muonCorrections'].update(apply=True, momentum='hybrid', regimeThreshold=200.0)
    reco['objects']['tightMuons']['cut'] = HYBRID_CUT
    terms = [term('id', 'medium', 'muon_Z.json.gz', 'NUM_TightID_DEN_TrackerMuons', False),
             term('iso', 'medium', 'muon_Z.json.gz', 'NUM_TightPFIso_DEN_TightID', False),
             term('trig', 'medium', 'muon_Z.json.gz',
                  'NUM_IsoMu24_or_Mu50_or_CascadeMu100_or_HighPtTkMu100_DEN_CutBasedIdTight_and_PFIsoTight', True),
             term('highReco', 'high', 'muon_HighPt.json.gz', 'NUM_GlobalMuons_DEN_TrackerMuonProbes', False),
             term('highId', 'high', 'muon_HighPt.json.gz', 'NUM_HighPtID_DEN_GlobalMuonProbes', False),
             term('highIso', 'high', 'muon_HighPt.json.gz', 'NUM_probe_LooseRelTkIso_DEN_HighPtProbes', False),
             term('highTrig', 'high', 'muon_Z.json.gz',
                  'NUM_IsoMu24_or_Mu50_or_CascadeMu100_or_HighPtTkMu100_DEN_CutBasedIdGlobalHighPt_and_TkIsoLoose', True)]
    reco['muonScaleFactors'] = {'Zmumu': terms, 'Wmunu': terms}
    for ch in ('Zmumu', 'Wmunu'):
        reco['channels'][ch]['triggers'] = HIGH_TRIGGERS
    return cfg


RECIPES = {'default': lambda c: c, 'hybrid': hybrid}
