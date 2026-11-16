#include "DataFormats/Candidate/interface/Candidate.h"
#include "DataFormats/Candidate/interface/LeafCandidate.h"
#include "DataFormats/Common/interface/View.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/global/EDProducer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/Exception.h"

#include <vector>

// W candidate: lepton + neutrino, with the lepton's charge. `leptons` and `neutrino` hold the same number (0 or 1) of
// candidates.
class LeptonNeutrinoBosonProducer : public edm::global::EDProducer<> {
public:
  explicit LeptonNeutrinoBosonProducer(const edm::ParameterSet& cfg)
      : leptons_(consumes(cfg.getParameter<edm::InputTag>("leptons"))),
        neutrino_(consumes(cfg.getParameter<edm::InputTag>("neutrino"))),
        put_(produces()) {}

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::InputTag>("leptons");
    desc.add<edm::InputTag>("neutrino");
    descriptions.addWithDefaultLabel(desc);
  }

  void produce(edm::StreamID, edm::Event& event, const edm::EventSetup&) const override {
    const auto& leps = event.get(leptons_);
    const auto& nus = event.get(neutrino_);
    if (leps.size() != nus.size() || leps.size() > 1)
      throw cms::Exception("LeptonNeutrinoBosonProducer") << leps.size() << " leptons and " << nus.size() << " neutrinos";
    std::vector<reco::LeafCandidate> out;
    if (leps.size() == 1)
      out.emplace_back(leps[0].charge(), leps[0].p4() + nus[0].p4());
    event.emplace(put_, std::move(out));
  }

private:
  const edm::EDGetTokenT<edm::View<reco::Candidate>> leptons_, neutrino_;
  const edm::EDPutTokenT<std::vector<reco::LeafCandidate>> put_;
};

DEFINE_FWK_MODULE(LeptonNeutrinoBosonProducer);
