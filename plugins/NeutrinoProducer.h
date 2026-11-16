#ifndef SRothman_EventSelection_NeutrinoProducer_h
#define SRothman_EventSelection_NeutrinoProducer_h

#include "DataFormats/Candidate/interface/Candidate.h"
#include "DataFormats/Candidate/interface/LeafCandidate.h"
#include "DataFormats/Common/interface/View.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/global/EDProducer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "SRothman/EventSelection/interface/Neutrino.h"

#include <typeinfo>
#include <vector>

// Module contract of every neutrino reconstruction strategy: inputs `leptons` (empty or exactly one candidate) and
// `met` (View of one candidate); output vector<LeafCandidate>, one massless neutrino per lepton with status() = the
// strategy's code. A strategy S provides S(const ParameterSet&), S::fillDescriptions(ParameterSetDescription&) for its
// own parameters, and evsel::NeutrinoResult operator()(const reco::Candidate& lepton, const reco::Candidate& met) const.
template <class S>
class NeutrinoProducer : public edm::global::EDProducer<> {
public:
  explicit NeutrinoProducer(const edm::ParameterSet& cfg)
      : leptons_(consumes(cfg.getParameter<edm::InputTag>("leptons"))),
        met_(consumes(cfg.getParameter<edm::InputTag>("met"))),
        put_(produces()),
        strategy_(cfg) {}

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::InputTag>("leptons");
    desc.add<edm::InputTag>("met");
    S::fillDescriptions(desc);
    descriptions.addWithDefaultLabel(desc);
  }

  void produce(edm::StreamID, edm::Event& event, const edm::EventSetup&) const override {
    std::vector<reco::LeafCandidate> out;
    const auto& leps = event.get(leptons_);
    if (leps.size() > 1)
      throw cms::Exception("NeutrinoProducer") << typeid(S).name() << ": " << leps.size() << " leptons, expected at most 1";
    if (leps.size() == 1) {
      const auto r = strategy_(leps[0], event.get(met_).at(0));
      reco::LeafCandidate nu(0, reco::LeafCandidate::LorentzVector(r.px, r.py, r.pz, r.e));
      nu.setStatus(r.status);
      out.push_back(nu);
    }
    event.emplace(put_, std::move(out));
  }

private:
  const edm::EDGetTokenT<edm::View<reco::Candidate>> leptons_, met_;
  const edm::EDPutTokenT<std::vector<reco::LeafCandidate>> put_;
  const S strategy_;
};

#endif
