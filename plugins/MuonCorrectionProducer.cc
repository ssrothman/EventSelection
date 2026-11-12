#include "DataFormats/PatCandidates/interface/Muon.h"
#include "DataFormats/TrackReco/interface/Track.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/global/EDProducer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "SRothman/EventSelection/interface/MuonScaRe.h"

#include <memory>

// Copies of the muons with the MUO POG Run-3 scale (data and MC) and resolution (MC) corrections applied to pt, in the
// order of jsonpog-integration/examples/muoScaleAndSmearingRDFExample.py, for lowPtThreshold <= pt <= highPtThreshold.
// variation (MC only): nominal, scaleUp, scaleDown, resolutionUp, resolutionDown; resolutionVariationK picks the
// definition of k in the resolution variation (cpp or python reference; to be clarified with the POG).
// Tracker layers as NanoAOD's Muon_nTrackerLayers (0 without inner track).
class MuonCorrectionProducer : public edm::global::EDProducer<> {
public:
  explicit MuonCorrectionProducer(const edm::ParameterSet& cfg)
      : src_(consumes(cfg.getParameter<edm::InputTag>("src"))),
        put_(produces()),
        isData_(cfg.getParameter<bool>("isData")),
        lowPt_(cfg.getParameter<double>("lowPtThreshold")),
        highPt_(cfg.getParameter<double>("highPtThreshold")),
        pythonK_(parseK(cfg.getParameter<std::string>("resolutionVariationK"))),
        variation_(parseVariation(cfg.getParameter<std::string>("variation"))),
        scare_(std::make_unique<evsel::MuonScaRe>(cfg.getParameter<std::string>("correctionFile"))) {
    if (isData_ && variation_ != Variation::Nominal)
      throw cms::Exception("Configuration") << "MuonCorrectionProducer: variations are for MC only";
  }

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::InputTag>("src", edm::InputTag("slimmedMuons"));
    desc.add<std::string>("correctionFile",
                          "/cvmfs/cms-griddata.cern.ch/cat/metadata/MUO/Run3-22EFGSep23-Summer22EE-NanoAODv12/"
                          "latest/muon_scalesmearing.json.gz");
    desc.add<bool>("isData", false);
    desc.add<double>("lowPtThreshold", 26.0)->setComment("muons below this pt are not corrected (POG default)");
    desc.add<double>("highPtThreshold", 200.0)->setComment("muons above this pt are not corrected (stated validity)");
    desc.add<std::string>("resolutionVariationK", "cpp")
        ->setComment("k in the resolution variation: cpp (sqrt(k_data^2-k_mc^2)) or python (k_mc)");
    desc.add<std::string>("variation", "nominal")
        ->setComment("nominal, scaleUp, scaleDown, resolutionUp or resolutionDown");
    descriptions.addWithDefaultLabel(desc);
  }

  void produce(edm::StreamID, edm::Event& event, const edm::EventSetup&) const override {
    const auto evt = static_cast<std::uint32_t>(event.id().event());
    const auto lumi = static_cast<std::uint32_t>(event.id().luminosityBlock());
    std::vector<pat::Muon> out;
    for (const auto& mu : event.get(src_)) {
      out.push_back(mu);
      const double pt = mu.pt(), eta = mu.eta(), phi = mu.phi();
      const int q = mu.charge();
      const float nL = mu.innerTrack().isNonnull() ? mu.innerTrack()->hitPattern().trackerLayersWithMeasurement() : 0;
      const double ptScale = scare_->scale(isData_, pt, eta, phi, q, lowPt_, highPt_);
      double ptc = isData_ ? ptScale : scare_->resolution(ptScale, eta, phi, nL, evt, lumi, lowPt_, highPt_);
      switch (variation_) {
        case Variation::Nominal:
          break;
        case Variation::ScaleUp:
          ptc = scare_->scaleVariation(ptc, eta, phi, q, +1);
          break;
        case Variation::ScaleDown:
          ptc = scare_->scaleVariation(ptc, eta, phi, q, -1);
          break;
        case Variation::ResolutionUp:
          ptc = scare_->resolutionVariation(ptScale, ptc, eta, +1, pythonK_);
          break;
        case Variation::ResolutionDown:
          ptc = scare_->resolutionVariation(ptScale, ptc, eta, -1, pythonK_);
          break;
      }
      out.back().setP4(reco::Candidate::PolarLorentzVector(ptc, eta, phi, mu.mass()));
    }
    event.emplace(put_, std::move(out));
  }

private:
  enum class Variation { Nominal, ScaleUp, ScaleDown, ResolutionUp, ResolutionDown };
  static bool parseK(const std::string& k) {
    if (k == "cpp")
      return false;
    if (k == "python")
      return true;
    throw cms::Exception("Configuration") << "MuonCorrectionProducer: resolutionVariationK must be cpp or python";
  }
  static Variation parseVariation(const std::string& v) {
    if (v == "nominal")
      return Variation::Nominal;
    if (v == "scaleUp")
      return Variation::ScaleUp;
    if (v == "scaleDown")
      return Variation::ScaleDown;
    if (v == "resolutionUp")
      return Variation::ResolutionUp;
    if (v == "resolutionDown")
      return Variation::ResolutionDown;
    throw cms::Exception("Configuration") << "MuonCorrectionProducer: unknown variation " << v;
  }

  const edm::EDGetTokenT<edm::View<pat::Muon>> src_;
  const edm::EDPutTokenT<std::vector<pat::Muon>> put_;
  const bool isData_;
  const double lowPt_, highPt_;
  const bool pythonK_;
  const Variation variation_;
  // Shared by all streams; correctionlib's evaluate() is const and its interface declares no mutable state.
  const std::unique_ptr<const evsel::MuonScaRe> scare_;
};

DEFINE_FWK_MODULE(MuonCorrectionProducer);
