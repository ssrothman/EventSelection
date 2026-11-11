#define CATCH_CONFIG_MAIN
#include "catch.hpp"

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

TEST_CASE("countExtra ignores the selected objects themselves", "[selection]") {
  const std::vector<Obj> sel{lep(40, 0.5, 1.0, 1)};
  const std::vector<Obj> veto{lep(40, 0.5, 1.0, 1), lep(12, -1.0, 2.0, -1)};
  CHECK(countExtra(veto, sel) == 1);
  CHECK(countExtra({lep(40, 0.5, 1.0, 1)}, sel) == 0);
  CHECK(countExtra(veto, {}) == 2);
}
