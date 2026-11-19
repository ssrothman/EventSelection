#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/global/EDProducer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/transform.h"
#include "SRothman/EventSelection/plugins/Common.h"

// Z -> ll: among the (already identified) leptons in src, the pair passing evsel::PairCuts with mass closest
// to targetMass, if its pt is at least pairPtMin and no candidate of vetoSrcs other than the pair (dR > 0.01 from both)
// exists. Products: bool; "boson" (0 or 1 candidate, the dilepton); "leptons" (0 or 2, leading first);
// "leptonPtrs" (pointers to the same two leptons in src, e.g. to find their PF candidates or dressing photons).
class DileptonSelector : public edm::global::EDProducer<> {
public:
  explicit DileptonSelector(const edm::ParameterSet& cfg)
      : src_(consumes(cfg.getParameter<edm::InputTag>("src"))),
        vetoes_(edm::vector_transform(cfg.getParameter<std::vector<edm::InputTag>>("vetoSrcs"),
                                      [this](const edm::InputTag& t) { return consumes<edm::View<reco::Candidate>>(t); })),
        flag_(produces()),
        boson_(produces("boson")),
        leptons_(produces("leptons")),
        leptonPtrs_(produces("leptonPtrs")) {
    cuts_.leadPtMin = cfg.getParameter<double>("leadPtMin");
    cuts_.subleadPtMin = cfg.getParameter<double>("subleadPtMin");
    cuts_.massMin = cfg.getParameter<double>("massMin");
    cuts_.massMax = cfg.getParameter<double>("massMax");
    cuts_.oppositeCharge = cfg.getParameter<bool>("oppositeCharge");
    cuts_.targetMass = cfg.getParameter<double>("targetMass");
    cuts_.pairPtMin = cfg.getParameter<double>("pairPtMin");
  }

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::InputTag>("src", edm::InputTag("tightMuons"));
    desc.add<std::vector<edm::InputTag>>("vetoSrcs", {});
    desc.add<double>("leadPtMin", 26.0);
    desc.add<double>("subleadPtMin", 15.0);
    desc.add<double>("massMin", 71.0);
    desc.add<double>("massMax", 111.0);
    desc.add<bool>("oppositeCharge", true);
    desc.add<double>("targetMass", 91.1876);
    desc.add<double>("pairPtMin", 0.0)->setComment("applied to the chosen pair, after the choice");
    descriptions.addWithDefaultLabel(desc);
  }

  void produce(edm::StreamID, edm::Event& event, const edm::EventSetup&) const override {
    const auto& leps = event.get(src_);
    const auto pair = evsel::selectDilepton(evsel::toObjs(leps), cuts_);
    std::vector<reco::LeafCandidate> boson, out;
    std::vector<edm::Ptr<reco::Candidate>> ptrs;
    bool pass = false;
    if (pair) {
      const auto& a = leps[pair->first];
      const auto& b = leps[pair->second];
      std::size_t extra = 0;
      for (const auto& t : vetoes_)
        extra += evsel::countExtra(evsel::toObjs(event.get(t)), {evsel::toObj(a), evsel::toObj(b)});
      pass = extra == 0;
      if (pass) {
        out = {evsel::copy(a), evsel::copy(b)};
        ptrs = {leps.ptrAt(pair->first), leps.ptrAt(pair->second)};
        boson.emplace_back(a.charge() + b.charge(), a.p4() + b.p4());
      }
    }
    event.emplace(flag_, pass);
    event.emplace(boson_, std::move(boson));
    event.emplace(leptons_, std::move(out));
    event.emplace(leptonPtrs_, std::move(ptrs));
  }

private:
  const edm::EDGetTokenT<edm::View<reco::Candidate>> src_;
  const std::vector<edm::EDGetTokenT<edm::View<reco::Candidate>>> vetoes_;
  const edm::EDPutTokenT<bool> flag_;
  const edm::EDPutTokenT<std::vector<reco::LeafCandidate>> boson_, leptons_;
  const edm::EDPutTokenT<std::vector<edm::Ptr<reco::Candidate>>> leptonPtrs_;
  evsel::PairCuts cuts_;
};

DEFINE_FWK_MODULE(DileptonSelector);
