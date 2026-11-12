#ifndef SRothman_EventSelection_MuonHighPt_h
#define SRothman_EventSelection_MuonHighPt_h

#include <array>
#include <cstdint>

// MUO POG Run-3 high-pT muon momentum scale (Generalized Endpoint kappa_b) and MC resolution smearing, as tabulated
// on the MUO POG wiki (guidelines/corrections.md, "High pT: Momentum Scale" and "High pT: Momentum Resolution").
namespace evsel {

  struct HighPtParams {
    // kappa_b and its uncertainty [c/TeV], index phiBin * 6 + etaBin (see MuonHighPt::etaBin, phiBin).
    std::array<double, 18> kappa{}, kappaUnc{};
    // sigma(p) = c0 + c1 p + c2 p^2 + c3 p^3 (p in GeV), for |eta| < 1.2 and 1.2 <= |eta| < 2.4.
    std::array<double, 4> sigmaBarrel{}, sigmaEndcap{};
    // Nominal MC smearing factor f in p -> p (1 + N(0, sigma f)); 0 means no smearing.
    double smearBarrel = 0, smearEndcap = 0;
  };

  class MuonHighPt {
  public:
    explicit MuonHighPt(const HighPtParams& p) : p_(p) {}

    // Factor of the resolution systematic: an extra smearing on top of the nominal one (the wiki's "10% shift").
    static constexpr double kSystSmearFactor = 0.46;

    // Bins [-2.4,-2.1,-1.2,0,1.2,2.1,2.4) in eta and [-180,-60,60,180] degrees in phi; -1 outside |eta| < 2.4.
    static int etaBin(double eta);
    static int phiBin(double phi);
    // 0 outside |eta| < 2.4.
    double kappa(double eta, double phi) const;
    double kappaUnc(double eta, double phi) const;

    // pt after q/pt -> q/pt + kappa / 1000 (kappa in c/TeV, pt in GeV); unchanged if the charge would flip.
    static double shiftCurvature(double pt, int charge, double kappa);

    double sigma(double p, double eta) const;
    double smearFactor(double eta) const;
    // 1 + gauss * sigma(p, eta) * factor, or 1 if that is not positive.
    double smear(double p, double eta, double factor, double gauss) const;

  private:
    HighPtParams p_;
  };

  // Standard-normal number for (run, lumi, event, muon index, salt); independent of the module and the stream.
  double seededGauss(std::uint64_t run, std::uint64_t lumi, std::uint64_t event, std::uint64_t index, std::uint64_t salt);

}  // namespace evsel

#endif
