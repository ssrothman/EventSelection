"""Applies the muon momentum corrections in every mode and writes the inputs and outputs per muon (double precision).
Usage: cmsRun validateMuonCorrections_cfg.py config=<analysis yaml> inputFiles=file:... outputFile=mu.root maxEvents=N
Tables: Mu (slimmedMuons: pt, eta, phi, charge, nTrackerLayers) and MuCorr<variant> (corrected pt, same order)."""
import FWCore.ParameterSet.Config as cms
import yaml
from FWCore.ParameterSet.VarParsing import VarParsing

opts = VarParsing('analysis')
opts.register('config', '', VarParsing.multiplicity.singleton, VarParsing.varType.string, 'analysis configuration YAML')
opts.parseArguments()
with open(opts.config) as f:
    cfg = yaml.safe_load(f)
mc, muons = cfg['selection']['reco']['muonCorrections'], cfg['inputs']['muons']

process = cms.Process('MUCORRVAL')
process.load('FWCore.MessageService.MessageLogger_cfi')
process.MessageLogger.cerr.FwkReport.reportEvery = 500
process.source = cms.Source('PoolSource', fileNames=cms.untracked.vstring(opts.inputFiles))
process.maxEvents.input = opts.maxEvents
task = cms.Task()


def var(expr, typ):
    return cms.PSet(expr=cms.string(expr), type=cms.string(typ), doc=cms.string(expr), precision=cms.int32(-1))


def table(name, src, variables, plugin='SimpleCandidateFlatTableProducer'):
    t = cms.EDProducer(plugin, src=cms.InputTag(src), name=cms.string(name),
                       doc=cms.string(''), singleton=cms.bool(False), extension=cms.bool(False), cut=cms.string(''),
                       variables=cms.PSet(**variables))
    setattr(process, name + 'Table', t)
    task.add(t)


table('Mu', muons, dict(pt=var('pt', 'double'), eta=var('eta', 'double'), phi=var('phi', 'double'),
                        charge=var('charge', 'int'),
                        nTrackerLayers=var('?innerTrack.isNonnull?innerTrack().hitPattern().trackerLayersWithMeasurement():0', 'int')),
      plugin='SimplePATMuonFlatTableProducer')
variants = dict(MCnominal=(False, 'nominal', 'cpp'), Datanominal=(True, 'nominal', 'cpp'),
                MCscaleUp=(False, 'scaleUp', 'cpp'), MCscaleDown=(False, 'scaleDown', 'cpp'),
                MCresolutionUpCpp=(False, 'resolutionUp', 'cpp'), MCresolutionDownCpp=(False, 'resolutionDown', 'cpp'),
                MCresolutionUpPython=(False, 'resolutionUp', 'python'),
                MCresolutionDownPython=(False, 'resolutionDown', 'python'))
for name, (isData, variation, kDef) in variants.items():
    setattr(process, 'corr' + name, cms.EDProducer('MuonCorrectionProducer', src=cms.InputTag(muons),
                                                   correctionFile=cms.string(mc['correctionFile']), isData=cms.bool(isData),
                                                   lowPtThreshold=cms.double(mc['lowPtThreshold']),
                                                   highPtThreshold=cms.double(mc['highPtThreshold']),
                                                   resolutionVariationK=cms.string(kDef),
                                                   variation=cms.string(variation)))
    task.add(getattr(process, 'corr' + name))
    table('MuCorr' + name, 'corr' + name, dict(pt=var('pt', 'double')))
process.out = cms.OutputModule('NanoAODOutputModule', fileName=cms.untracked.string(opts.outputFile),
                               outputCommands=cms.untracked.vstring('drop *', 'keep nanoaodFlatTable_*Table_*_*'))
process.e = cms.EndPath(process.out, task)
