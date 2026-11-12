#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "SRothman/EventSelection/interface/MuonScaRe.h"

#include "correction.h"

#include <cmath>

using evsel::CrystalBall;
using evsel::MuonScaRe;

namespace {
  const char* kFile = "/cvmfs/cms-griddata.cern.ch/cat/metadata/MUO/Run3-22EFGSep23-Summer22EE-NanoAODv12/latest/muon_scalesmearing.json.gz";
}

TEST_CASE("seed matches the POG Python SeedSequence and ignores lumi and phi", "[muonscare]") {
  // Expected values from jsonpog-integration/examples/MuonScaRe.py: SeedSequence([ev, lumi, phi]).generate(1)[0].
  CHECK(MuonScaRe::seed(12345, 1, 0.3) == 4100016929u);
  CHECK(MuonScaRe::seed(12345, 999, -2.9) == 4100016929u);
  CHECK(MuonScaRe::seed(4294967295u, 3, 1.0) == 1344041726u);
  CHECK(MuonScaRe::seed(72023128, 27292, 0.5) == 1502102038u);
}

TEST_CASE("Crystal Ball inverse CDF inverts the CDF in the core and both tails", "[muonscare]") {
  const CrystalBall cb(0.1, 1.2, 1.5, 3.0);
  for (double u : {0.001, 0.02, 0.2, 0.5, 0.8, 0.98, 0.999})
    CHECK(cb.cdf(cb.invcdf(u)) == Approx(u).epsilon(1e-9));
  CHECK(cb.invcdf(0.02) < cb.m - cb.a * cb.s);  // low tail
  CHECK(cb.invcdf(0.999) > cb.m + cb.a * cb.s);  // high tail
}

TEST_CASE("scale correction formula and thresholds", "[muonscare]") {
  const MuonScaRe s(kFile);
  auto cset = correction::CorrectionSet::from_file(kFile);
  for (bool data : {false, true})
    for (int q : {-1, 1}) {
      const double eta = 0.7, phi = -1.2, pt = 60.0;
      const std::string dm = data ? "data" : "mc";
      const double a = cset->at("a_" + dm)->evaluate({eta, phi, "nom"});
      const double m = cset->at("m_" + dm)->evaluate({eta, phi, "nom"});
      CHECK(s.scale(data, pt, eta, phi, q, 26, 200) == Approx(1.0 / (m / pt + q * a)).epsilon(1e-14));
      CHECK(s.scale(data, pt, eta, phi, q, 26, 200) != pt);
    }
  CHECK(s.scale(false, 25.0, 0.7, -1.2, 1, 26, 200) == 25.0);
  CHECK(s.scale(false, 250.0, 0.7, -1.2, 1, 26, 200) == 250.0);
  CHECK(s.scale(false, 250.0, 0.7, -1.2, 1, 26, 1e9) != 250.0);  // the C++-reference behaviour when overridden
  CHECK(s.resolution(250.0, 0.7, -1.2, 12, 1, 1, 26, 200) == 250.0);
  CHECK(s.resolution(20.0, 0.7, -1.2, 12, 1, 1, 26, 200) == 20.0);
}

TEST_CASE("resolution smearing and its variations", "[muonscare]") {
  const MuonScaRe s(kFile);
  auto cset = correction::CorrectionSet::from_file(kFile);
  const double eta = 1.9, phi = 0.4, pt = 45.0;
  const float nL = 13;
  const double kd = cset->at("k_data")->evaluate({std::abs(eta), "nom"}), km = cset->at("k_mc")->evaluate({std::abs(eta), "nom"});
  REQUIRE(km < kd);
  CHECK(s.kFactor(eta, "nom") == Approx(std::sqrt(kd * kd - km * km)).epsilon(1e-14));
  // Same event: deterministic; the random number is the same for any lumi.
  const double a = s.resolution(pt, eta, phi, nL, 777, 1, 26, 200);
  CHECK(a == s.resolution(pt, eta, phi, nL, 777, 2, 26, 200));
  CHECK(a != pt);
  CHECK(a != s.resolution(pt, eta, phi, nL, 778, 1, 26, 200));
  const double kUnc = cset->at("k_mc")->evaluate({std::abs(eta), "stat"});
  const double kRes = std::sqrt(kd * kd - km * km);
  for (int sign : {+1, -1}) {
    CHECK(s.resolutionVariation(pt, a, eta, sign, false) ==
          Approx(pt * (1 + (kRes + sign * kUnc) * (a / pt - 1) / kRes)).epsilon(1e-12));
    CHECK(s.resolutionVariation(pt, a, eta, sign, true) ==
          Approx(pt * (1 + (km + sign * kUnc) * (a / pt - 1) / km)).epsilon(1e-12));
  }
  CHECK(s.resolutionVariation(pt, a, eta, +1, false) != s.resolutionVariation(pt, a, eta, +1, true));
  // Unphysical variations fall back to the unsmeared pt.
  CHECK(s.resolutionVariation(pt, 5 * pt, eta, +1, false) == pt);
}
