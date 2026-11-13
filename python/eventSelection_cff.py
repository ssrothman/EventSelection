"""Event selections at reconstruction and generator level, as bool flags plus optional filters.

Every value comes from the caller's configuration dict (see simulation/configs/default.yaml in boostedV);
there are no physics defaults here.

Reco level (labels unprefixed): object collections from PAT string cuts, goodVertexFlag, metFilterFlag, and per channel
  <ch>Trigger, <ch>Sel, <ch> (AND of trigger, vertex, MET filters and selection) and <ch>Filter.
Gen level (labels prefixed 'gen'): Rivet particle-level objects (genParticleLevel: dressed prompt leptons, isolated
  prompt photons, MET from prompt neutrinos), object collections from candidate string cuts, and per channel
  gen<ch>Sel, gen<ch> (= gen<ch>Sel) and gen<ch>Filter; there is no trigger, vertex or MET-filter requirement.
Selected objects are in <prefix><ch>Sel with instance 'leptons' (dilepton, leptonMET) or 'objects' (singleObject).
"""
import FWCore.ParameterSet.Config as cms
from SRothman.EventSelection.muonHighPtRun3_cff import highPtPSet

LEVELS = ('reco', 'gen')
PREFIX = dict(reco='', gen='gen')
_PAT_SELECTORS = dict(muons='PATMuonSelector', electrons='PATElectronSelector', photons='PATPhotonSelector')
OBJECT_INSTANCE = dict(dilepton='leptons', leptonMET='leptons', singleObject='objects')
BOSON_INSTANCE = dict(dilepton='boson', leptonMET='boson', singleObject='objects')


def label(level, name):
    """Module label of `name` at `level`: unchanged at reco, 'gen' + capitalised name at gen."""
    return name if level == 'reco' else PREFIX[level] + name[0].upper() + name[1:]


def _add(process, task, label, module):
    setattr(process, label, module)
    if task is not None:
        task.add(module)
    return label


def _dressing(process, task, suffix, name, o, d, inputs):
    """<name>Undressed (PAT cut) -> <name>Dressed (DressedLeptonProducer) -> <name> (dressedCut)."""
    _add(process, task, name + 'Undressed' + suffix,
         cms.EDFilter(_PAT_SELECTORS[o['source']], src=cms.InputTag(inputs[o['source']]), cut=cms.string(o['cut']),
                      filter=cms.bool(False)))
    c = d['cone']
    fsrIndex = 'muFsrIndex' if o['source'] == 'muons' else 'eleFsrIndex'
    _add(process, task, name + 'Dressed' + suffix,
         cms.EDProducer('DressedLeptonProducer', src=cms.InputTag(name + 'Undressed' + suffix),
                        mode=cms.string(d['mode']), fsrPhotons=cms.InputTag('leptonFSRphotons' + suffix),
                        fsrIndex=cms.InputTag('leptonFSRphotons' + suffix, fsrIndex),
                        originals=cms.InputTag(inputs[o['source']]), pfCandidates=cms.InputTag(inputs['pfCandidates']),
                        footprintElectrons=cms.InputTag(inputs['electrons']), coneSize=cms.double(c['coneSize']),
                        photonPtMin=cms.double(c['photonPtMin']), applyPuppiWeights=cms.bool(c['applyPuppiWeights'])))
    _add(process, task, name + suffix,
         cms.EDFilter('CandPtrSelector', src=cms.InputTag(name + 'Dressed' + suffix), cut=cms.string(o['dressedCut'])))


def addRecoCommon(process, reco, inputs, isMC, task=None, suffix=''):
    """Reco object collections (reco['objects']: {label: {source: muons|electrons|photons, cut, dressedCut}}),
    lepton dressing (reco['dressing']['mode']: none | fsr | cone), goodVertexFlag (reco['vertex']) and
    metFilterFlag (reco['metFilters']). With dressing, a lepton collection <name> is <name>Undressed (PAT `cut`),
    then <name>Dressed, then `dressedCut` on the dressed leptons; photons are never dressed.
    Every muon consumer reads correctedMuons (MuonCorrectionProducer: momentum assignment, userInt highPtRegime and,
    with reco['muonCorrections']['apply'], the MUO POG corrections) instead of inputs['muons'] if it is needed: with
    apply, a non-PF momentum, or muon scale factors in MC."""
    mc = reco['muonCorrections']
    if mc['apply'] or mc['momentum'] != 'pf' or (isMC and reco['muonScaleFactors']):
        hp = mc['highPt']
        _add(process, task, 'correctedMuons' + suffix,
             cms.EDProducer('MuonCorrectionProducer', src=cms.InputTag(inputs['muons']), isData=cms.bool(not isMC),
                            momentum=cms.string(mc['momentum']), regimeThreshold=cms.double(mc['regimeThreshold']),
                            applyCorrections=cms.bool(mc['apply']), correctionFile=cms.string(mc['correctionFile']),
                            lowPtThreshold=cms.double(mc['lowPtThreshold']),
                            highPtThreshold=cms.double(mc['highPtThreshold']),
                            resolutionVariationK=cms.string(mc['resolutionVariationK']),
                            variation=cms.string(mc['variation']),
                            highPt=highPtPSet(hp['era'], hp['ptMin'], hp['kappaTarget'])))
        inputs = dict(inputs, muons='correctedMuons' + suffix)
    d = reco['dressing']
    if d['mode'] == 'fsr':
        f = d['fsr']
        _add(process, task, 'leptonFSRphotons' + suffix,
             cms.EDProducer('LeptonFSRProducer', packedPFCandidates=cms.InputTag(inputs['pfCandidates']),
                            slimmedElectrons=cms.InputTag(inputs['electrons']), muons=cms.InputTag(inputs['muons']),
                            electrons=cms.InputTag(inputs['electrons']), muonPtMin=cms.double(f['muonPtMin']),
                            muonEtaMax=cms.double(f['muonEtaMax']), elePtMin=cms.double(f['elePtMin']),
                            eleEtaMax=cms.double(f['eleEtaMax']), photonPtMin=cms.double(f['photonPtMin']),
                            deltaROverEt2Max=cms.double(f['deltaROverEt2Max']), isolation=cms.double(f['isolation'])))
    for name, o in reco['objects'].items():
        if d['mode'] != 'none' and o['source'] in ('muons', 'electrons'):
            _dressing(process, task, suffix, name, o, d, inputs)
        else:
            _add(process, task, name + suffix,
                 cms.EDFilter(_PAT_SELECTORS[o['source']], src=cms.InputTag(inputs[o['source']]),
                              cut=cms.string(o['cut']), filter=cms.bool(False)))
    v = reco['vertex']
    _add(process, task, 'goodVertexFlag' + suffix,
         cms.EDProducer('VertexFlagProducer', src=cms.InputTag(inputs['vertices']), ndofMin=cms.double(v['ndofMin']),
                        absZMax=cms.double(v['absZMax']), rhoMax=cms.double(v['rhoMax'])))
    _add(process, task, 'metFilterFlag' + suffix,
         cms.EDProducer('TriggerFlagProducer', src=cms.InputTag('TriggerResults', '', inputs['metFilterProcess']),
                        paths=cms.vstring(*reco['metFilters']), requireAll=cms.bool(True),
                        allowMissing=cms.bool(False)))


def addGenCommon(process, gen, inputs, task=None, suffix=''):
    """Rivet particle level (gen['particleLevel']) and gen object collections
    (gen['objects']: {label: {source: leptons|photons, cut}}). The particle-level MET is genParticleLevel:mets."""
    pl = gen['particleLevel']
    # GenParticles2HepMCConverter needs the particle data table in the EventSetup.
    if not hasattr(process, 'HepPDTESSource'):
        process.load('SimGeneral.HepPDTESSource.pythiapdt_cfi')
    _add(process, task, 'genMergedParticles' + suffix,
         cms.EDProducer('MergedGenParticleProducer', inputPruned=cms.InputTag(inputs['prunedGenParticles']),
                        inputPacked=cms.InputTag(inputs['packedGenParticles'])))
    _add(process, task, 'genParticles2HepMC' + suffix,
         cms.EDProducer('GenParticles2HepMCConverter', genParticles=cms.InputTag('genMergedParticles' + suffix),
                        genEventInfo=cms.InputTag(inputs['genEventInfo']), signalParticlePdgIds=cms.vint32()))
    _add(process, task, 'genParticleLevel' + suffix,
         cms.EDProducer('ParticleLevelProducer', src=cms.InputTag('genParticles2HepMC' + suffix, 'unsmeared'),
                        doJetClustering=cms.bool(False), usePromptFinalStates=cms.bool(pl['usePromptFinalStates']),
                        excludePromptLeptonsFromJetClustering=cms.bool(False),
                        excludeNeutrinosFromJetClustering=cms.bool(True),
                        particleMinPt=cms.double(pl['particleMinPt']), particleMaxEta=cms.double(pl['particleMaxEta']),
                        lepConeSize=cms.double(pl['lepConeSize']), lepMinPt=cms.double(pl['lepMinPt']),
                        lepMaxEta=cms.double(pl['lepMaxEta']),
                        # jets are not clustered (doJetClustering False); these values are unused
                        jetConeSize=cms.double(0.4), jetMinPt=cms.double(10.0), jetMaxEta=cms.double(999.0),
                        fatJetConeSize=cms.double(0.8), fatJetMinPt=cms.double(170.0), fatJetMaxEta=cms.double(999.0),
                        phoIsoConeSize=cms.double(pl['phoIsoConeSize']), phoMaxRelIso=cms.double(pl['phoMaxRelIso']),
                        phoMinPt=cms.double(pl['phoMinPt']), phoMaxEta=cms.double(pl['phoMaxEta'])))
    for name, o in gen['objects'].items():
        _add(process, task, name + suffix,
             cms.EDFilter('CandPtrSelector', src=cms.InputTag('genParticleLevel' + suffix, o['source']),
                          cut=cms.string(o['cut'])))


def addChannel(process, level, name, ch, inputs, task=None, suffix=''):
    """One channel at `level` from its spec ch (keys depend on ch['type']); requires the level's common modules with
    the same suffix. Threshold variations: call again with a modified spec and a suffix."""
    s, lab = suffix, label(level, name)
    t = ch['type']
    vetoes = cms.VInputTag(*[v + s for v in ch['vetoes']]) if t != 'dilepton' else None
    met = metLabel(level, inputs, s)
    if t == 'dilepton':
        p = cms.EDProducer('DileptonSelector', src=cms.InputTag(ch['objects'] + s),
                           leadPtMin=cms.double(ch['leadPtMin']), subleadPtMin=cms.double(ch['subleadPtMin']),
                           massMin=cms.double(ch['massMin']), massMax=cms.double(ch['massMax']),
                           oppositeCharge=cms.bool(ch['oppositeCharge']), targetMass=cms.double(ch['targetMass']))
    elif t == 'leptonMET':
        p = cms.EDProducer('LeptonMETSelector', src=cms.InputTag(ch['objects'] + s), met=cms.InputTag(met),
                           vetoSrcs=vetoes, ptMin=cms.double(ch['ptMin']), metMin=cms.double(ch['metMin']),
                           mtMin=cms.double(ch['mtMin']))
    elif t == 'singleObject':
        p = cms.EDProducer('SingleObjectEventSelector', src=cms.InputTag(ch['objects'] + s), vetoSrcs=vetoes,
                           nMin=cms.uint32(ch['nMin']), nMax=cms.uint32(ch['nMax']))
    else:
        raise ValueError('channel %s: unknown type %s' % (name, t))
    sel = _add(process, task, lab + 'Sel' + s, p)
    srcs = [sel]
    if level == 'reco':
        trig = _add(process, task, lab + 'Trigger' + s,
                    cms.EDProducer('TriggerFlagProducer', src=cms.InputTag('TriggerResults', '', inputs['hltProcess']),
                                   paths=cms.vstring(*ch['triggers']), requireAll=cms.bool(False),
                                   allowMissing=cms.bool(ch['allowMissingTriggers'])))
        srcs = [trig, 'goodVertexFlag' + s, 'metFilterFlag' + s, sel]
    flag = _add(process, task, lab + s, cms.EDProducer('FlagCombiner', mode=cms.string('and'), srcs=cms.VInputTag(*srcs)))
    setattr(process, lab + s + 'Filter', cms.EDFilter('EventFlagFilter', srcs=cms.VInputTag(flag), mode=cms.string('and')))
    return flag


OBJECT_TABLE_MODULE = dict(muons='MuonExtensionTableProducer', electrons='ElectronExtensionTableProducer',
                          photons='PhotonExtensionTableProducer')


def _objectExtension(process, task, reco, ch, spec, inputs, suffix):
    """Extension of the reco <ch>Leptons or <ch>Objects table with reco['objectTables'] columns of the object type."""
    src = reco['objects'][spec['objects']]['source']
    objects = 'correctedMuons' + suffix if src == 'muons' and hasattr(process, 'correctedMuons' + suffix) else inputs[src]
    inst = OBJECT_INSTANCE[spec['type']]
    table = ch + ('Leptons' if inst == 'leptons' else 'Objects') + suffix
    _add(process, task, ch + 'ObjectsExt' + suffix + 'Table',
         cms.EDProducer(OBJECT_TABLE_MODULE[src], src=cms.InputTag(ch + 'Sel' + suffix, inst),
                        objects=cms.InputTag(objects), name=cms.string(table), matchDeltaR=cms.double(0.1),
                        triggerPaths=cms.vstring(*spec['triggers']),
                        triggerResults=cms.InputTag('TriggerResults', '', inputs['hltProcess']),
                        triggerObjects=cms.InputTag(inputs['triggerObjects']), triggerMatchDeltaR=cms.double(0.1),
                        variables=cms.VPSet(*[cms.PSet(name=cms.string(v['name']), expr=cms.string(v['expr']),
                                                       type=cms.string(v['type']), doc=cms.string(v['doc']),
                                                       precision=cms.int32(v['precision']))
                                              for v in reco['objectTables'][src]])))


def metLabel(level, inputs, suffix=''):
    return inputs['met'] if level == 'reco' else 'genParticleLevel%s:mets' % suffix


def _tables(process, level, cfg, flags, inputs, task, suffix):
    names, srcs = ([], []) if level == 'gen' else (['goodVertex', 'metFilters'], ['goodVertexFlag' + suffix, 'metFilterFlag' + suffix])
    for ch in flags:
        names += [ch, ch + 'Sel'] + ([ch + 'Trigger'] if level == 'reco' else [])
        srcs += [label(level, ch) + suffix, label(level, ch) + 'Sel' + suffix] + ([ch + 'Trigger' + suffix] if level == 'reco' else [])
    tableName = ('Selection' if level == 'reco' else 'GenSelection') + suffix
    _add(process, task, label(level, 'selectionFlags') + suffix + 'Table',
         cms.EDProducer('EventFlagTableProducer', name=cms.string(tableName), names=cms.vstring(*names),
                        srcs=cms.VInputTag(*srcs)))

    def var(expr, typ):
        return cms.PSet(expr=cms.string(expr), type=cms.string(typ), doc=cms.string(expr), precision=cms.int32(-1))

    for ch, spec in cfg['channels'].items():
        inst = BOSON_INSTANCE[spec['type']]
        _add(process, task, label(level, ch) + 'Candidates' + suffix + 'Table',
             cms.EDProducer('SimpleCandidateFlatTableProducer', src=cms.InputTag(label(level, ch) + 'Sel' + suffix, inst),
                            name=cms.string(('Gen' if level == 'gen' else '') + ch +
                                            ('Boson' if inst == 'boson' else 'Objects') + suffix),
                            doc=cms.string('%s %s selected candidate' % (level, ch)), singleton=cms.bool(False),
                            extension=cms.bool(False), cut=cms.string(''),
                            variables=cms.PSet(pt=var('pt', 'float'), eta=var('eta', 'float'), phi=var('phi', 'float'),
                                               mass=var('mass', 'float'), charge=var('charge', 'int'))))
        if OBJECT_INSTANCE[spec['type']] == 'leptons':
            _add(process, task, label(level, ch) + 'Leptons' + suffix + 'Table',
                 cms.EDProducer('SimpleCandidateFlatTableProducer',
                                src=cms.InputTag(label(level, ch) + 'Sel' + suffix, 'leptons'),
                                name=cms.string(('Gen' if level == 'gen' else '') + ch + 'Leptons' + suffix),
                                doc=cms.string('%s %s selected leptons' % (level, ch)), singleton=cms.bool(False),
                                extension=cms.bool(False), cut=cms.string(''),
                                variables=cms.PSet(pt=var('pt', 'float'), eta=var('eta', 'float'), phi=var('phi', 'float'),
                                                   mass=var('mass', 'float'), charge=var('charge', 'int'),
                                                   pdgId=var('pdgId', 'int'))))
        if level == 'reco':
            _objectExtension(process, task, cfg, ch, spec, inputs, suffix)
    if any(spec['type'] == 'leptonMET' for spec in cfg['channels'].values()):
        met = metLabel(level, inputs, suffix)
        _add(process, task, label(level, 'selectionMet') + suffix + 'Table',
             cms.EDProducer('SimpleCandidateFlatTableProducer', src=cms.InputTag(met),
                            name=cms.string(('GenSelectionMET' if level == 'gen' else 'SelectionMET') + suffix),
                            doc=cms.string('MET used by the %s leptonMET channels (%s)' % (level, met)),
                            singleton=cms.bool(True), extension=cms.bool(False),
                            variables=cms.PSet(pt=var('pt', 'float'), phi=var('phi', 'float'))))


def muonSFLabel(channel, suffix=''):
    """Label of the module whose double product is the channel's muon SF event weight."""
    return channel + 'MuonSF' + suffix + 'Table'


def addMuonScaleFactors(process, reco, inputs, task=None, suffix=''):
    """MC only: a MuonSFProducer per channel of reco['muonScaleFactors'], on the channel's selected leptons."""
    for ch, terms in reco['muonScaleFactors'].items():
        _add(process, task, muonSFLabel(ch, suffix),
             cms.EDProducer('MuonSFProducer', selected=cms.InputTag(label('reco', ch) + suffix),
                            src=cms.InputTag(label('reco', ch) + 'Sel' + suffix, 'leptons'),
                            muons=cms.InputTag('correctedMuons' + suffix), name=cms.string(ch + 'MuonSF' + suffix),
                            matchDeltaR=cms.double(0.1),
                            triggerResults=cms.InputTag('TriggerResults', '', inputs['hltProcess']),
                            triggerObjects=cms.InputTag(inputs['triggerObjects']), triggerMatchDeltaR=cms.double(0.1),
                            terms=cms.VPSet(*[cms.PSet(name=cms.string(t['name']), regime=cms.string(t['regime']),
                                                       file=cms.string(t['file']), key=cms.string(t['key']),
                                                       trigger=cms.bool(t['trigger']), paths=cms.vstring(t['paths']),
                                                       outOfRange=cms.string(t['outOfRange'])) for t in terms])))


def addSelection(process, sel, inputs, levels, isMC, task=None, suffix='', withTables=True):
    """Builds the reco selection and, in MC, the gen selection (both always, so that both levels' flags and objects
    exist), per-level '<prefix>anyChannel' OR flags, and 'anySelected' = OR over `levels` with 'anySelectedFilter'.
    `levels` must be a non-empty subset of the built levels. Returns {level: {channel: flag label}}."""
    built = ['reco', 'gen'] if isMC else ['reco']
    if not levels or any(l not in built for l in levels):
        raise ValueError('selection levels %s not available (built: %s)' % (list(levels), built))
    addRecoCommon(process, sel['reco'], inputs, isMC, task, suffix)
    if isMC:
        addGenCommon(process, sel['gen'], inputs, task, suffix)
    flags = {}
    for level in built:
        cfg = sel[level]
        flags[level] = {name: addChannel(process, level, name, ch, inputs, task, suffix)
                        for name, ch in cfg['channels'].items()}
        anyLabel = label(level, 'anyChannel') + suffix
        _add(process, task, anyLabel,
             cms.EDProducer('FlagCombiner', mode=cms.string('or'), srcs=cms.VInputTag(*flags[level].values())))
        if withTables:
            _tables(process, level, cfg, flags[level], inputs, task, suffix)
    if isMC:
        addMuonScaleFactors(process, sel['reco'], inputs, task, suffix)
    anyFlags = [label(l, 'anyChannel') + suffix for l in levels]
    _add(process, task, 'anySelected' + suffix,
         cms.EDProducer('FlagCombiner', mode=cms.string('or'), srcs=cms.VInputTag(*anyFlags)))
    setattr(process, 'anySelected' + suffix + 'Filter',
            cms.EDFilter('EventFlagFilter', srcs=cms.VInputTag(*anyFlags), mode=cms.string('or')))
    return flags


def selectedObjects(levelCfg, level, channels, suffix=''):
    """InputTag strings of the selected objects of the given channels at `level` (e.g. for jet cleaning)."""
    return ['%sSel%s:%s' % (label(level, c), suffix, OBJECT_INSTANCE[levelCfg['channels'][c]['type']]) for c in channels]
