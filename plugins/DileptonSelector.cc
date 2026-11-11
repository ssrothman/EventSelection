#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/global/EDProducer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "SRothman/EventSelection/plugins/Common.h"

// Z -> ll: among the (already identified) leptons in src, the pair passing evsel::PairCuts with mass closest
// to targetMass. Products: bool; "boson" (0 or 1 candidate, the dilepton); "leptons" (0 or 2, leading first).
class DileptonSelector : public edm::global::EDProducer<> {
public:
  explicit DileptonSelector(const edm::ParameterSet& cfg)
      : src_(consumes(cfg.getParameter<edm::InputTag>("src"))),
        flag_(produces()),
        boson_(produces("boson")),
        leptons_(produces("leptons")) {
    cuts_.leadPtMin = cfg.getParameter<double>("leadPtMin");
    cuts_.subleadPtMin = cfg.getParameter<double>("subleadPtMin");
    cuts_.massMin = cfg.getParameter<double>("massMin");
    cuts_.massMax = cfg.getParameter<double>("massMax");
    cuts_.oppositeCharge = cfg.getParameter<bool>("oppositeCharge");
    cuts_.targetMass = cfg.getParameter<double>("targetMass");
  }

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::InputTag>("src", edm::InputTag("tightMuons"));
    desc.add<double>("leadPtMin", 26.0);
    desc.add<double>("subleadPtMin", 15.0);
    desc.add<double>("massMin", 71.0);
    desc.add<double>("massMax", 111.0);
    desc.add<bool>("oppositeCharge", true);
    desc.add<double>("targetMass", 91.1876);
    descriptions.addWithDefaultLabel(desc);
  }

  void produce(edm::StreamID, edm::Event& event, const edm::EventSetup&) const override {
    const auto& leps = event.get(src_);
    const auto pair = evsel::bestPair(evsel::toObjs(leps), cuts_);
    std::vector<reco::LeafCandidate> boson, out;
    if (pair) {
      const auto& a = leps[pair->first];
      const auto& b = leps[pair->second];
      out = {evsel::copy(a), evsel::copy(b)};
      boson.emplace_back(a.charge() + b.charge(), a.p4() + b.p4());
    }
    event.emplace(flag_, pair.has_value());
    event.emplace(boson_, std::move(boson));
    event.emplace(leptons_, std::move(out));
  }

private:
  const edm::EDGetTokenT<edm::View<reco::Candidate>> src_;
  const edm::EDPutTokenT<bool> flag_;
  const edm::EDPutTokenT<std::vector<reco::LeafCandidate>> boson_, leptons_;
  evsel::PairCuts cuts_;
};

DEFINE_FWK_MODULE(DileptonSelector);
