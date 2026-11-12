#include "SRothman/EventSelection/interface/Selection.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace evsel {

  double deltaR(const Obj& a, const Obj& b) {
    return std::hypot(a.eta - b.eta, std::remainder(a.phi - b.phi, 2 * M_PI));
  }

  double pairMass(const Obj& a, const Obj& b) {
    auto p4 = [](const Obj& o) {
      const double px = o.pt * std::cos(o.phi), py = o.pt * std::sin(o.phi), pz = o.pt * std::sinh(o.eta);
      return std::array<double, 4>{{px, py, pz, std::sqrt(px * px + py * py + pz * pz + o.mass * o.mass)}};
    };
    const auto x = p4(a), y = p4(b);
    const double E = x[3] + y[3], px = x[0] + y[0], py = x[1] + y[1], pz = x[2] + y[2];
    return std::sqrt(std::max(E * E - px * px - py * py - pz * pz, 0.0));
  }

  double transverseMass(double pt1, double phi1, double pt2, double phi2) {
    return std::sqrt(std::max(2 * pt1 * pt2 * (1 - std::cos(phi1 - phi2)), 0.0));
  }

  std::optional<std::pair<std::size_t, std::size_t>> bestPair(const std::vector<Obj>& o, const PairCuts& c) {
    std::optional<std::pair<std::size_t, std::size_t>> best;
    double bestDist = 0;
    for (std::size_t i = 0; i < o.size(); ++i)
      for (std::size_t j = i + 1; j < o.size(); ++j) {
        const std::size_t lead = o[i].pt >= o[j].pt ? i : j, sub = lead == i ? j : i;
        if (o[lead].pt < c.leadPtMin || o[sub].pt < c.subleadPtMin)
          continue;
        if (c.oppositeCharge && o[i].charge * o[j].charge >= 0)
          continue;
        const double m = pairMass(o[i], o[j]);
        if (m < c.massMin || m > c.massMax)
          continue;
        const double d = std::abs(m - c.targetMass);
        if (!best || d < bestDist) {
          best = std::make_pair(lead, sub);
          bestDist = d;
        }
      }
    return best;
  }

  std::size_t countExtra(const std::vector<Obj>& others, const std::vector<Obj>& selected, double dRmin) {
    std::size_t n = 0;
    for (const auto& x : others) {
      bool same = false;
      for (const auto& s : selected)
        same |= deltaR(x, s) < dRmin;
      n += !same;
    }
    return n;
  }

  std::vector<int> assignToNearest(const std::vector<Obj>& leptons, const std::vector<Obj>& photons, double cone) {
    std::vector<int> out(photons.size(), -1);
    for (std::size_t g = 0; g < photons.size(); ++g) {
      double best = cone;
      for (std::size_t l = 0; l < leptons.size(); ++l) {
        const double dR = deltaR(leptons[l], photons[g]);
        if (dR < best) {
          best = dR;
          out[g] = static_cast<int>(l);
        }
      }
    }
    return out;
  }

}  // namespace evsel
