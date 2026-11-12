#include "SRothman/EventSelection/interface/MuonScaRe.h"

#include "TRandom3.h"

#include <boost/math/special_functions/erf.hpp>

#include <cmath>
#include <stdexcept>

namespace evsel {

  namespace {
    // Same mixing as SeedSequence in MuonScaRe.cc, generating a single 32-bit seed.
    std::uint32_t seedSequence(const std::vector<std::uint32_t>& seeds) {
      const std::size_t n = 1;
      const std::uint32_t mult = 0x9e3779b9, mixConst = 0x85ebca6b;
      std::vector<std::uint32_t> buffer(n, 0x8b8b8b8b);
      std::size_t i = 0;
      for (; i < std::min(n, seeds.size()); ++i)
        buffer[i] = buffer[i] ^ (seeds[i] + mult * static_cast<std::uint32_t>(i));
      for (; i < n; ++i)
        buffer[i] = buffer[i] ^ (mult * static_cast<std::uint32_t>(i));
      for (std::size_t k = 0; k < n; ++k) {
        const std::uint32_t z = buffer[(k + n - 1) % n] ^ (buffer[k] >> 27);
        buffer[k] = (z * mixConst) ^ (buffer[k] << 13);
      }
      return buffer[0];
    }
    // The reference uses this truncated value of pi in the Crystal Ball normalisation.
    constexpr double kPiCB = 3.14159;
  }  // namespace

  CrystalBall::CrystalBall(double mean, double sigma, double alpha, double nn) : m(mean), s(sigma), a(alpha), n(nn) {
    const double sqrtPiOver2 = std::sqrt(kPiCB / 2.0), sqrt2 = std::sqrt(2.0);
    const double fa = std::fabs(a);
    const double ex = std::exp(-fa * fa / 2);
    const double A = std::pow(n / fa, n) * ex;
    const double C1 = n / fa / (n - 1) * ex;
    const double D1 = 2 * sqrtPiOver2 * std::erf(fa / sqrt2);
    B = n / fa - fa;
    C = (D1 + 2 * C1) / C1;
    D = (D1 + 2 * C1) / 2;
    N = 1.0 / s / (D1 + 2 * C1);
    k = 1.0 / (n - 1);
    NA = N * A;
    Ns = N * s;
    NC = Ns * C1;
    F = 1 - fa * fa / n;
    G = s * n / fa;
    cdfMa = cdf(m - a * s);
    cdfPa = cdf(m + a * s);
  }

  double CrystalBall::cdf(double x) const {
    const double d = (x - m) / s;
    if (d < -a)
      return NC / std::pow(F - s * d / G, n - 1);
    if (d > a)
      return NC * (C - std::pow(F + s * d / G, 1 - n));
    return Ns * (D - std::sqrt(kPiCB / 2.0) * std::erf(-d / std::sqrt(2.0)));
  }

  double CrystalBall::invcdf(double u) const {
    if (u < cdfMa)
      return m + G * (F - std::pow(NC / u, k));
    if (u > cdfPa)
      return m - G * (F - std::pow(C - u / NC, -k));
    return m - std::sqrt(2.0) * s * boost::math::erf_inv((D - u / Ns) / std::sqrt(kPiCB / 2.0));
  }

  MuonScaRe::MuonScaRe(const std::string& file) {
    const auto cset = correction::CorrectionSet::from_file(file);
    cb_ = cset->at("cb_params");
    poly_ = cset->at("poly_params");
    kData_ = cset->at("k_data");
    kMc_ = cset->at("k_mc");
    aData_ = cset->at("a_data");
    aMc_ = cset->at("a_mc");
    mData_ = cset->at("m_data");
    mMc_ = cset->at("m_mc");
  }

  std::uint32_t MuonScaRe::seed(std::uint32_t event, std::uint32_t lumi, double phi) {
    const std::int64_t phiSeed = static_cast<std::int64_t>((phi / M_PI) * ((1LL << 31) - 1)) & 0xFFF;
    return seedSequence({event, lumi, static_cast<std::uint32_t>(phiSeed)});
  }

  double MuonScaRe::randomCB(double eta, double phi, float nL, std::uint32_t event, std::uint32_t lumi) const {
    const double ae = std::abs(eta);
    const CrystalBall cb(cb_->evaluate({ae, nL, 0}), cb_->evaluate({ae, nL, 1}), cb_->evaluate({ae, nL, 3}),
                         cb_->evaluate({ae, nL, 2}));
    TRandom3 rnd(seed(event, lumi, phi));
    return cb.invcdf(rnd.Rndm());
  }

  double MuonScaRe::sigma(double pt, double eta, float nL) const {
    const double ae = std::abs(eta);
    const double s = poly_->evaluate({ae, nL, 0}) + poly_->evaluate({ae, nL, 1}) * pt + poly_->evaluate({ae, nL, 2}) * pt * pt;
    return s < 0 ? 0 : s;
  }

  double MuonScaRe::kFactor(double eta, const std::string& var) const {
    const double kd = kData_->evaluate({std::abs(eta), var});
    const double km = kMc_->evaluate({std::abs(eta), var});
    return km < kd ? std::sqrt(kd * kd - km * km) : 0.0;
  }

  double MuonScaRe::resolution(double pt, double eta, double phi, float nL, std::uint32_t event, std::uint32_t lumi,
                               double lowPtThreshold, double highPtThreshold) const {
    double ptc = pt * (1 + kFactor(eta, "nom") * sigma(pt, eta, nL) * randomCB(eta, phi, nL, event, lumi));
    if (std::isnan(ptc))
      ptc = pt;
    if (ptc / pt > 2 || ptc / pt < 0.1 || ptc < 0 || pt < lowPtThreshold || pt > highPtThreshold)
      ptc = pt;
    return ptc;
  }

  double MuonScaRe::scale(bool isData, double pt, double eta, double phi, int charge, double lowPtThreshold,
                          double highPtThreshold) const {
    const double a = (isData ? aData_ : aMc_)->evaluate({eta, phi, "nom"});
    const double m = (isData ? mData_ : mMc_)->evaluate({eta, phi, "nom"});
    if (pt < lowPtThreshold || pt > highPtThreshold)
      return pt;
    return 1. / (m / pt + charge * a);
  }

  double MuonScaRe::scaleVariation(double pt, double eta, double phi, int charge, int updown) const {
    const double sa = aMc_->evaluate({eta, phi, "stat"});
    const double sm = mMc_->evaluate({eta, phi, "stat"});
    const double rho = mMc_->evaluate({eta, phi, "rho_stat"});
    const double unc = pt * pt * std::sqrt(sm * sm / (pt * pt) + sa * sa + 2 * charge * rho * sm / pt * sa);
    return pt + updown * unc;
  }

  double MuonScaRe::resolutionVariation(double ptNoResol, double ptResol, double eta, int updown, bool pythonK) const {
    const double k = pythonK ? kMc_->evaluate({std::abs(eta), "nom"}) : kFactor(eta, "nom");
    if (!(k > 0))
      return ptResol;
    const double kUnc = kMc_->evaluate({std::abs(eta), "stat"});
    const double stdTimesRndm = (ptResol / ptNoResol - 1) / k;
    double v = ptNoResol * (1 + (k + updown * kUnc) * stdTimesRndm);
    if (v / ptNoResol > 2 || v / ptNoResol < 0.1 || v < 0)
      v = ptNoResol;
    return v;
  }

}  // namespace evsel
