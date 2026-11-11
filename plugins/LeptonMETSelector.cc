#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/global/EDProducer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/transform.h"
#include "SRothman/EventSelection/plugins/Common.h"

// W -> l nu: exactly one lepton in src with pt > ptMin, no other lepton in vetoSrcs (dR > 0.01 from it),
// MET > metMin and mT(lepton, MET) > mtMin. Products: bool; "boson" (lepton + MET four-vector, MET has pz = 0);
// "leptons" (the lepton).
class LeptonMETSelector : public edm::global::EDProducer<> {
public:
  explicit LeptonMETSelector(const edm::ParameterSet& cfg)
      : src_(consumes(cfg.getParameter<edm::InputTag>("src"))),
        met_(consumes(cfg.getParameter<edm::InputTag>("met"))),
        vetoes_(edm::vector_transform(cfg.getParameter<std::vector<edm::InputTag>>("vetoSrcs"),
                                      [this](const edm::InputTag& t) { return consumes<edm::View<reco::Candidate>>(t); })),
        flag_(produces()),
        boson_(produces("boson")),
        leptons_(produces("leptons")),
        ptMin_(cfg.getParameter<double>("ptMin")),
        metMin_(cfg.getParameter<double>("metMin")),
        mtMin_(cfg.getParameter<double>("mtMin")) {}

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::InputTag>("src", edm::InputTag("tightMuons"));
    desc.add<edm::InputTag>("met", edm::InputTag("slimmedMETsPuppi"));
    desc.add<std::vector<edm::InputTag>>("vetoSrcs", {});
    desc.add<double>("ptMin", 30.0);
    desc.add<double>("metMin", 40.0);
    desc.add<double>("mtMin", 50.0);
    descriptions.addWithDefaultLabel(desc);
  }

  void produce(edm::StreamID, edm::Event& event, const edm::EventSetup&) const override {
    std::vector<reco::LeafCandidate> boson, out;
    bool pass = false;
    const auto& leps = event.get(src_);
    std::vector<const reco::Candidate*> sel;
    for (const auto& l : leps)
      if (l.pt() > ptMin_)
        sel.push_back(&l);
    if (sel.size() == 1) {
      const auto& l = *sel[0];
      std::size_t extra = 0;
      for (const auto& t : vetoes_)
        extra += evsel::countExtra(evsel::toObjs(event.get(t)), {evsel::toObj(l)});
      const auto& met = event.get(met_).at(0);
      const double mt = evsel::transverseMass(l.pt(), l.phi(), met.pt(), met.phi());
      pass = extra == 0 && met.pt() > metMin_ && mt > mtMin_;
      if (pass) {
        out.push_back(evsel::copy(l));
        boson.emplace_back(l.charge(), l.p4() + reco::Candidate::LorentzVector(met.px(), met.py(), 0, met.pt()));
      }
    }
    event.emplace(flag_, pass);
    event.emplace(boson_, std::move(boson));
    event.emplace(leptons_, std::move(out));
  }

private:
  const edm::EDGetTokenT<edm::View<reco::Candidate>> src_, met_;
  const std::vector<edm::EDGetTokenT<edm::View<reco::Candidate>>> vetoes_;
  const edm::EDPutTokenT<bool> flag_;
  const edm::EDPutTokenT<std::vector<reco::LeafCandidate>> boson_, leptons_;
  const double ptMin_, metMin_, mtMin_;
};

DEFINE_FWK_MODULE(LeptonMETSelector);
