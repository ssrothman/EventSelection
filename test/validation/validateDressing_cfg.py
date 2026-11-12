"""Runs the reco object selection with lepton dressing and writes undressed/dressed lepton tables.
Usage: cmsRun validateDressing_cfg.py config=<analysis yaml> mode=fsr|cone inputFiles=file:... outputFile=d.root maxEvents=N
Writes <name>Undressed / <name>Dressed for the tight leptons (same order), All{Muons,Electrons} and, for fsr, FsrPhoton."""
import copy

import FWCore.ParameterSet.Config as cms
import yaml
from FWCore.ParameterSet.VarParsing import VarParsing
from SRothman.EventSelection.eventSelection_cff import addRecoCommon

opts = VarParsing('analysis')
opts.register('config', '', VarParsing.multiplicity.singleton, VarParsing.varType.string, 'analysis configuration YAML')
opts.register('mode', 'cone', VarParsing.multiplicity.singleton, VarParsing.varType.string, 'fsr | cone')
opts.parseArguments()
with open(opts.config) as f:
    cfg = yaml.safe_load(f)
reco = copy.deepcopy(cfg['selection']['reco'])
reco['dressing']['mode'] = opts.mode

process = cms.Process('DRESSVAL')
process.load('FWCore.MessageService.MessageLogger_cfi')
process.MessageLogger.cerr.FwkReport.reportEvery = 500
process.source = cms.Source('PoolSource', fileNames=cms.untracked.vstring(opts.inputFiles))
process.maxEvents.input = opts.maxEvents
task = cms.Task()
addRecoCommon(process, reco, cfg['inputs'], True, task)


def var(expr, typ):
    return cms.PSet(expr=cms.string(expr), type=cms.string(typ), doc=cms.string(expr), precision=cms.int32(-1))


kin = cms.PSet(pt=var('pt', 'float'), eta=var('eta', 'float'), phi=var('phi', 'float'), mass=var('mass', 'float'),
               pdgId=var('pdgId', 'int'))
tables = {name + 'Undressed': name + 'Undressed' for name in ('tightMuons', 'tightElectrons')}
tables.update({name + 'Dressed': name + 'Dressed' for name in ('tightMuons', 'tightElectrons')})
tables.update(AllMuons=cfg['inputs']['muons'], AllElectrons=cfg['inputs']['electrons'])
for name, src in tables.items():
    setattr(process, name + 'Table', cms.EDProducer('SimpleCandidateFlatTableProducer', src=cms.InputTag(src),
                                                   name=cms.string(name), doc=cms.string(''), singleton=cms.bool(False),
                                                   extension=cms.bool(False), cut=cms.string(''), variables=kin))
    task.add(getattr(process, name + 'Table'))
if opts.mode == 'fsr':
    from PhysicsTools.NanoAOD.fsrPhotons_cff import fsrTable
    process.fsrTable = fsrTable.clone(src='leptonFSRphotons')
    # NanoAOD stores FSR photon eta/phi with 12 mantissa bits; the check needs full float precision.
    for v in process.fsrTable.variables.parameterNames_():
        getattr(process.fsrTable.variables, v).precision = -1
    task.add(process.fsrTable)
process.out = cms.OutputModule('NanoAODOutputModule', fileName=cms.untracked.string(opts.outputFile),
                               outputCommands=cms.untracked.vstring('drop *', 'keep nanoaodFlatTable_*Table_*_*'))
process.e = cms.EndPath(process.out, task)
