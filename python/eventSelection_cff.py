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


def addRecoCommon(process, reco, inputs, task=None, suffix=''):
    """Reco object collections (reco['objects']: {label: {source: muons|electrons|photons, cut}}),
    goodVertexFlag (reco['vertex']) and metFilterFlag (reco['metFilters'])."""
    for name, o in reco['objects'].items():
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
    met = inputs['met'] if level == 'reco' else 'genParticleLevel%s:mets' % s
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


def _tables(process, level, cfg, flags, task, suffix):
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


def addSelection(process, sel, inputs, levels, isMC, task=None, suffix='', withTables=True):
    """Builds the reco selection and, in MC, the gen selection (both always, so that both levels' flags and objects
    exist), per-level '<prefix>anyChannel' OR flags, and 'anySelected' = OR over `levels` with 'anySelectedFilter'.
    `levels` must be a non-empty subset of the built levels. Returns {level: {channel: flag label}}."""
    built = ['reco', 'gen'] if isMC else ['reco']
    if not levels or any(l not in built for l in levels):
        raise ValueError('selection levels %s not available (built: %s)' % (list(levels), built))
    addRecoCommon(process, sel['reco'], inputs, task, suffix)
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
            _tables(process, level, cfg, flags[level], task, suffix)
    anyFlags = [label(l, 'anyChannel') + suffix for l in levels]
    _add(process, task, 'anySelected' + suffix,
         cms.EDProducer('FlagCombiner', mode=cms.string('or'), srcs=cms.VInputTag(*anyFlags)))
    setattr(process, 'anySelected' + suffix + 'Filter',
            cms.EDFilter('EventFlagFilter', srcs=cms.VInputTag(*anyFlags), mode=cms.string('or')))
    return flags


def selectedObjects(levelCfg, level, channels, suffix=''):
    """InputTag strings of the selected objects of the given channels at `level` (e.g. for jet cleaning)."""
    return ['%sSel%s:%s' % (label(level, c), suffix, OBJECT_INSTANCE[levelCfg['channels'][c]['type']]) for c in channels]
