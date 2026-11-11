"""Event selections as bool flags (one cmsRun handles all channels) plus optional filters.

Each channel <ch> provides:
  <ch>Sel     channel-specific selector (bool + selected objects, instances 'boson'/'leptons' or 'objects')
  <ch>        FlagCombiner: AND of trigger, good vertex, MET filters and <ch>Sel
  <ch>Filter  EventFlagFilter on <ch> (not in the task; put it on a Path to filter)
Defaults below are proposals for Run-3 MiniAOD (Summer22EE); every cut is a parameter.
"""
import FWCore.ParameterSet.Config as cms

# Run-3 MET filters (the Flag_* paths in the PAT TriggerResults).
MET_FILTERS = ['Flag_goodVertices', 'Flag_globalSuperTightHalo2016Filter', 'Flag_EcalDeadCellTriggerPrimitiveFilter',
               'Flag_BadPFMuonFilter', 'Flag_BadPFMuonDzFilter', 'Flag_hfNoisyHitsFilter', 'Flag_eeBadScFilter',
               'Flag_ecalBadCalibFilter']

_EB_EE_GAP = '(abs(superCluster().eta()) < 1.4442 || abs(superCluster().eta()) > 1.566)'
OBJECT_CUTS = dict(
    tightMuons="pt > 15 && abs(eta) < 2.4 && passed('CutBasedIdTight') && passed('PFIsoTight')",
    vetoMuons="pt > 10 && abs(eta) < 2.4 && passed('CutBasedIdLoose') && passed('PFIsoLoose')",
    tightElectrons="pt > 15 && abs(superCluster().eta()) < 2.5 && " + _EB_EE_GAP +
                   " && electronID('cutBasedElectronID-RunIIIWinter22-V1-tight') > 0.5",
    vetoElectrons="pt > 10 && abs(superCluster().eta()) < 2.5 && " + _EB_EE_GAP +
                  " && electronID('cutBasedElectronID-RunIIIWinter22-V1-veto') > 0.5",
    tightPhotons="pt > 220 && abs(superCluster().eta()) < 1.4442 && "
                 "photonID('cutBasedPhotonID-RunIIIWinter22-122X-V1-tight') > 0.5",
)
TRIGGERS = dict(Zmumu=['HLT_IsoMu24_v*'], Zee=['HLT_Ele32_WPTight_Gsf_v*'], Wmunu=['HLT_IsoMu24_v*'],
                Wenu=['HLT_Ele32_WPTight_Gsf_v*'], Hadronic=['HLT_AK8PFJet500_v*', 'HLT_PFHT1050_v*'],
                GammaJets=['HLT_Photon200_v*'])
CHANNELS = ('Zmumu', 'Zee', 'Wmunu', 'Wenu', 'Hadronic', 'GammaJets')


def _add(process, task, label, module):
    setattr(process, label, module)
    if task is not None:
        task.add(module)
    return label


def addCommon(process, task=None, suffix='', objectCuts=None, metFilterProcess='PAT', metFilters=None):
    """Object collections, good-vertex flag and MET-filter flag. objectCuts overrides OBJECT_CUTS entries."""
    unknown = set(objectCuts or {}) - set(OBJECT_CUTS)
    if unknown:
        raise ValueError('unknown object collections: %s' % sorted(unknown))
    cuts = dict(OBJECT_CUTS, **(objectCuts or {}))
    sel = dict(Muons=('PATMuonSelector', 'slimmedMuons'), Electrons=('PATElectronSelector', 'slimmedElectrons'),
               Photons=('PATPhotonSelector', 'slimmedPhotons'))
    for name, cut in cuts.items():
        kind = [k for k in sel if name.endswith(k)][0]
        _add(process, task, name + suffix,
             cms.EDFilter(sel[kind][0], src=cms.InputTag(sel[kind][1]), cut=cms.string(cut), filter=cms.bool(False)))
    _add(process, task, 'goodVertexFlag' + suffix, cms.EDProducer('VertexFlagProducer'))
    _add(process, task, 'metFilterFlag' + suffix,
         cms.EDProducer('TriggerFlagProducer', src=cms.InputTag('TriggerResults', '', metFilterProcess),
                        paths=cms.vstring(*(metFilters or MET_FILTERS)), requireAll=cms.bool(True),
                        allowMissing=cms.bool(False)))


def addTrigger(process, label, paths, task=None, allowMissing=False):
    return _add(process, task, label,
                cms.EDProducer('TriggerFlagProducer', src=cms.InputTag('TriggerResults', '', 'HLT'),
                               paths=cms.vstring(*paths), requireAll=cms.bool(False),
                               allowMissing=cms.bool(allowMissing)))


def addChannel(process, channel, task=None, suffix='', triggers=None, **cuts):
    """Adds one channel; cuts override the selector parameters (threshold variations: call again with a suffix).
    Requires addCommon(process, suffix=suffix)."""
    s = suffix
    trig = addTrigger(process, channel + 'Trigger' + s, triggers or TRIGGERS[channel], task)
    vetoLeptons = cms.VInputTag('vetoMuons' + s, 'vetoElectrons' + s)
    if channel in ('Zmumu', 'Zee'):
        lep = 'tightMuons' if channel == 'Zmumu' else 'tightElectrons'
        defaults = dict(leadPtMin=26.0, subleadPtMin=15.0) if channel == 'Zmumu' else dict(leadPtMin=35.0, subleadPtMin=15.0)
        p = cms.EDProducer('DileptonSelector', src=cms.InputTag(lep + s), massMin=cms.double(71.0),
                           massMax=cms.double(111.0))
        params = dict(defaults, **cuts)
    elif channel in ('Wmunu', 'Wenu'):
        lep = 'tightMuons' if channel == 'Wmunu' else 'tightElectrons'
        p = cms.EDProducer('LeptonMETSelector', src=cms.InputTag(lep + s), met=cms.InputTag('slimmedMETsPuppi'),
                           vetoSrcs=vetoLeptons)
        params = dict(dict(ptMin=30.0 if channel == 'Wmunu' else 35.0, metMin=40.0, mtMin=50.0), **cuts)
    elif channel == 'GammaJets':
        p = cms.EDProducer('SingleObjectEventSelector', src=cms.InputTag('tightPhotons' + s), vetoSrcs=vetoLeptons)
        params = dict(cuts)
    elif channel == 'Hadronic':
        # Passes with no veto leptons and no tight photons.
        p = cms.EDProducer('SingleObjectEventSelector', src=cms.InputTag('tightPhotons' + s),
                           vetoSrcs=vetoLeptons, nMin=cms.uint32(0), nMax=cms.uint32(0))
        params = dict(cuts)
    else:
        raise ValueError('unknown channel ' + channel)
    for k, v in params.items():
        setattr(p, k, cms.uint32(v) if k in ('nMin', 'nMax') else cms.double(v))
    sel = _add(process, task, channel + 'Sel' + s, p)
    flag = _add(process, task, channel + s,
                cms.EDProducer('FlagCombiner', mode=cms.string('and'),
                               srcs=cms.VInputTag(trig, 'goodVertexFlag' + s, 'metFilterFlag' + s, sel)))
    setattr(process, channel + s + 'Filter', cms.EDFilter('EventFlagFilter', srcs=cms.VInputTag(flag), mode=cms.string('and')))
    return flag


def addAllChannels(process, task=None, suffix='', tableName='Selection', withTables=True,
                   objectCuts=None, metFilters=None, metFilterProcess='PAT', triggers=None, channelCuts=None):
    """All channels, the 'anyChannel' OR flag/filter and, optionally, NanoAOD tables:
    <tableName> (one bool per channel and its components) and <ch>Boson / <ch>Objects candidate tables.
    Overrides: objectCuts {collection: cut string} (see OBJECT_CUTS), metFilters [Flag_* names],
    triggers {channel: [path globs]} (see TRIGGERS), channelCuts {channel: {selector parameter: value}}."""
    triggers = dict(triggers or {})
    channelCuts = dict(channelCuts or {})
    unknown = (set(triggers) | set(channelCuts)) - set(CHANNELS)
    if unknown:
        raise ValueError('unknown channels: %s' % sorted(unknown))
    addCommon(process, task, suffix, objectCuts=objectCuts, metFilterProcess=metFilterProcess, metFilters=metFilters)
    flags = [addChannel(process, ch, task, suffix, triggers=triggers.get(ch), **channelCuts.get(ch, {}))
             for ch in CHANNELS]
    _add(process, task, 'anyChannel' + suffix, cms.EDProducer('FlagCombiner', mode=cms.string('or'), srcs=cms.VInputTag(*flags)))
    setattr(process, 'anyChannel' + suffix + 'Filter',
            cms.EDFilter('EventFlagFilter', srcs=cms.VInputTag(*flags), mode=cms.string('or')))
    if not withTables:
        return flags
    names, srcs = ['goodVertex', 'metFilters'], ['goodVertexFlag' + suffix, 'metFilterFlag' + suffix]
    for ch in CHANNELS:
        names += [ch, ch + 'Trigger', ch + 'Sel']
        srcs += [ch + suffix, ch + 'Trigger' + suffix, ch + 'Sel' + suffix]
    _add(process, task, 'selectionFlags' + suffix + 'Table',
         cms.EDProducer('EventFlagTableProducer', name=cms.string(tableName + suffix), names=cms.vstring(*names),
                        srcs=cms.VInputTag(*srcs)))
    for ch in CHANNELS:
        inst = 'boson' if ch[0] in 'ZW' else 'objects'
        _add(process, task, ch + 'Candidates' + suffix + 'Table',
             cms.EDProducer('SimpleCandidateFlatTableProducer', src=cms.InputTag(ch + 'Sel' + suffix, inst),
                            name=cms.string(ch + ('Boson' if inst == 'boson' else 'Objects') + suffix),
                            doc=cms.string(ch + ' selected candidate'), singleton=cms.bool(False),
                            extension=cms.bool(False), cut=cms.string(''),
                            variables=cms.PSet(pt=cms.PSet(expr=cms.string('pt'), type=cms.string('float'), doc=cms.string('pt'), precision=cms.int32(-1)),
                                               eta=cms.PSet(expr=cms.string('eta'), type=cms.string('float'), doc=cms.string('eta'), precision=cms.int32(-1)),
                                               phi=cms.PSet(expr=cms.string('phi'), type=cms.string('float'), doc=cms.string('phi'), precision=cms.int32(-1)),
                                               mass=cms.PSet(expr=cms.string('mass'), type=cms.string('float'), doc=cms.string('mass'), precision=cms.int32(-1)),
                                               charge=cms.PSet(expr=cms.string('charge'), type=cms.string('int'), doc=cms.string('charge'), precision=cms.int32(-1)))))
    return flags
