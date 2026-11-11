#include "DataFormats/VertexReco/interface/Vertex.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/global/EDProducer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"

#include <cmath>

// bool = the first vertex is good: not fake, ndof > ndofMin, |z| < absZMax, rho < rhoMax.
class VertexFlagProducer : public edm::global::EDProducer<> {
public:
  explicit VertexFlagProducer(const edm::ParameterSet& cfg)
      : src_(consumes(cfg.getParameter<edm::InputTag>("src"))),
        put_(produces()),
        ndofMin_(cfg.getParameter<double>("ndofMin")),
        zMax_(cfg.getParameter<double>("absZMax")),
        rhoMax_(cfg.getParameter<double>("rhoMax")) {}

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::InputTag>("src", edm::InputTag("offlineSlimmedPrimaryVertices"));
    desc.add<double>("ndofMin", 4.0);
    desc.add<double>("absZMax", 24.0);
    desc.add<double>("rhoMax", 2.0);
    descriptions.addWithDefaultLabel(desc);
  }

  void produce(edm::StreamID, edm::Event& event, const edm::EventSetup&) const override {
    const auto& v = event.get(src_);
    const bool good = !v.empty() && !v[0].isFake() && v[0].ndof() > ndofMin_ && std::abs(v[0].z()) < zMax_ &&
                      v[0].position().rho() < rhoMax_;
    event.emplace(put_, good);
  }

private:
  const edm::EDGetTokenT<std::vector<reco::Vertex>> src_;
  const edm::EDPutTokenT<bool> put_;
  const double ndofMin_, zMax_, rhoMax_;
};

DEFINE_FWK_MODULE(VertexFlagProducer);
