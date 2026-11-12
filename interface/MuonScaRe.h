#ifndef SRothman_EventSelection_MuonScaRe_h
#define SRothman_EventSelection_MuonScaRe_h

#include "correction.h"

#include <cstdint>
#include <string>
#include <vector>

// Muon momentum scale and resolution corrections of the MUO POG for Run 3 (muon_scalesmearing.json, Z-peak method,
// CMS-DP-2024-065). Follows jsonpog-integration/examples/MuonScaRe.cc function by function.
namespace evsel {

  class MuonScaRe {
  public:
    explicit MuonScaRe(const std::string& correctionFile);

    // Scale-corrected pt; unchanged outside [lowPtThreshold, highPtThreshold]. The C++ reference has no upper bound,
    // the Python reference and the stated validity (pt < 200 GeV) have one.
    double scale(bool isData, double pt, double eta, double phi, int charge, double lowPtThreshold,
                 double highPtThreshold) const;
    // MC resolution smearing of the scale-corrected pt; unchanged outside [lowPtThreshold, highPtThreshold] or if the
    // result is unphysical. The random number depends only on the event number (the reference's seed sequence of
    // length 1 uses only its first input).
    double resolution(double pt, double eta, double phi, float nTrackerLayers, std::uint32_t event, std::uint32_t lumi,
                      double lowPtThreshold, double highPtThreshold) const;
    // Statistical scale variation of the corrected pt; up = +1, down = -1.
    double scaleVariation(double pt, double eta, double phi, int charge, int updown) const;
    // Resolution variation from the scale-only (ptNoResol) and the smeared (ptResol) pt; up = +1, down = -1.
    // TODO(clarify with MUO POG): the references disagree on k. MuonScaRe.cc uses the residual smearing factor
    // sqrt(k_data^2 - k_mc^2) (pythonK false), MuonScaRe.py uses k_mc (pythonK true).
    double resolutionVariation(double ptNoResol, double ptResol, double eta, int updown, bool pythonK) const;

    // Pieces exposed for tests.
    double randomCB(double eta, double phi, float nL, std::uint32_t event, std::uint32_t lumi) const;
    double kFactor(double eta, const std::string& var) const;
    static std::uint32_t seed(std::uint32_t event, std::uint32_t lumi, double phi);

  private:
    double sigma(double pt, double eta, float nL) const;
    correction::Correction::Ref cb_, poly_, kData_, kMc_, aData_, aMc_, mData_, mMc_;
  };

  // Crystal Ball distribution with the inverse CDF used for the smearing (as in MuonScaRe.cc).
  struct CrystalBall {
    CrystalBall(double mean, double sigma, double alpha, double n);
    double cdf(double x) const;
    double invcdf(double u) const;
    double m, s, a, n;
    double B, C, D, N, NA, Ns, NC, F, G, k, cdfMa, cdfPa;
  };

}  // namespace evsel

#endif
