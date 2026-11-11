# SRothman/EventSelection

Event selections for the boostedV EEC workflow, written as bool flags so that one cmsRun job handles every channel,
plus thin filters on those flags.

## Modules
| Module | Product(s) |
|---|---|
| `TriggerFlagProducer` | `bool`: OR (or AND, with `requireAll`) of the paths matching the glob `paths`. A pattern that matches nothing throws, unless `allowMissing`. Used for HLT paths and for the `Flag_*` MET filters. |
| `VertexFlagProducer` | `bool`: the first PV is not fake, ndof > 4, \|z\| < 24, ρ < 2. |
| `DileptonSelector` | `bool`, `"boson"`, `"leptons"`: the opposite-sign pair with lead/sublead pt cuts and mass in a window, closest to m_Z. |
| `LeptonMETSelector` | `bool`, `"boson"` (ℓ + MET, with MET p_z = 0), `"leptons"`: exactly one lepton above `ptMin`, no other veto lepton, MET and m_T cuts. |
| `SingleObjectEventSelector` | `bool`, `"objects"` (the leading object): nMin ≤ N ≤ nMax objects and empty veto collections. |
| `FlagCombiner` / `EventFlagFilter` | AND/OR of bool products, as a product or as a filter. |
| `EventFlagTableProducer` | singleton NanoAOD table with one bool column per flag. |

Framework-free logic (pair choice, m_T, veto overlap) lives in `interface/Selection.h`.

## Python: `SRothman.EventSelection.eventSelection_cff`
- `addCommon` creates the object collections (`tightMuons`, `vetoMuons`, `tightElectrons`, `vetoElectrons`, `tightPhotons`) with the existing `PAT*Selector` string cuts, plus `goodVertexFlag` and `metFilterFlag`.
- `addChannel(process, ch, suffix='', **cuts)` adds `<ch>Trigger`, `<ch>Sel`, `<ch>` (the AND of trigger, vertex, MET filters and selection) and `<ch>Filter`. Threshold variations: call it again with a `suffix` and the changed cuts.
- `addAllChannels` adds all channels, `anyChannel` / `anyChannelFilter`, and NanoAOD tables: `Selection_*` flags and `<ch>Boson` / `<ch>Objects`.
  Overrides: `objectCuts={collection: cut}`, `metFilters=[...]`, `triggers={channel: [globs]}` and
  `channelCuts={channel: {parameter: value}}`. Unknown collection or channel names raise.

Proposed defaults, all of them parameters:

| Channel | Trigger | Selection |
|---|---|---|
| Zmumu | `HLT_IsoMu24_v*` | tight ID + PFIsoTight muons, pt > 26/15, \|η\| < 2.4, OS, 71 < m < 111 |
| Zee | `HLT_Ele32_WPTight_Gsf_v*` | cutBased RunIIIWinter22 V1 tight electrons, pt > 35/15, \|η_SC\| < 2.5 excluding the EB/EE gap, OS, same window |
| Wmunu / Wenu | as Z | exactly one tight lepton with pt > 30/35, no other veto lepton, PUPPI MET > 40, m_T > 50 |
| Hadronic | `HLT_AK8PFJet500_v*` OR `HLT_PFHT1050_v*` | no veto lepton, no tight photon (jet requirements are in JetSetup) |
| GammaJets | `HLT_Photon200_v*` | exactly one RunIIIWinter22 tight photon with pt > 220, \|η_SC\| < 1.4442; no veto lepton |

MET filters (`MET_FILTERS`): goodVertices, globalSuperTightHalo2016Filter, EcalDeadCellTriggerPrimitiveFilter,
BadPFMuonFilter, BadPFMuonDzFilter, hfNoisyHitsFilter, eeBadScFilter, ecalBadCalibFilter.

## Tests
- `scram b runtests` runs `testSRothmanEventSelection` (logic) and `testSRothmanEventSelectionModules` (TestProcessor).
- Real-data validation:
  `cmsRun test/validation/validateSelection_cfg.py inputFiles=file:... outputFile=sel.root maxEvents=2000`, then
  `python3 test/validation/checkSelection.py sel_numEvent2000.root <same MiniAOD>`. The second step compares Z→μμ with an independent FWLite selection.
