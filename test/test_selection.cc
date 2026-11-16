#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "SRothman/EventSelection/interface/Neutrino.h"
#include "SRothman/EventSelection/interface/Selection.h"

#include <cmath>

using namespace evsel;

namespace {
  // Two massless leptons back to back in phi at eta 0 with pt p have mass 2p.
  Obj lep(double pt, double eta, double phi, int q) { return {pt, eta, phi, 0.0, q}; }
}  // namespace

TEST_CASE("pairMass and transverseMass", "[selection]") {
  CHECK(pairMass(lep(45, 0, 0, 1), lep(45, 0, M_PI, -1)) == Approx(90));
  // Same direction, massless: zero mass.
  CHECK(pairMass(lep(30, 1, 1, 1), lep(60, 1, 1, -1)) == Approx(0).margin(1e-6));
  CHECK(transverseMass(40, 0, 40, M_PI) == Approx(80));
  CHECK(transverseMass(40, 0.3, 25, 0.3) == Approx(0).margin(1e-9));
}

TEST_CASE("bestPair picks the opposite-sign pair closest to the target mass", "[selection]") {
  PairCuts c;
  c.leadPtMin = 26;
  c.subleadPtMin = 15;
  c.massMin = 71;
  c.massMax = 111;
  // (0,1): m = 2*sqrt(40*40)=80 (OS); (0,2): m ~ 2*sqrt(40*46)=85.8 (OS) is closer to 91; (1,2) same sign.
  std::vector<Obj> o{lep(40, 0, 0, 1), lep(40, 0, M_PI, -1), lep(46, 0, M_PI, -1)};
  auto p = bestPair(o, c);
  REQUIRE(p);
  CHECK(p->first == 2);  // leading first
  CHECK(p->second == 0);
  c.oppositeCharge = false;
  o[2].charge = -1;
  CHECK(bestPair(o, c));
  // Same-sign only: no pair with OS required.
  c.oppositeCharge = true;
  CHECK_FALSE(bestPair({lep(40, 0, 0, 1), lep(46, 0, M_PI, 1)}, c));
  // Subleading below threshold.
  CHECK_FALSE(bestPair({lep(170, 0, 0, 1), lep(12, 0, M_PI, -1)}, c));
  // Outside the mass window.
  CHECK_FALSE(bestPair({lep(30, 0, 0, 1), lep(30, 0, M_PI, -1)}, c));  // m = 60
  CHECK_FALSE(bestPair({}, c));
}

TEST_CASE("selectDilepton applies pairPtMin to the chosen pair", "[selection]") {
  PairCuts c;
  c.leadPtMin = 26;
  c.subleadPtMin = 15;
  c.massMin = 71;
  c.massMax = 111;
  // Same phi, deta 0.8814: pair pt 200, m = 91.0.
  const std::vector<Obj> z{lep(100, 0, 0.3, 1), lep(100, 0.8814, 0.3, -1)};
  c.pairPtMin = 150;
  REQUIRE(selectDilepton(z, c));
  CHECK(selectDilepton(z, c)->first == 0);
  c.pairPtMin = 199;
  CHECK(selectDilepton(z, c));
  c.pairPtMin = 201;
  CHECK_FALSE(selectDilepton(z, c));
  // (0,1): m 85.8, pt 6, closest to the Z mass; (2,3): m 82.2, pt 200; mixed OS pairs fall outside the window.
  const std::vector<Obj> o{lep(40, 0, 0, 1), lep(46, 0, M_PI, -1), lep(100, 0, 0, 1), lep(100, 0.8, 0, -1)};
  c.pairPtMin = 0;
  auto p = selectDilepton(o, c);
  REQUIRE(p);
  CHECK(p->first == 1);
  CHECK(p->second == 0);
  // The chosen low-pt pair fails; the high-pt pair is not chosen instead.
  c.pairPtMin = 150;
  CHECK(bestPair(o, c));
  CHECK_FALSE(selectDilepton(o, c));
  CHECK_FALSE(selectDilepton({}, c));
}

TEST_CASE("countExtra ignores the selected objects themselves", "[selection]") {
  const std::vector<Obj> sel{lep(40, 0.5, 1.0, 1)};
  const std::vector<Obj> veto{lep(40, 0.5, 1.0, 1), lep(12, -1.0, 2.0, -1)};
  CHECK(countExtra(veto, sel) == 1);
  CHECK(countExtra({lep(40, 0.5, 1.0, 1)}, sel) == 0);
  CHECK(countExtra(veto, {}) == 2);
}

TEST_CASE("assignToNearest gives each photon to the nearest lepton inside the cone", "[selection][dressing]") {
  const std::vector<Obj> leps{lep(40, 0.0, 0.0, -1), lep(30, 0.0, 0.15, 1)};
  const std::vector<Obj> photons{lep(2, 0.0, 0.05, 0),    // dR 0.05 to lep 0, 0.10 to lep 1 -> 0
                                 lep(2, 0.0, 0.11, 0),    // dR 0.11 to lep 0, 0.04 to lep 1 -> 1
                                 lep(2, 0.3, 0.0, 0),     // outside both cones -> -1
                                 lep(2, 0.0, 0.0999, 0)}; // 0.0999 vs 0.0501 -> 1
  CHECK(assignToNearest(leps, photons, 0.1) == std::vector<int>{0, 1, -1, 1});
  CHECK(assignToNearest(leps, photons, 0.01) == std::vector<int>{-1, -1, -1, -1});
  CHECK(assignToNearest({}, photons, 0.1) == std::vector<int>{-1, -1, -1, -1});
  CHECK(assignToNearest(leps, {}, 0.1).empty());
}

namespace {
  constexpr double kMW = 80.3692;
  // Invariant mass of a massless lepton (pt, eta, phi) and the massless neutrino r.
  double mLepNu(double pt, double eta, double phi, const NeutrinoResult& r) {
    const double e = pt * std::cosh(eta) + r.e, px = pt * std::cos(phi) + r.px, py = pt * std::sin(phi) + r.py,
                 pz = pt * std::sinh(eta) + r.pz;
    return std::sqrt(std::max(0.0, e * e - px * px - py * py - pz * pz));
  }
}  // namespace

TEST_CASE("wMassNeutrino: two real roots, the one with the smaller |pz|", "[neutrino]") {
  // Lepton (50, 1.0, 0.3), MET (40, 2.5): mT < mW, so two real roots.
  const auto r = wMassNeutrino(50, 1.0, 0.3, 40, 2.5, kMW);
  CHECK(r.status == kTwoRealRoots);
  CHECK(mLepNu(50, 1.0, 0.3, r) == Approx(kMW).epsilon(1e-9));
  CHECK(std::hypot(r.px, r.py) == Approx(40));
  CHECK(r.e == Approx(std::hypot(40, r.pz)));
  // The other root has the larger |pz|: reconstruct it from the sum of the roots 2 mu pz,l / pT_l^2.
  const double mu = 0.5 * kMW * kMW + 50 * 40 * std::cos(2.5 - 0.3);
  const double other = 2 * mu * 50 * std::sinh(1.0) / (50 * 50) - r.pz;
  CHECK(std::abs(other) > std::abs(r.pz));
  NeutrinoResult o = r;
  o.pz = other;
  o.e = std::hypot(40, other);
  CHECK(mLepNu(50, 1.0, 0.3, o) == Approx(kMW).epsilon(1e-9));
}

TEST_CASE("wMassNeutrino: complex roots when mT > mW, the real part is returned", "[neutrino]") {
  // mT = sqrt(2 * 60 * 60 * (1 - cos(pi))) = 120 > mW.
  const auto r = wMassNeutrino(60, -0.7, 1.0, 60, 1.0 + M_PI, kMW);
  CHECK(r.status == kComplexRoots);
  const double mu = 0.5 * kMW * kMW + 60 * 60 * std::cos(M_PI);
  CHECK(r.pz == Approx(mu * 60 * std::sinh(-0.7) / (60 * 60)));
  CHECK(mLepNu(60, -0.7, 1.0, r) != Approx(kMW).epsilon(1e-3));
  // Complex exactly when mT > mW, on a grid.
  for (double metPt : {10.0, 40.0, 90.0, 150.0})
    for (double dphi : {0.0, 1.0, 2.0, 3.0}) {
      const double mt = transverseMass(45, 0.2, metPt, 0.2 + dphi);
      CHECK((wMassNeutrino(45, 0.2, 0.2, metPt, 0.2 + dphi, kMW).status == kComplexRoots) == (mt > kMW));
    }
}

TEST_CASE("wMassNeutrino: the repeated root, a central lepton, rotation invariance", "[neutrino]") {
  // wMass 4, lepton pt 2 and MET pt 2 back to back: mu = 8 - 4 = 4 = pT_l pT_nu, discriminant exactly 0.
  const auto r = wMassNeutrino(2, 0.4, 0.0, 2, M_PI, 4.0);
  CHECK(r.status == kOneRealRoot);
  CHECK(r.pz == Approx(4 * 2 * std::sinh(0.4) / 4));
  // Central lepton (pz,l = 0): roots +-, |pz| equal, either is fine, the mass condition holds.
  const auto c = wMassNeutrino(50, 0.0, 0.0, 30, 2.0, kMW);
  CHECK(c.status == kTwoRealRoots);
  CHECK(mLepNu(50, 0.0, 0.0, c) == Approx(kMW).epsilon(1e-9));
  // A common phi rotation leaves pz and the status unchanged and rotates the transverse part.
  const auto a = wMassNeutrino(50, 1.0, 0.3, 40, 2.5, kMW), b = wMassNeutrino(50, 1.0, 0.3 + 0.8, 40, 2.5 + 0.8, kMW);
  CHECK(a.pz == Approx(b.pz).epsilon(1e-12));
  CHECK(a.status == b.status);
  CHECK_THROWS(wMassNeutrino(0, 0, 0, 40, 0, kMW));
}
