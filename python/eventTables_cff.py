"""Event-level NANO tables (MET, vertices, rho, gen vertex, gen particles, pileup weights) from the `output` and
`inputs` sections of the boostedV analysis YAML. The variable sets are those of the standard NanoAOD modules (PhysicsTools/NanoAOD)."""
import FWCore.ParameterSet.Config as cms
from PhysicsTools.NanoAOD.common_cff import Var


def _metVars():
    """Standard NanoAOD pfmetTable / puppiMetTable variables plus the JES and JER shifts stored in MiniAOD."""
    from PhysicsTools.NanoAOD.met_cff import puppiMetTable
    v = puppiMetTable.variables.clone()
    for shift, lab in (('JetEn', 'JES'), ('JetRes', 'JER')):
        for d in ('Up', 'Down'):
            setattr(v, 'pt%s%s' % (lab, d), Var("shiftedPt('%s%s')" % (shift, d), float, doc='%s %s pt' % (lab, d), precision=10))
            setattr(v, 'phi%s%s' % (lab, d), Var("shiftedPhi('%s%s')" % (shift, d), float, doc='%s %s phi' % (lab, d), precision=10))
    return v


def addEventTables(process, out, inputs, vertexCuts, isMC, task):
    """vertexCuts: selection.reco.vertex (ndofMin, absZMax, rhoMax)."""
    from PhysicsTools.NanoAOD.met_cff import rawPuppiMetTable, metMCTable
    for name, src in out['metTables'].items():
        setattr(process, 'met%sTable' % name, rawPuppiMetTable.clone(
            src=cms.InputTag(src), name=cms.string(name), doc=cms.string('%s (type-1 corrected)' % src),
            variables=_metVars()))
        setattr(process, 'met%sRawTable' % name, rawPuppiMetTable.clone(
            src=cms.InputTag(src), name=cms.string('Raw' + name), doc=cms.string('%s, uncorrected' % src)))
        task.add(getattr(process, 'met%sTable' % name), getattr(process, 'met%sRawTable' % name))
    if isMC and out['metTables']:
        process.genMetTable = metMCTable.clone(src=cms.InputTag(next(iter(out['metTables'].values()))))
        task.add(process.genMetTable)

    if out['vertexTables']:
        from PhysicsTools.NanoAOD.vertices_cff import vertexTable, svCandidateTable
        v = vertexCuts
        process.vertexTable = vertexTable.clone(
            pvSrc=cms.InputTag(inputs['vertices']), pfcSrc=cms.InputTag(inputs['pfCandidates']),
            svSrc=cms.InputTag(inputs['secondaryVertices']),
            # The goodVertex flag's cuts, so that npvsGood counts vertices as Selection_goodVertex judges the first.
            goodPvCut=cms.string('!isFake && ndof > %r && abs(z) < %r && position.Rho < %r'
                                 % (v['ndofMin'], v['absZMax'], v['rhoMax'])))
        process.svCandidateTable = svCandidateTable.clone(src=cms.InputTag('vertexTable'))
        task.add(process.vertexTable, process.svCandidateTable)
        if isMC:
            from PhysicsTools.NanoAOD.genVertex_cff import genVertexTable, genVertexT0Table
            process.genVertexTable = genVertexTable.clone(src=cms.InputTag(inputs['genVertex'], 'xyz0'))
            process.genVertexT0Table = genVertexT0Table.clone()
            process.genVertexT0Table.variables.t0.src = cms.InputTag(inputs['genVertex'], 't0')
            task.add(process.genVertexTable, process.genVertexT0Table)

    if out['rhoTable']:
        from PhysicsTools.NanoAOD.globals_cff import rhoTable
        process.rhoTable = rhoTable.clone()
        for r in process.rhoTable.variables.parameterNames_():
            if r not in out['rhoTable']:
                delattr(process.rhoTable.variables, r)
        missing = set(out['rhoTable']) - set(process.rhoTable.variables.parameterNames_())
        if missing:
            raise ValueError('output/rhoTable: not a standard rho: %s' % sorted(missing))
        task.add(process.rhoTable)

    if isMC and out['genParticles']:
        from PhysicsTools.NanoAOD.genparticles_cff import finalGenParticles, genParticleTable
        process.finalGenParticles = finalGenParticles.clone(src=cms.InputTag(inputs['prunedGenParticles']))
        # The standard genIso column needs the GenPartIsoProducer, which is not run here.
        process.genParticleTable = genParticleTable.clone(externalVariables=cms.PSet())
        task.add(process.finalGenParticles, process.genParticleTable)

    pu = out['pileupWeights']
    if isMC and pu['file']:
        process.pileupWeightTable = cms.EDProducer(
            'PileupWeightTableProducer', src=cms.InputTag(inputs['pileupInfo']), file=cms.string(pu['file']),
            key=cms.string(pu['key']), name=cms.string('Pileup'), extension=cms.bool(True))
        task.add(process.pileupWeightTable)


def addAK4Jets(process, ak4, isMC, task):
    """AK4 jets (output.ak4Jets of the analysis YAML): JECs re-applied from the raw momentum with correctionlib
    (PATJetJECProducer), selected by `cut` on the corrected jets, written as table `name`."""
    if not ak4['enabled']:
        return
    from PhysicsTools.NanoAOD.simplePATJetFlatTableProducer_cfi import simplePATJetFlatTableProducer
    j = ak4['jec']
    process.ak4JetsCorrected = cms.EDProducer(
        'PATJetJECProducer', src=cms.InputTag(ak4['src']), rho=cms.InputTag(ak4['rho']),
        correctionFile=cms.string(j['correctionFile']), key=cms.string(j['mcKey'] if isMC else j['dataKey']),
        uncKey=cms.string(j['uncKey']), jetIdVersion=cms.string(ak4['jetIdVersion']),
        sortByPt=cms.bool(True))
    v = cms.PSet(
        pt=Var('pt', float, doc='pt with the re-applied JEC', precision=-1),
        eta=Var('eta', float, precision=-1), phi=Var('phi', float, precision=-1),
        mass=Var('mass', float, doc='mass with the re-applied JEC', precision=-1),
        rawPt=Var("userFloat('rawPt')", float, doc='uncorrected pt', precision=-1),
        jecFactor=Var("userFloat('jecFactor')", float, doc='re-applied JEC factor (pt / rawPt)', precision=-1),
        jetId=Var("userInt('jetId')", 'uint8', doc='bit 0 tight, bit 1 tightLepVeto'),
        srcIdx=Var("userInt('srcIdx')", 'int16', doc='index in the MiniAOD jet collection'))
    if j['uncKey']:
        v.jesUncTotal = Var("userFloat('jesUncTotal')", float, doc='relative JES uncertainty (Total)', precision=-1)
    for x in ak4['variables']:
        if isMC or not x['mcOnly']:
            setattr(v, x['name'], Var(x['expr'], x['type'], doc=x['doc'], precision=x['precision']))
    process.ak4JetTable = simplePATJetFlatTableProducer.clone(
        src=cms.InputTag('ak4JetsCorrected'), name=cms.string(ak4['name']), cut=cms.string(ak4['cut']),
        doc=cms.string('%s with re-applied JECs' % ak4['src']), variables=v)
    task.add(process.ak4JetsCorrected, process.ak4JetTable)

