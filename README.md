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
| `DressedLeptonProducer` | leptons with photons added to their momentum. `fsr`: the lepton's FSR photon from CMSSW's `LeptonFSRProducer` (at most one isolated photon outside electron footprints, lowest ΔR/E_T²). `cone`: every PF photon within `coneSize` of the nearest lepton of the collection, outside every electron's footprint, optionally PUPPI-weighted (Rivet-like). |
| `MuonCorrectionProducer` | muon copies with the momentum assignment (`momentum`: `pf`, `tuneP`, or `hybrid` = TuneP if PF pT ≥ `regimeThreshold`) and userInt `highPtRegime` (0/1). With `applyCorrections`, MUO POG Run-3 corrections: regime 0, "MuonScaRe" (CMS-DP-2024-065) scale in data and MC and resolution smearing in MC, for `lowPtThreshold` ≤ pT ≤ `highPtThreshold`; regime 1 (TuneP pT ≥ `highPt.ptMin`, \|η\| < 2.4), GE κ_b (q/pT + κ_b on MC, or q/pT − κ_b on data, per `highPt.kappaTarget`) then MC smearing p → p(1 + N(0, σ(p, η)·f)). Job-wide `variation` (MC): scaleUp/Down, resolutionUp/Down (regime 0); highPtScaleUp/Down (κ_b ± its uncertainty), highPtResolution (extra smearing, f = 0.46) (regime 1). |
| `MuonSFProducer` | a channel's muon efficiency scale factors: `double` event weight and a singleton NanoAOD table (`nominal`, `<term>_{stat,syst}{Up,Down}`). Each selected lepton is matched to the nearest same-charge `correctedMuons` muon (ΔR < `matchDeltaR`); terms (correctionlib keys) apply to muons of their regime. Per-muon terms multiply. Trigger terms (≤ 1 per regime, HLT path globs `paths`) apply only to the triggering muon: the highest-pT muon within `triggerMatchDeltaR` of a `triggerObjects` object from the last filter of a fired path of its regime's term (provisional, see item 6 below). If such a path fired but no muon matches, a warning is emitted and there is no trigger SF. All weights are 1 for events failing the channel flag `selected`. `outOfRange: skip` gives SF 1. |

Framework-free logic (pair choice, m_T, veto overlap) lives in `interface/Selection.h`.

## Python: `SRothman.EventSelection.eventSelection_cff`
The helpers take the `selection` and `inputs` sections of the boostedV analysis YAML (`simulation/configs/default.yaml`)
as dicts and have no physics defaults. Cuts, triggers, MET filters, object definitions and the channels themselves are set there.

Selections exist at two levels:
- **Reco** (labels unprefixed): PAT string cuts on MiniAOD objects; every channel also requires its trigger, the good vertex and the MET filters. Two optional steps (both off in `default.yaml`):
  - **Muon momentum and corrections** (`muonCorrections`): `MuonCorrectionProducer` (`correctedMuons`) is added, and every muon consumer reads it, when `apply` is set, `momentum` is not `pf`, or (MC) any channel has muon scale factors.
    - `momentum`: `pf` (medium-pT recipe), `tuneP` (high-pT recipe) or `hybrid` (per muon by PF pT vs `regimeThreshold`). Object cuts can use `userInt('highPtRegime')` to pick the ID per regime.
    - Medium regime: the MUO POG Run-3 scale (data and MC) and resolution (MC) corrections from `muon_scalesmearing.json`, ported from `jsonpog-integration/examples/MuonScaRe.cc` (`interface/MuonScaRe.h`). Muons outside [`lowPtThreshold`, `highPtThreshold`] = [26, 200] GeV are unchanged; the upper bound follows the POG Python and the stated validity, while the POG C++ corrects the scale at any pT above 26.
    - High regime (`highPt`: `era`, `ptMin`, `kappaTarget`): the MUO POG wiki's Run-3 GE κ_b tables and MC smearing (`interface/MuonHighPt.h`, tables in `python/muonHighPtRun3_cff.py` for 2022preEE, 2022postEE, 2023preBPix, 2023postBPix). Smearing random numbers are seeded per (run, lumi, event, muon index).
    - A job-wide `variation` (MC only) selects nominal, scaleUp/Down, resolutionUp/Down, highPtScaleUp/Down or highPtResolution.
  - **Muon scale factors** (`muonScaleFactors`, MC only): a `MuonSFProducer` per listed channel (`<ch>MuonSFTable`, table `<ch>MuonSF`); `eecNano_cfg.py` multiplies the nominal weight into that channel's reco-level aggregated histograms.
    `resolutionVariationK: cpp | python` selects the k in the resolution variation; the default `cpp` is provisional, see [Muon corrections: points to clarify with the MUO POG](#muon-corrections-points-to-clarify-with-the-muo-pog).
  - **Lepton dressing** (`dressing.mode`): `fsr` adds the CMSSW `LeptonFSRProducer` photon (at most one per lepton, as for NanoAOD `FsrPhoton`); `cone` adds every PF photon within ΔR of the nearest lepton, outside electron footprints, optionally PUPPI-weighted (Rivet-like). A lepton collection becomes `<name>Undressed` (PAT `cut`) → `<name>Dressed` (`DressedLeptonProducer`) → `<name>` (`dressedCut`). Isolation is not recomputed: PFIso still counts the dressing photons.
- **Gen** (MC only; labels `gen` + capitalised name, e.g. `genZmumuSel`): built on the Rivet particle level (`ParticleLevelProducer`, as in NanoAOD) via `genMergedParticles → genParticles2HepMC → genParticleLevel`. That gives prompt final-state leptons dressed with photons, isolated prompt photons, and MET from prompt neutrinos (`genParticleLevel:mets`). Objects use candidate string cuts (`CandPtrSelector`); there are no triggers or event filters. The particle-data-table ESSource is loaded automatically.

Both levels use the same selector modules and channel types (`dilepton`, `leptonMET`, `singleObject`).

- **Reco options (default: off):**
  - `muonCorrections` (`apply`, non-`pf` `momentum`, or MC muon scale factors) puts `correctedMuons` in front of every muon consumer (selectors, FSR finder, dressing).
  - `dressing.mode` is `none` | `fsr` | `cone`. With dressing, a lepton collection `<name>` becomes `<name>Undressed` (PAT `cut`) → `<name>Dressed` → `dressedCut` (candidate cut on the dressed four-vector); photons are never dressed. Isolation is not recomputed, so PFIso still counts the dressing photons.
- `addSelection(process, selection, inputs, levels, isMC)` builds reco and, in MC, gen. Both levels are always built so their flags and selected objects exist.
  - Per level it adds the OR of that level's channels, `anyChannel` / `genAnyChannel`.
  - `anySelected` / `anySelectedFilter` is the OR over `levels`.
  - NanoAOD tables: `Selection_*` and `GenSelection_*` flags, and `<ch>Boson`/`<ch>Objects` and `Gen<ch>Boson`/`Gen<ch>Objects` candidates.
- `addChannel(process, level, name, spec, inputs, suffix='')` builds one channel; for threshold variations, call it again with a `suffix`.
- `selectedObjects(levelCfg, level, channels)` gives the InputTags of a level's selected objects, for jet cleaning.
- `label(level, name)` gives a module's label at a level.

## Tests
- `scram b runtests` runs `testSRothmanEventSelection` (logic) and `testSRothmanEventSelectionModules` (TestProcessor).
- `testSRothmanEventSelectionMuonScaRe`: the muon-correction port (seeds against the POG Python, scale formula, thresholds, Crystal Ball inverse, both resolution-variation definitions).
- `testSRothmanEventSelectionMuonHighPt`: κ_b/η/φ binning, curvature shift, σ(p), smearing guard, seeded Gaussian.
- `testSRothmanEventSelectionMuonSF`: `MuonSFProducer` against correctionlib evaluated directly (cvmfs Summer22EE files): regimes, p vs pT inputs, matching, HLT matching (leading muon, fired path, last filter, ΔR, term paths), channel flag, variations, out-of-range handling, configuration errors.
- Real-data validation:
  `cmsRun test/validation/validateSelection_cfg.py config=<analysis yaml> inputFiles=file:<MC MiniAOD> outputFile=sel.root maxEvents=2000`, then
  `python3 test/validation/checkSelection.py sel_numEvent2000.root <same MiniAOD>`. The second step compares Z→μμ with an independent FWLite selection.
- Dressing: `cmsRun test/validation/validateDressing_cfg.py config=<yaml> mode=fsr|cone inputFiles=file:... outputFile=d.root maxEvents=N`, then `python3 test/validation/checkDressing.py fsr|cone d_numEventN.root <MiniAOD>`.
  - `fsr` is checked against the FSR producer's photons.
  - `cone` is checked against an independent FWLite recomputation.
- Muon corrections: `cmsRun test/validation/validateMuonCorrections_cfg.py config=<yaml> inputFiles=file:... outputFile=mu.root maxEvents=N`, then `python3 test/validation/checkMuonCorrections.py mu_numEventN.root <muon_scalesmearing.json.gz>`. It compares every mode with the POG's `MuonScaRe.py`, and the `cpp` resolution variation with the `MuonScaRe.cc` formula.

- High-pT muons:
  - `python3 test/validation/checkHighPtTables.py <muo-wiki>/docs/guidelines/corrections.md` checks every value of `python/muonHighPtRun3_cff.py` against the wiki source.
  - `cmsRun test/validation/validateHighPtMuons_cfg.py era=<era> inputFiles=file:... outputFile=hp.root`, then `python3 test/validation/checkHighPtMuons.py hp.root <era>`: momentum and regime per mode, exact κ_b shifts, smearing pulls.
- Muon scale factors: `cmsRun test/validation/validateMuonSF_cfg.py config=<yaml> recipe=default|hybrid inputFiles=file:... outputFile=sf.root`, then `python3 test/validation/checkMuonSF.py sf.root <yaml> default|hybrid` recomputes every weight column independently. `recipe=hybrid` (`test/validation/muonRecipes.py`) is a worked hybrid configuration (IDs, triggers and SF keys).

## Muon corrections: points to clarify with the MUO POG
**Ask the POG:** the muon smearing and its systematics in every mode (medium, high and hybrid; items 2, 4 and the seed note below) are provisional.

The two POG reference implementations (`jsonpog-integration/examples/MuonScaRe.cc` and `MuonScaRe.py`) disagree:
1. **Scale above 200 GeV.** The C++ corrects all muons above 26 GeV; the Python and the README's stated validity (pT < 200 GeV) leave them unchanged. We use `highPtThreshold: 200` (configurable).
2. **k in the resolution variation.** The C++ uses the residual smearing factor √(k_data² − k_mc²), i.e. the factor used for the smearing; the Python uses k_mc. The two differ by up to 0.6% in pT on DY muons. Configurable as `resolutionVariationK: cpp | python`; the default `cpp` is **provisional until the POG clarifies**.

3. **High-pT κ_b.** The wiki gives the Run-3 tables and says to correct the pT, but not the formula, sign, units or which sample. We assume κ_b in c/TeV and q/pT → q/pT + κ_b on MC (`kappaTarget: mc`, default) or q/pT − κ_b on data (`kappaTarget: data`). **Provisional until the POG confirms.**
4. **High-pT resolution systematic.** We read the wiki's p → p(1 + N(0, 0.46·σ)) as an extra smearing on top of the nominal one: replacing the nominal factor would give no variation where it is already 0.46 (2022postEE endcap) and a reduction where it is 0.568 (2023postBPix endcap).
5. **PUPPI MET with TuneP muons (Wmunu). Ask the POG.** The wiki says PF quantities such as MET need the muon's PF pT replaced by its TuneP pT; whether and how this applies to PUPPI MET is not stated. Not implemented.
6. **Trigger SF and HLT matching. Ask the POG.** Only the triggering muon gets a trigger SF. Provisionally this is the highest-pT selected muon matched to an HLT object of its regime's term. If a path fired but no muon matches (the trigger was fired by another object), the module warns and applies no trigger SF.

All open questions of the project: `OPEN_QUESTIONS.md` (repository root).

Also worth confirming: in both references the smearing seed comes from a seed sequence of length one, which uses only its first input. So only the event number seeds the smearing, and all muons in an event share the same uniform random number (lumi and φ have no effect). The non-nested code path of `MuonScaRe.py` is broken (`get_rndm` uses an undefined `evtNr_f`).
