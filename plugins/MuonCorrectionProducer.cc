#include "DataFormats/PatCandidates/interface/Muon.h"
#include "DataFormats/TrackReco/interface/Track.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/global/EDProducer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "SRothman/EventSelection/interface/MuonHighPt.h"
#include "SRothman/EventSelection/interface/MuonScaRe.h"

#include <algorithm>
#include <cmath>
#include <memory>

// Copies of the muons with the MUO POG Run-3 momentum assignment and corrections. Each muon gets userInt
// "highPtRegime": 1 if its momentum is the TuneP one (momentum tuneP, or hybrid with PF pt >= regimeThreshold), else 0,
// and userFloat "originalPt": the input muon's pt, before the momentum assignment and corrections.
// With applyCorrections:
//  - regime 0: MuonScaRe scale (data and MC) and resolution (MC) for lowPtThreshold <= pt <= highPtThreshold, in the
//    order of jsonpog-integration/examples/muoScaleAndSmearingRDFExample.py.
//  - regime 1, TuneP pt >= highPt.ptMin and |eta| < 2.4: q/pt -> q/pt + kappa_b/1000 on the kappaTarget sample
//    (mc: +kappa_b on MC; data: -kappa_b on data), then the nominal high-pT smearing in MC.
// variation (MC only) acts on its own regime: scaleUp/Down, resolutionUp/Down (regime 0);
// highPtScaleUp/Down (kappa_b uncertainty), highPtResolution (extra smearing, factor 0.46) (regime 1).
// Tracker layers as NanoAOD's Muon_nTrackerLayers (0 without inner track).
class MuonCorrectionProducer : public edm::global::EDProducer<> {
public:
  explicit MuonCorrectionProducer(const edm::ParameterSet& cfg)
      : src_(consumes(cfg.getParameter<edm::InputTag>("src"))),
        put_(produces()),
        isData_(cfg.getParameter<bool>("isData")),
        momentum_(parseMomentum(cfg.getParameter<std::string>("momentum"))),
        regimeThreshold_(cfg.getParameter<double>("regimeThreshold")),
        apply_(cfg.getParameter<bool>("applyCorrections")),
        lowPt_(cfg.getParameter<double>("lowPtThreshold")),
        highPt_(cfg.getParameter<double>("highPtThreshold")),
        pythonK_(parseK(cfg.getParameter<std::string>("resolutionVariationK"))),
        variation_(parseVariation(cfg.getParameter<std::string>("variation"))),
        scare_(apply_ ? std::make_unique<evsel::MuonScaRe>(cfg.getParameter<std::string>("correctionFile")) : nullptr),
        highPtCorr_(highPtParams(cfg.getParameter<edm::ParameterSet>("highPt"))),
        highPtMin_(cfg.getParameter<edm::ParameterSet>("highPt").getParameter<double>("ptMin")),
        kappaOnData_(parseTarget(cfg.getParameter<edm::ParameterSet>("highPt").getParameter<std::string>("kappaTarget"))) {
    if (isData_ && variation_ != Variation::Nominal)
      throw cms::Exception("Configuration") << "MuonCorrectionProducer: variations are for MC only";
    if (!apply_ && variation_ != Variation::Nominal)
      throw cms::Exception("Configuration") << "MuonCorrectionProducer: variations need applyCorrections";
  }

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::InputTag>("src", edm::InputTag("slimmedMuons"));
    desc.add<bool>("isData", false);
    desc.add<std::string>("momentum", "pf")->setComment("pf | tuneP | hybrid");
    desc.add<double>("regimeThreshold", 200.0)->setComment("hybrid: PF pt at or above which TuneP is used");
    desc.add<bool>("applyCorrections", true);
    desc.add<std::string>("correctionFile",
                          "/cvmfs/cms-griddata.cern.ch/cat/metadata/MUO/Run3-22EFGSep23-Summer22EE-NanoAODv12/"
                          "latest/muon_scalesmearing.json.gz");
    desc.add<double>("lowPtThreshold", 26.0)->setComment("muons below this pt are not corrected (POG default)");
    desc.add<double>("highPtThreshold", 200.0)->setComment("muons above this pt are not corrected (stated validity)");
    desc.add<std::string>("resolutionVariationK", "cpp")
        ->setComment("k in the resolution variation: cpp (sqrt(k_data^2-k_mc^2)) or python (k_mc)");
    desc.add<std::string>("variation", "nominal")
        ->setComment("nominal, scaleUp, scaleDown, resolutionUp, resolutionDown, highPtScaleUp, highPtScaleDown, "
                     "highPtResolution");
    edm::ParameterSetDescription hp;
    hp.add<std::vector<double>>("kappa", std::vector<double>(18, 0.0))->setComment("c/TeV, phiBin * 6 + etaBin");
    hp.add<std::vector<double>>("kappaUnc", std::vector<double>(18, 0.0));
    hp.add<std::vector<double>>("sigmaBarrel", std::vector<double>(4, 0.0));
    hp.add<std::vector<double>>("sigmaEndcap", std::vector<double>(4, 0.0));
    hp.add<double>("smearBarrel", 0.0);
    hp.add<double>("smearEndcap", 0.0);
    hp.add<double>("ptMin", 200.0)->setComment("regime-1 muons below this TuneP pt are not corrected");
    hp.add<std::string>("kappaTarget", "mc")->setComment("mc (+kappa_b on MC) | data (-kappa_b on data)");
    desc.add<edm::ParameterSetDescription>("highPt", hp);
    descriptions.addWithDefaultLabel(desc);
  }

  void produce(edm::StreamID, edm::Event& event, const edm::EventSetup&) const override {
    const auto evt = static_cast<std::uint32_t>(event.id().event());
    const auto lumi = static_cast<std::uint32_t>(event.id().luminosityBlock());
    std::vector<pat::Muon> out;
    const auto& muons = event.get(src_);
    for (std::size_t i = 0; i < muons.size(); ++i) {
      const auto& mu = muons[i];
      out.push_back(mu);
      const bool high = momentum_ == Momentum::TuneP || (momentum_ == Momentum::Hybrid && mu.pt() >= regimeThreshold_);
      out.back().addUserInt("highPtRegime", high);
      out.back().addUserFloat("originalPt", mu.pt());
      const double eta = mu.eta(), phi = mu.phi();
      double pt = high && mu.tunePMuonBestTrack().isNonnull() ? mu.tunePMuonBestTrack()->pt() : mu.pt();
      if (apply_)
        pt = high ? correctHigh(pt, eta, phi, mu.charge(), event.id(), i) : correctMedium(mu, pt, evt, lumi);
      out.back().setP4(reco::Candidate::PolarLorentzVector(pt, eta, phi, mu.mass()));
    }
    event.emplace(put_, std::move(out));
  }

private:
  enum class Momentum { PF, TuneP, Hybrid };
  enum class Variation {
    Nominal,
    ScaleUp,
    ScaleDown,
    ResolutionUp,
    ResolutionDown,
    HighPtScaleUp,
    HighPtScaleDown,
    HighPtResolution
  };
  // Salts of the two independent high-pT smearing draws.
  static constexpr std::uint64_t kSmearSalt = 0x4d55484e;
  static constexpr std::uint64_t kSystSalt = 0x4d554853;

  double correctMedium(const pat::Muon& mu, double pt, std::uint32_t evt, std::uint32_t lumi) const {
    const double eta = mu.eta(), phi = mu.phi();
    const int q = mu.charge();
    const float nL = mu.innerTrack().isNonnull() ? mu.innerTrack()->hitPattern().trackerLayersWithMeasurement() : 0;
    const double ptScale = scare_->scale(isData_, pt, eta, phi, q, lowPt_, highPt_);
    double ptc = isData_ ? ptScale : scare_->resolution(ptScale, eta, phi, nL, evt, lumi, lowPt_, highPt_);
    switch (variation_) {
      case Variation::ScaleUp:
        return scare_->scaleVariation(ptc, eta, phi, q, +1);
      case Variation::ScaleDown:
        return scare_->scaleVariation(ptc, eta, phi, q, -1);
      case Variation::ResolutionUp:
        return scare_->resolutionVariation(ptScale, ptc, eta, +1, pythonK_);
      case Variation::ResolutionDown:
        return scare_->resolutionVariation(ptScale, ptc, eta, -1, pythonK_);
      default:
        return ptc;
    }
  }

  double correctHigh(double pt, double eta, double phi, int q, const edm::EventID& id, std::size_t index) const {
    if (pt < highPtMin_ || evsel::MuonHighPt::etaBin(eta) < 0)
      return pt;
    double kappa = 0;
    if (isData_ == kappaOnData_)
      kappa = isData_ ? -highPtCorr_.kappa(eta, phi) : highPtCorr_.kappa(eta, phi);
    if (variation_ == Variation::HighPtScaleUp)
      kappa += highPtCorr_.kappaUnc(eta, phi);
    else if (variation_ == Variation::HighPtScaleDown)
      kappa -= highPtCorr_.kappaUnc(eta, phi);
    pt = evsel::MuonHighPt::shiftCurvature(pt, q, kappa);
    if (isData_)
      return pt;
    const auto gauss = [&](std::uint64_t salt) { return evsel::seededGauss(id.run(), id.luminosityBlock(), id.event(), index, salt); };
    const double f = highPtCorr_.smearFactor(eta);
    if (f > 0)
      pt *= highPtCorr_.smear(pt * std::cosh(eta), eta, f, gauss(kSmearSalt));
    if (variation_ == Variation::HighPtResolution)
      pt *= highPtCorr_.smear(pt * std::cosh(eta), eta, evsel::MuonHighPt::kSystSmearFactor, gauss(kSystSalt));
    return pt;
  }

  static evsel::MuonHighPt highPtParams(const edm::ParameterSet& ps) {
    evsel::HighPtParams p;
    auto fill = [&](const char* name, auto& arr) {
      const auto v = ps.getParameter<std::vector<double>>(name);
      if (v.size() != arr.size())
        throw cms::Exception("Configuration") << "MuonCorrectionProducer: highPt." << name << " needs " << arr.size()
                                              << " values, got " << v.size();
      std::copy(v.begin(), v.end(), arr.begin());
    };
    fill("kappa", p.kappa);
    fill("kappaUnc", p.kappaUnc);
    fill("sigmaBarrel", p.sigmaBarrel);
    fill("sigmaEndcap", p.sigmaEndcap);
    p.smearBarrel = ps.getParameter<double>("smearBarrel");
    p.smearEndcap = ps.getParameter<double>("smearEndcap");
    return evsel::MuonHighPt(p);
  }
  static Momentum parseMomentum(const std::string& m) {
    if (m == "pf")
      return Momentum::PF;
    if (m == "tuneP")
      return Momentum::TuneP;
    if (m == "hybrid")
      return Momentum::Hybrid;
    throw cms::Exception("Configuration") << "MuonCorrectionProducer: momentum must be pf, tuneP or hybrid";
  }
  static bool parseTarget(const std::string& t) {
    if (t == "mc")
      return false;
    if (t == "data")
      return true;
    throw cms::Exception("Configuration") << "MuonCorrectionProducer: highPt.kappaTarget must be mc or data";
  }
  static bool parseK(const std::string& k) {
    if (k == "cpp")
      return false;
    if (k == "python")
      return true;
    throw cms::Exception("Configuration") << "MuonCorrectionProducer: resolutionVariationK must be cpp or python";
  }
  static Variation parseVariation(const std::string& v) {
    static const std::vector<std::pair<std::string, Variation>> names{
        {"nominal", Variation::Nominal},
        {"scaleUp", Variation::ScaleUp},
        {"scaleDown", Variation::ScaleDown},
        {"resolutionUp", Variation::ResolutionUp},
        {"resolutionDown", Variation::ResolutionDown},
        {"highPtScaleUp", Variation::HighPtScaleUp},
        {"highPtScaleDown", Variation::HighPtScaleDown},
        {"highPtResolution", Variation::HighPtResolution}};
    for (const auto& [n, var] : names)
      if (n == v)
        return var;
    throw cms::Exception("Configuration") << "MuonCorrectionProducer: unknown variation " << v;
  }

  const edm::EDGetTokenT<edm::View<pat::Muon>> src_;
  const edm::EDPutTokenT<std::vector<pat::Muon>> put_;
  const bool isData_;
  const Momentum momentum_;
  const double regimeThreshold_;
  const bool apply_;
  const double lowPt_, highPt_;
  const bool pythonK_;
  const Variation variation_;
  // Shared by all streams; correctionlib's evaluate() is const and its interface declares no mutable state.
  const std::unique_ptr<const evsel::MuonScaRe> scare_;
  const evsel::MuonHighPt highPtCorr_;
  const double highPtMin_;
  const bool kappaOnData_;
};

DEFINE_FWK_MODULE(MuonCorrectionProducer);
