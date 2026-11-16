#include "FWCore/Framework/interface/MakerMacros.h"
#include "SRothman/EventSelection/plugins/NeutrinoProducer.h"

// m(lepton, neutrino) = wMass with pT(neutrino) = MET (evsel::wMassNeutrino). status: 1 two real roots (smaller |p_z|),
// 2 one real root, 3 complex roots (real part).
namespace {
  struct WMassStrategy {
    explicit WMassStrategy(const edm::ParameterSet& cfg) : wMass_(cfg.getParameter<double>("wMass")) {}
    static void fillDescriptions(edm::ParameterSetDescription& desc) {
      desc.add<double>("wMass")->setComment("W mass (GeV) of the constraint");
    }
    evsel::NeutrinoResult operator()(const reco::Candidate& lep, const reco::Candidate& met) const {
      return evsel::wMassNeutrino(lep.pt(), lep.eta(), lep.phi(), met.pt(), met.phi(), wMass_);
    }
    const double wMass_;
  };
}  // namespace

using WMassNeutrinoProducer = NeutrinoProducer<WMassStrategy>;
DEFINE_FWK_MODULE(WMassNeutrinoProducer);
