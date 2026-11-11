"""Runs all event selections on MiniAOD and writes their NanoAOD tables.
Usage: cmsRun validateSelection_cfg.py inputFiles=file:... outputFile=sel.root maxEvents=2000"""
import FWCore.ParameterSet.Config as cms
from FWCore.ParameterSet.VarParsing import VarParsing
from SRothman.EventSelection.eventSelection_cff import addAllChannels

opts = VarParsing('analysis')
opts.parseArguments()
process = cms.Process('SELVAL')
process.load('FWCore.MessageService.MessageLogger_cfi')
process.MessageLogger.cerr.FwkReport.reportEvery = 500
process.source = cms.Source('PoolSource', fileNames=cms.untracked.vstring(opts.inputFiles))
process.maxEvents.input = opts.maxEvents
task = cms.Task()
addAllChannels(process, task)
process.out = cms.OutputModule('NanoAODOutputModule', fileName=cms.untracked.string(opts.outputFile),
                               outputCommands=cms.untracked.vstring('drop *', 'keep nanoaodFlatTable_*Table_*_*'))
process.e = cms.EndPath(process.out, task)
