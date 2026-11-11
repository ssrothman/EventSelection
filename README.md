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
The helpers take the `selection` and `inputs` sections of the boostedV analysis YAML (`simulation/configs/default.yaml`)
as dicts and have no physics defaults. Cuts, triggers, MET filters, object definitions and the channels themselves are set there.

Selections exist at two levels:
- **Reco** (labels unprefixed): PAT string cuts on MiniAOD objects; every channel also requires its trigger, the good vertex and the MET filters.
- **Gen** (MC only; labels `gen` + capitalised name, e.g. `genZmumuSel`): built on the Rivet particle level (`ParticleLevelProducer`, as in NanoAOD) via `genMergedParticles → genParticles2HepMC → genParticleLevel`. That gives prompt final-state leptons dressed with photons, isolated prompt photons, and MET from prompt neutrinos (`genParticleLevel:mets`). Objects use candidate string cuts (`CandPtrSelector`); there are no triggers or event filters. The particle-data-table ESSource is loaded automatically.

Both levels use the same selector modules and channel types (`dilepton`, `leptonMET`, `singleObject`).

- `addSelection(process, selection, inputs, levels, isMC)` builds reco and, in MC, gen. Both levels are always built so their flags and selected objects exist.
  - Per level it adds the OR of that level's channels, `anyChannel` / `genAnyChannel`.
  - `anySelected` / `anySelectedFilter` is the OR over `levels`.
  - NanoAOD tables: `Selection_*` and `GenSelection_*` flags, and `<ch>Boson`/`<ch>Objects` and `Gen<ch>Boson`/`Gen<ch>Objects` candidates.
- `addChannel(process, level, name, spec, inputs, suffix='')` builds one channel; for threshold variations, call it again with a `suffix`.
- `selectedObjects(levelCfg, level, channels)` gives the InputTags of a level's selected objects, for jet cleaning.
- `label(level, name)` gives a module's label at a level.

## Tests
- `scram b runtests` runs `testSRothmanEventSelection` (logic) and `testSRothmanEventSelectionModules` (TestProcessor).
- Real-data validation:
  `cmsRun test/validation/validateSelection_cfg.py config=<analysis yaml> inputFiles=file:<MC MiniAOD> outputFile=sel.root maxEvents=2000`, then
  `python3 test/validation/checkSelection.py sel_numEvent2000.root <same MiniAOD>`. The second step compares Z→μμ with an independent FWLite selection.
