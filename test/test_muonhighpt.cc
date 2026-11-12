#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "SRothman/EventSelection/interface/MuonHighPt.h"

#include <cmath>

using evsel::MuonHighPt;

namespace {
  constexpr double kDeg = M_PI / 180.0;
  evsel::HighPtParams indexParams() {
    evsel::HighPtParams p;
    for (int i = 0; i < 18; ++i) {
      p.kappa[i] = i;
      p.kappaUnc[i] = 100 + i;
    }
    p.sigmaBarrel = {{0.01, 6e-5, -3e-8, 5e-12}};
    p.sigmaEndcap = {{0.014, 5e-5, -2e-8, 3e-12}};
    p.smearBarrel = 0.0;
    p.smearEndcap = 0.46;
    return p;
  }
}  // namespace

TEST_CASE("eta and phi bins follow the wiki table edges", "[muonhighpt]") {
  CHECK(MuonHighPt::etaBin(-2.4) == 0);
  CHECK(MuonHighPt::etaBin(-2.1) == 1);
  CHECK(MuonHighPt::etaBin(-1.2001) == 1);
  CHECK(MuonHighPt::etaBin(-1e-9) == 2);
  CHECK(MuonHighPt::etaBin(0.0) == 3);
  CHECK(MuonHighPt::etaBin(1.2) == 4);
  CHECK(MuonHighPt::etaBin(2.39) == 5);
  CHECK(MuonHighPt::etaBin(2.4) == -1);
  CHECK(MuonHighPt::etaBin(-2.41) == -1);
  CHECK(MuonHighPt::phiBin(-M_PI) == 0);
  CHECK(MuonHighPt::phiBin(-60.01 * kDeg) == 0);
  CHECK(MuonHighPt::phiBin(-M_PI / 3) == 1);
  CHECK(MuonHighPt::phiBin(59.99 * kDeg) == 1);
  CHECK(MuonHighPt::phiBin(M_PI / 3) == 2);
  CHECK(MuonHighPt::phiBin(M_PI) == 2);
}

TEST_CASE("kappa lookup is phi-major and zero outside the acceptance", "[muonhighpt]") {
  const MuonHighPt h(indexParams());
  CHECK(h.kappa(1.5, 0.0) == 1 * 6 + 4);
  CHECK(h.kappa(-2.2, 114 * kDeg) == 2 * 6 + 0);
  CHECK(h.kappa(0.5, -170 * kDeg) == 0 * 6 + 3);
  CHECK(h.kappaUnc(1.5, 0.0) == 100 + 10);
  CHECK(h.kappa(2.5, 0.0) == 0.0);
  CHECK(h.kappaUnc(-2.5, 0.0) == 0.0);
}

TEST_CASE("curvature shift q/pt -> q/pt + kappa/1000", "[muonhighpt]") {
  CHECK(MuonHighPt::shiftCurvature(500, +1, 0.1) == Approx(1.0 / (0.002 + 0.0001)));
  CHECK(MuonHighPt::shiftCurvature(500, -1, 0.1) == Approx(1.0 / (0.002 - 0.0001)));
  CHECK(MuonHighPt::shiftCurvature(500, +1, -0.1) == Approx(1.0 / (0.002 - 0.0001)));
  CHECK(MuonHighPt::shiftCurvature(500, +1, 0.0) == 500);
  // The charge would flip: unchanged.
  CHECK(MuonHighPt::shiftCurvature(20000, +1, -0.1) == 20000);
  CHECK(MuonHighPt::shiftCurvature(20000, -1, 0.1) == 20000);
}

TEST_CASE("resolution polynomial, smearing factor and unphysical smearing", "[muonhighpt]") {
  const MuonHighPt h(indexParams());
  const double p = 1000;
  CHECK(h.sigma(p, 0.5) == Approx(0.01 + 6e-5 * p - 3e-8 * p * p + 5e-12 * p * p * p));
  CHECK(h.sigma(p, -1.2) == Approx(0.014 + 5e-5 * p - 2e-8 * p * p + 3e-12 * p * p * p));
  CHECK(h.smearFactor(1.19) == 0.0);
  CHECK(h.smearFactor(-1.3) == 0.46);
  CHECK(h.smear(p, 1.5, 0.46, 1.0) == Approx(1 + h.sigma(p, 1.5) * 0.46));
  CHECK(h.smear(p, 1.5, 0.46, -2.0) == Approx(1 - 2 * h.sigma(p, 1.5) * 0.46));
  CHECK(h.smear(p, 1.5, 0.46, -1e6) == 1.0);
}

TEST_CASE("seeded gaussian is reproducible, key-dependent and standard normal", "[muonhighpt]") {
  CHECK(evsel::seededGauss(1, 2, 3, 0, 7) == evsel::seededGauss(1, 2, 3, 0, 7));
  CHECK(evsel::seededGauss(1, 2, 3, 0, 7) != evsel::seededGauss(1, 2, 3, 1, 7));
  CHECK(evsel::seededGauss(1, 2, 3, 0, 7) != evsel::seededGauss(1, 2, 3, 0, 8));
  CHECK(evsel::seededGauss(1, 2, 3, 0, 7) != evsel::seededGauss(1, 2, 4, 0, 7));
  CHECK(evsel::seededGauss(1, 2, 3, 0, 7) != evsel::seededGauss(1, 3, 3, 0, 7));
  CHECK(evsel::seededGauss(1, 2, 3, 0, 7) != evsel::seededGauss(2, 2, 3, 0, 7));
  const int n = 40000;
  double s = 0, s2 = 0;
  for (int i = 0; i < n; ++i) {
    const double g = evsel::seededGauss(1, 1, i, 0, 7);
    s += g;
    s2 += g * g;
  }
  const double mean = s / n;
  CHECK(std::abs(mean) < 0.02);  // 4 sigma of the mean
  CHECK(std::sqrt(s2 / n - mean * mean) == Approx(1.0).margin(0.02));
}
