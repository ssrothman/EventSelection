"""Runs the reco and gen event selections of an analysis configuration on MC MiniAOD and writes their NanoAOD tables.
Usage: cmsRun validateSelection_cfg.py config=<analysis yaml> inputFiles=file:... outputFile=sel.root maxEvents=2000"""
import FWCore.ParameterSet.Config as cms
import yaml
from FWCore.ParameterSet.VarParsing import VarParsing
from SRothman.EventSelection.eventSelection_cff import addSelection

opts = VarParsing('analysis')
opts.register('config', '', VarParsing.multiplicity.singleton, VarParsing.varType.string, 'analysis configuration YAML')
opts.parseArguments()
with open(opts.config) as f:
    cfg = yaml.safe_load(f)

process = cms.Process('SELVAL')
process.load('FWCore.MessageService.MessageLogger_cfi')
process.MessageLogger.cerr.FwkReport.reportEvery = 500
process.source = cms.Source('PoolSource', fileNames=cms.untracked.vstring(opts.inputFiles))
process.maxEvents.input = opts.maxEvents
task = cms.Task()
addSelection(process, cfg['selection'], cfg['inputs'], levels=['reco', 'gen'], isMC=True, task=task)
process.out = cms.OutputModule('NanoAODOutputModule', fileName=cms.untracked.string(opts.outputFile),
                               outputCommands=cms.untracked.vstring('drop *', 'keep nanoaodFlatTable_*Table_*_*'))
process.e = cms.EndPath(process.out, task)
