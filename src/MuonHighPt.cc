#include "SRothman/EventSelection/interface/MuonHighPt.h"

#include <cmath>
#include <random>

namespace evsel {

  int MuonHighPt::etaBin(double eta) {
    static constexpr std::array<double, 7> edges{{-2.4, -2.1, -1.2, 0.0, 1.2, 2.1, 2.4}};
    if (!(eta >= edges.front() && eta < edges.back()))
      return -1;
    int b = 0;
    while (eta >= edges[b + 1])
      ++b;
    return b;
  }

  int MuonHighPt::phiBin(double phi) {
    return phi < -M_PI / 3 ? 0 : phi < M_PI / 3 ? 1 : 2;
  }

  double MuonHighPt::kappa(double eta, double phi) const {
    const int e = etaBin(eta);
    return e < 0 ? 0.0 : p_.kappa[phiBin(phi) * 6 + e];
  }

  double MuonHighPt::kappaUnc(double eta, double phi) const {
    const int e = etaBin(eta);
    return e < 0 ? 0.0 : p_.kappaUnc[phiBin(phi) * 6 + e];
  }

  double MuonHighPt::shiftCurvature(double pt, int charge, double kappa) {
    const double q = charge < 0 ? -1.0 : 1.0;
    const double c = q / pt + kappa / 1000.0;
    return c * q > 0 ? q / c : pt;
  }

  double MuonHighPt::sigma(double p, double eta) const {
    const auto& c = std::abs(eta) < 1.2 ? p_.sigmaBarrel : p_.sigmaEndcap;
    return c[0] + p * (c[1] + p * (c[2] + p * c[3]));
  }

  double MuonHighPt::smearFactor(double eta) const { return std::abs(eta) < 1.2 ? p_.smearBarrel : p_.smearEndcap; }

  double MuonHighPt::smear(double p, double eta, double factor, double gauss) const {
    const double f = 1.0 + gauss * sigma(p, eta) * factor;
    return f > 0 ? f : 1.0;
  }

  double seededGauss(std::uint64_t run, std::uint64_t lumi, std::uint64_t event, std::uint64_t index, std::uint64_t salt) {
    // splitmix64 finaliser applied to each word in turn.
    auto mix = [](std::uint64_t x) {
      x += 0x9e3779b97f4a7c15ull;
      x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
      x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
      return x ^ (x >> 31);
    };
    std::uint64_t h = mix(salt);
    for (auto w : {run, lumi, event, index})
      h = mix(h ^ w);
    std::mt19937_64 rng(h);
    return std::normal_distribution<double>(0.0, 1.0)(rng);
  }

}  // namespace evsel
