#include "FWCore/Framework/interface/MakerMacros.h"
#include "SRothman/EventSelection/plugins/NeutrinoProducer.h"

// Neutrino with the MET transverse momentum and p_z = 0 (status 0): the baseline strategy.
namespace {
  struct ZeroPzStrategy {
    explicit ZeroPzStrategy(const edm::ParameterSet&) {}
    static void fillDescriptions(edm::ParameterSetDescription&) {}
    evsel::NeutrinoResult operator()(const reco::Candidate&, const reco::Candidate& met) const {
      return {met.px(), met.py(), 0.0, met.pt(), 0};
    }
  };
}  // namespace

using ZeroPzNeutrinoProducer = NeutrinoProducer<ZeroPzStrategy>;
DEFINE_FWK_MODULE(ZeroPzNeutrinoProducer);
