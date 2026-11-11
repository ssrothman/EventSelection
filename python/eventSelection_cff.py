"""Event selections as bool flags (one cmsRun handles all channels) plus optional filters.

Every value comes from the caller's configuration dict (see simulation/configs/default.yaml in boostedV);
there are no physics defaults here.

Each channel <ch> provides:
  <ch>Trigger  TriggerFlagProducer on the channel's HLT paths
  <ch>Sel      selector (bool + selected objects, instances 'boson'/'leptons' or 'objects')
  <ch>         FlagCombiner: AND of <ch>Trigger, goodVertexFlag, metFilterFlag and <ch>Sel
  <ch>Filter   EventFlagFilter on <ch> (not in the task; put it on a Path to filter)
"""
import FWCore.ParameterSet.Config as cms

_SELECTORS = dict(muons='PATMuonSelector', electrons='PATElectronSelector', photons='PATPhotonSelector')
# Instance labels of each channel type's products.
OBJECT_INSTANCE = dict(dilepton='leptons', leptonMET='leptons', singleObject='objects')
BOSON_INSTANCE = dict(dilepton='boson', leptonMET='boson', singleObject='objects')


def _add(process, task, label, module):
    setattr(process, label, module)
    if task is not None:
        task.add(module)
    return label


def addCommon(process, sel, inputs, task=None, suffix=''):
    """Object collections (sel['objects']: {label: {source: muons|electrons|photons, cut: string}}),
    goodVertexFlag (sel['vertex']) and metFilterFlag (sel['metFilters'])."""
    for name, o in sel['objects'].items():
        _add(process, task, name + suffix,
             cms.EDFilter(_SELECTORS[o['source']], src=cms.InputTag(inputs[o['source']]), cut=cms.string(o['cut']),
                          filter=cms.bool(False)))
    v = sel['vertex']
    _add(process, task, 'goodVertexFlag' + suffix,
         cms.EDProducer('VertexFlagProducer', src=cms.InputTag(inputs['vertices']), ndofMin=cms.double(v['ndofMin']),
                        absZMax=cms.double(v['absZMax']), rhoMax=cms.double(v['rhoMax'])))
    _add(process, task, 'metFilterFlag' + suffix,
         cms.EDProducer('TriggerFlagProducer', src=cms.InputTag('TriggerResults', '', inputs['metFilterProcess']),
                        paths=cms.vstring(*sel['metFilters']), requireAll=cms.bool(True),
                        allowMissing=cms.bool(False)))


def addChannel(process, name, ch, inputs, task=None, suffix=''):
    """One channel from its spec ch (keys depend on ch['type']); requires addCommon with the same suffix.
    Threshold variations: call again with a modified spec and a suffix."""
    s = suffix
    trig = _add(process, task, name + 'Trigger' + s,
                cms.EDProducer('TriggerFlagProducer', src=cms.InputTag('TriggerResults', '', inputs['hltProcess']),
                               paths=cms.vstring(*ch['triggers']), requireAll=cms.bool(False),
                               allowMissing=cms.bool(ch['allowMissingTriggers'])))
    t = ch['type']
    vetoes = cms.VInputTag(*[v + s for v in ch['vetoes']]) if t != 'dilepton' else None
    if t == 'dilepton':
        p = cms.EDProducer('DileptonSelector', src=cms.InputTag(ch['objects'] + s),
                           leadPtMin=cms.double(ch['leadPtMin']), subleadPtMin=cms.double(ch['subleadPtMin']),
                           massMin=cms.double(ch['massMin']), massMax=cms.double(ch['massMax']),
                           oppositeCharge=cms.bool(ch['oppositeCharge']), targetMass=cms.double(ch['targetMass']))
    elif t == 'leptonMET':
        p = cms.EDProducer('LeptonMETSelector', src=cms.InputTag(ch['objects'] + s), met=cms.InputTag(inputs['met']),
                           vetoSrcs=vetoes, ptMin=cms.double(ch['ptMin']), metMin=cms.double(ch['metMin']),
                           mtMin=cms.double(ch['mtMin']))
    elif t == 'singleObject':
        p = cms.EDProducer('SingleObjectEventSelector', src=cms.InputTag(ch['objects'] + s), vetoSrcs=vetoes,
                           nMin=cms.uint32(ch['nMin']), nMax=cms.uint32(ch['nMax']))
    else:
        raise ValueError('channel %s: unknown type %s' % (name, t))
    sel = _add(process, task, name + 'Sel' + s, p)
    flag = _add(process, task, name + s,
                cms.EDProducer('FlagCombiner', mode=cms.string('and'),
                               srcs=cms.VInputTag(trig, 'goodVertexFlag' + s, 'metFilterFlag' + s, sel)))
    setattr(process, name + s + 'Filter', cms.EDFilter('EventFlagFilter', srcs=cms.VInputTag(flag), mode=cms.string('and')))
    return flag


def addSelection(process, sel, inputs, task=None, suffix='', tableName='Selection', withTables=True):
    """All channels in sel['channels'], the 'anyChannel' OR flag/filter and, optionally, NanoAOD tables:
    <tableName> (one bool per channel and its components) and <ch>Boson / <ch>Objects candidate tables.
    Returns {channel: flag label}."""
    addCommon(process, sel, inputs, task, suffix)
    flags = {name: addChannel(process, name, ch, inputs, task, suffix) for name, ch in sel['channels'].items()}
    _add(process, task, 'anyChannel' + suffix,
         cms.EDProducer('FlagCombiner', mode=cms.string('or'), srcs=cms.VInputTag(*flags.values())))
    setattr(process, 'anyChannel' + suffix + 'Filter',
            cms.EDFilter('EventFlagFilter', srcs=cms.VInputTag(*flags.values()), mode=cms.string('or')))
    if not withTables:
        return flags
    names, srcs = ['goodVertex', 'metFilters'], ['goodVertexFlag' + suffix, 'metFilterFlag' + suffix]
    for ch in flags:
        names += [ch, ch + 'Trigger', ch + 'Sel']
        srcs += [ch + suffix, ch + 'Trigger' + suffix, ch + 'Sel' + suffix]
    _add(process, task, 'selectionFlags' + suffix + 'Table',
         cms.EDProducer('EventFlagTableProducer', name=cms.string(tableName + suffix), names=cms.vstring(*names),
                        srcs=cms.VInputTag(*srcs)))

    def var(expr, typ):
        return cms.PSet(expr=cms.string(expr), type=cms.string(typ), doc=cms.string(expr), precision=cms.int32(-1))

    for ch, spec in sel['channels'].items():
        inst = BOSON_INSTANCE[spec['type']]
        _add(process, task, ch + 'Candidates' + suffix + 'Table',
             cms.EDProducer('SimpleCandidateFlatTableProducer', src=cms.InputTag(ch + 'Sel' + suffix, inst),
                            name=cms.string(ch + ('Boson' if inst == 'boson' else 'Objects') + suffix),
                            doc=cms.string(ch + ' selected candidate'), singleton=cms.bool(False),
                            extension=cms.bool(False), cut=cms.string(''),
                            variables=cms.PSet(pt=var('pt', 'float'), eta=var('eta', 'float'), phi=var('phi', 'float'),
                                               mass=var('mass', 'float'), charge=var('charge', 'int'))))
    return flags


def selectedObjects(sel, channels, suffix=''):
    """InputTag strings of the selected objects of the given channels (e.g. for jet cleaning)."""
    return ['%sSel%s:%s' % (c, suffix, OBJECT_INSTANCE[sel['channels'][c]['type']]) for c in channels]
