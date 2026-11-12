"""Runs the reco selection of an analysis configuration (optionally patched by muonRecipes.RECIPES[recipe]) on MC
MiniAOD and writes the muon SF tables, the channels' selected leptons and the corrected muons. Checked by checkMuonSF.py.
Usage: cmsRun validateMuonSF_cfg.py config=<analysis yaml> recipe=default|hybrid inputFiles=file:... outputFile=sf.root"""
import inspect
import os
import sys

import FWCore.ParameterSet.Config as cms
import yaml
from FWCore.ParameterSet.VarParsing import VarParsing
from SRothman.EventSelection.eventSelection_cff import addSelection

sys.path.insert(0, os.path.dirname(os.path.abspath(inspect.currentframe().f_code.co_filename)))
import muonRecipes  # noqa: E402

opts = VarParsing('analysis')
opts.register('config', '', VarParsing.multiplicity.singleton, VarParsing.varType.string, 'analysis configuration YAML')
opts.register('recipe', 'default', VarParsing.multiplicity.singleton, VarParsing.varType.string, 'muonRecipes key')
opts.parseArguments()
with open(opts.config) as f:
    cfg = muonRecipes.RECIPES[opts.recipe](yaml.safe_load(f))

process = cms.Process('SFVAL')
process.load('FWCore.MessageService.MessageLogger_cfi')
process.MessageLogger.cerr.FwkReport.reportEvery = 500
process.source = cms.Source('PoolSource', fileNames=cms.untracked.vstring(opts.inputFiles))
process.maxEvents.input = opts.maxEvents
task = cms.Task()
addSelection(process, cfg['selection'], cfg['inputs'], levels=['reco'], isMC=True, task=task, withTables=True)


def var(expr, typ):
    return cms.PSet(expr=cms.string(expr), type=cms.string(typ), doc=cms.string(expr), precision=cms.int32(-1))


kin = dict(pt=var('pt', 'double'), eta=var('eta', 'double'), phi=var('phi', 'double'), charge=var('charge', 'int'))
for ch in cfg['selection']['reco']['muonScaleFactors']:
    t = cms.EDProducer('SimpleCandidateFlatTableProducer', src=cms.InputTag(ch + 'Sel', 'leptons'), name=cms.string(ch + 'Lep'),
                       doc=cms.string(''), singleton=cms.bool(False), extension=cms.bool(False), cut=cms.string(''),
                       variables=cms.PSet(**kin))
    setattr(process, ch + 'LepTable', t)
    task.add(t)
process.CorrMuTable = cms.EDProducer('SimplePATMuonFlatTableProducer', src=cms.InputTag('correctedMuons'),
                                     name=cms.string('CorrMu'), doc=cms.string(''), singleton=cms.bool(False),
                                     extension=cms.bool(False), cut=cms.string(''),
                                     variables=cms.PSet(regime=var("userInt('highPtRegime')", 'int'), **kin))
task.add(process.CorrMuTable)
process.out = cms.OutputModule('NanoAODOutputModule', fileName=cms.untracked.string(opts.outputFile),
                               outputCommands=cms.untracked.vstring('drop *', 'keep nanoaodFlatTable_*Table_*_*'))
process.e = cms.EndPath(process.out, task)
