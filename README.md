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

- `addCommon` creates the object collections from `selection/objects` (PAT string cuts; source muons, electrons or photons), plus `goodVertexFlag` and `metFilterFlag`.
- `addChannel(process, name, spec, inputs, suffix='')` builds one channel: `<ch>Trigger`, `<ch>Sel`, `<ch>` (the AND of trigger, vertex, MET filters and selection) and `<ch>Filter`. The channel type is `dilepton`, `leptonMET` or `singleObject`, so a new channel needs only a YAML entry. Threshold variations: call it again with a modified spec and a `suffix`.
- `addSelection` adds every channel, `anyChannel` / `anyChannelFilter`, and NanoAOD tables: `Selection_*` flags and `<ch>Boson` / `<ch>Objects`.
- `selectedObjects(selection, channels)` gives the InputTags of the channels' selected objects, for jet cleaning.

## Tests
- `scram b runtests` runs `testSRothmanEventSelection` (logic) and `testSRothmanEventSelectionModules` (TestProcessor).
- Real-data validation:
  `cmsRun test/validation/validateSelection_cfg.py config=<analysis yaml> inputFiles=file:... outputFile=sel.root maxEvents=2000`, then
  `python3 test/validation/checkSelection.py sel_numEvent2000.root <same MiniAOD>`. The second step compares Z→μμ with an independent FWLite selection.
