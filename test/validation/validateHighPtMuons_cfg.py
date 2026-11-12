"""Runs MuonCorrectionProducer in the momentum modes and high-pT variants and writes, per muon (double precision), the
inputs and the outputs. Checked by checkHighPtMuons.py.
Usage: cmsRun validateHighPtMuons_cfg.py era=2022postEE inputFiles=file:... outputFile=hp.root maxEvents=N
Tables: Mu (slimmedMuons: pt, eta, phi, charge, tunePPt) and MuCorr<variant> (pt, highPtRegime; same order)."""
import FWCore.ParameterSet.Config as cms
from FWCore.ParameterSet.VarParsing import VarParsing
from SRothman.EventSelection.muonHighPtRun3_cff import highPtPSet

opts = VarParsing('analysis')
opts.register('era', '2022postEE', VarParsing.multiplicity.singleton, VarParsing.varType.string, 'high-pT table era')
opts.parseArguments()

process = cms.Process('HIGHPTVAL')
process.load('FWCore.MessageService.MessageLogger_cfi')
process.MessageLogger.cerr.FwkReport.reportEvery = 500
process.source = cms.Source('PoolSource', fileNames=cms.untracked.vstring(opts.inputFiles))
process.maxEvents.input = opts.maxEvents
task = cms.Task()
SCARE = '/cvmfs/cms-griddata.cern.ch/cat/metadata/MUO/Run3-22EFGSep23-Summer22EE-NanoAODv12/latest/muon_scalesmearing.json.gz'


def var(expr, typ):
    return cms.PSet(expr=cms.string(expr), type=cms.string(typ), doc=cms.string(expr), precision=cms.int32(-1))


def table(name, src, variables):
    t = cms.EDProducer('SimplePATMuonFlatTableProducer', src=cms.InputTag(src), name=cms.string(name),
                       doc=cms.string(''), singleton=cms.bool(False), extension=cms.bool(False), cut=cms.string(''),
                       variables=cms.PSet(**variables))
    setattr(process, name + 'Table', t)
    task.add(t)


table('Mu', 'slimmedMuons', dict(pt=var('pt', 'double'), eta=var('eta', 'double'), phi=var('phi', 'double'),
                                 charge=var('charge', 'int'),
                                 tunePPt=var('?tunePMuonBestTrack.isNonnull?tunePMuonBestTrack().pt():-1', 'double')))
# name: (momentum, apply, isData, variation, kappaTarget)
variants = dict(PF=('pf', False, False, 'nominal', 'mc'), TuneP=('tuneP', False, False, 'nominal', 'mc'),
                Hybrid=('hybrid', False, False, 'nominal', 'mc'),
                MCnominal=('hybrid', True, False, 'nominal', 'mc'),
                MCscaleUp=('hybrid', True, False, 'highPtScaleUp', 'mc'),
                MCscaleDown=('hybrid', True, False, 'highPtScaleDown', 'mc'),
                MCresolution=('hybrid', True, False, 'highPtResolution', 'mc'),
                MCtargetData=('hybrid', True, False, 'nominal', 'data'),
                MCtargetDataScaleUp=('hybrid', True, False, 'highPtScaleUp', 'data'),
                DataTargetMC=('hybrid', True, True, 'nominal', 'mc'),
                DataTargetData=('hybrid', True, True, 'nominal', 'data'))
for name, (momentum, apply, isData, variation, target) in variants.items():
    setattr(process, 'corr' + name, cms.EDProducer(
        'MuonCorrectionProducer', src=cms.InputTag('slimmedMuons'), isData=cms.bool(isData),
        momentum=cms.string(momentum), regimeThreshold=cms.double(200.0), applyCorrections=cms.bool(apply),
        correctionFile=cms.string(SCARE), lowPtThreshold=cms.double(26.0), highPtThreshold=cms.double(200.0),
        resolutionVariationK=cms.string('cpp'), variation=cms.string(variation),
        highPt=highPtPSet(opts.era, 200.0, target)))
    task.add(getattr(process, 'corr' + name))
    table('MuCorr' + name, 'corr' + name, dict(pt=var('pt', 'double'), regime=var("userInt('highPtRegime')", 'int')))
process.out = cms.OutputModule('NanoAODOutputModule', fileName=cms.untracked.string(opts.outputFile),
                               outputCommands=cms.untracked.vstring('drop *', 'keep nanoaodFlatTable_*Table_*_*'))
process.e = cms.EndPath(process.out, task)
