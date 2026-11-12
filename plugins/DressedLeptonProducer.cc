#include "DataFormats/Candidate/interface/LeafCandidate.h"
#include "DataFormats/Common/interface/ValueMap.h"
#include "DataFormats/PatCandidates/interface/Electron.h"
#include "DataFormats/PatCandidates/interface/GenericParticle.h"
#include "DataFormats/PatCandidates/interface/PackedCandidate.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/global/EDProducer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "SRothman/EventSelection/plugins/Common.h"

#include <unordered_set>
#include <vector>

// Leptons with photons added to their four-momentum ("dressing"), as std::vector<reco::LeafCandidate>.
//  mode fsr:  adds the lepton's FSR photon from LeptonFSRProducer (CommonTools/RecoUtils; at most one photon, the
//             lowest dR/ET^2). Each input lepton is a copy of an element of `originals` (the collection given to
//             LeptonFSRProducer) and is matched to it by identical pt, eta and phi.
//  mode cone: adds every PF photon with pt > photonPtMin within dR < coneSize, each to the nearest lepton of `src`,
//             excluding photons in the footprint of any electron in footprintElectrons (already in the electron
//             energy); photons are optionally scaled by their PUPPI weight.
class DressedLeptonProducer : public edm::global::EDProducer<> {
public:
  explicit DressedLeptonProducer(const edm::ParameterSet& cfg)
      : src_(consumes(cfg.getParameter<edm::InputTag>("src"))),
        put_(produces()),
        cone_(parseMode(cfg.getParameter<std::string>("mode"))),
        coneSize_(cfg.getParameter<double>("coneSize")),
        photonPtMin_(cfg.getParameter<double>("photonPtMin")),
        applyPuppi_(cfg.getParameter<bool>("applyPuppiWeights")) {
    if (cone_) {
      pf_ = consumes(cfg.getParameter<edm::InputTag>("pfCandidates"));
      electrons_ = consumes(cfg.getParameter<edm::InputTag>("footprintElectrons"));
    } else {
      fsrPhotons_ = consumes(cfg.getParameter<edm::InputTag>("fsrPhotons"));
      fsrIndex_ = consumes(cfg.getParameter<edm::InputTag>("fsrIndex"));
      originals_ = consumes(cfg.getParameter<edm::InputTag>("originals"));
    }
  }

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::InputTag>("src", edm::InputTag("tightMuonsUndressed"));
    desc.add<std::string>("mode", "cone")->setComment("fsr | cone");
    desc.add<edm::InputTag>("fsrPhotons", edm::InputTag("leptonFSRphotons"))->setComment("fsr mode");
    desc.add<edm::InputTag>("fsrIndex", edm::InputTag("leptonFSRphotons", "muFsrIndex"))
        ->setComment("fsr mode: muFsrIndex or eleFsrIndex");
    desc.add<edm::InputTag>("originals", edm::InputTag("slimmedMuons"))
        ->setComment("fsr mode: the lepton collection given to LeptonFSRProducer");
    desc.add<edm::InputTag>("pfCandidates", edm::InputTag("packedPFCandidates"))->setComment("cone mode");
    desc.add<edm::InputTag>("footprintElectrons", edm::InputTag("slimmedElectrons"))->setComment("cone mode");
    desc.add<double>("coneSize", 0.1)->setComment("cone mode");
    desc.add<double>("photonPtMin", 0.0)->setComment("cone mode");
    desc.add<bool>("applyPuppiWeights", true)->setComment("cone mode");
    descriptions.addWithDefaultLabel(desc);
  }

  void produce(edm::StreamID, edm::Event& event, const edm::EventSetup&) const override {
    const auto& leps = event.get(src_);
    std::vector<reco::LeafCandidate> out;
    out.reserve(leps.size());
    for (const auto& l : leps)
      out.push_back(evsel::copy(l));
    if (cone_)
      dressCone(event, leps, out);
    else
      dressFsr(event, leps, out);
    event.emplace(put_, std::move(out));
  }

private:
  static bool parseMode(const std::string& m) {
    if (m == "cone")
      return true;
    if (m == "fsr")
      return false;
    throw cms::Exception("Configuration") << "DressedLeptonProducer: mode must be fsr or cone, got " << m;
  }

  void dressCone(edm::Event& event, const edm::View<reco::Candidate>& leps, std::vector<reco::LeafCandidate>& out) const {
    const auto pf = event.getHandle(pf_);
    std::unordered_set<unsigned> footprint;
    for (const auto& e : event.get(electrons_))
      for (const auto& ref : e.associatedPackedPFCandidates())
        if (ref.id() == pf.id())
          footprint.insert(ref.key());
    std::vector<evsel::Obj> photons;
    std::vector<reco::Candidate::LorentzVector> p4;
    for (unsigned i = 0; i < pf->size(); ++i) {
      const auto& c = (*pf)[i];
      if (c.pdgId() != 22 || c.pt() <= photonPtMin_ || footprint.count(i))
        continue;
      const double w = applyPuppi_ ? c.puppiWeight() : 1.0;
      if (!(w > 0))
        continue;
      photons.push_back(evsel::toObj(c));
      p4.push_back(c.p4() * w);
    }
    const auto owner = evsel::assignToNearest(evsel::toObjs(leps), photons, coneSize_);
    for (std::size_t g = 0; g < owner.size(); ++g)
      if (owner[g] >= 0)
        out[owner[g]].setP4(out[owner[g]].p4() + p4[g]);
  }

  void dressFsr(edm::Event& event, const edm::View<reco::Candidate>& leps, std::vector<reco::LeafCandidate>& out) const {
    const auto& photons = event.get(fsrPhotons_);
    const auto& index = event.get(fsrIndex_);
    const auto originals = event.getHandle(originals_);
    for (std::size_t l = 0; l < leps.size(); ++l) {
      std::size_t i = 0;
      // The selected leptons are copies, so their kinematics are bit-identical to the original's.
      while (i < originals->size() && !((*originals)[i].pt() == leps[l].pt() && (*originals)[i].eta() == leps[l].eta() &&
                                        (*originals)[i].phi() == leps[l].phi()))
        ++i;
      if (i == originals->size())
        throw cms::Exception("DressedLeptonProducer") << "lepton " << l << " (pt " << leps[l].pt()
                                                      << ") not found in the originals collection";
      const int g = index.get(originals.id(), i);
      if (g >= 0)
        out[l].setP4(out[l].p4() + photons.at(g).p4());
    }
  }

  const edm::EDGetTokenT<edm::View<reco::Candidate>> src_;
  const edm::EDPutTokenT<std::vector<reco::LeafCandidate>> put_;
  const bool cone_;
  const double coneSize_, photonPtMin_;
  const bool applyPuppi_;
  edm::EDGetTokenT<pat::PackedCandidateCollection> pf_;
  edm::EDGetTokenT<pat::ElectronCollection> electrons_;
  edm::EDGetTokenT<std::vector<pat::GenericParticle>> fsrPhotons_;
  edm::EDGetTokenT<edm::ValueMap<int>> fsrIndex_;
  edm::EDGetTokenT<edm::View<reco::Candidate>> originals_;
};

DEFINE_FWK_MODULE(DressedLeptonProducer);
