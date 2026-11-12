#ifndef SRothman_EventSelection_Selection_h
#define SRothman_EventSelection_Selection_h

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

// Framework-free event-level selection logic.
namespace evsel {

  struct Obj {
    double pt = 0, eta = 0, phi = 0, mass = 0;
    int charge = 0;
  };

  double deltaR(const Obj& a, const Obj& b);
  // Invariant mass of the pair.
  double pairMass(const Obj& a, const Obj& b);
  // Transverse mass of a lepton and missing transverse momentum.
  double transverseMass(double pt1, double phi1, double pt2, double phi2);

  struct PairCuts {
    double leadPtMin = 0, subleadPtMin = 0;
    double massMin = 0, massMax = 1e9;
    bool oppositeCharge = true;
    double targetMass = 91.1876;
  };
  // Indices (leading first) of the pair passing PairCuts with mass closest to targetMass.
  std::optional<std::pair<std::size_t, std::size_t>> bestPair(const std::vector<Obj>& objs, const PairCuts& c);

  // Number of objects in `others` farther than dRmin from every object in `selected`
  // (vetoes that are not the selected objects themselves).
  std::size_t countExtra(const std::vector<Obj>& others, const std::vector<Obj>& selected, double dRmin = 0.01);

  // Cone dressing: index of the object in `leptons` nearest to each photon within dR < cone, or -1.
  // A photon is assigned to at most one lepton.
  std::vector<int> assignToNearest(const std::vector<Obj>& leptons, const std::vector<Obj>& photons, double cone);

}  // namespace evsel

#endif
