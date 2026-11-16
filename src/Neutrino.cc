#include "SRothman/EventSelection/interface/Neutrino.h"

#include <cmath>
#include <stdexcept>

namespace evsel {

  NeutrinoResult wMassNeutrino(double lepPt, double lepEta, double lepPhi, double metPt, double metPhi, double wMass) {
    if (!(lepPt > 0))
      throw std::invalid_argument("wMassNeutrino: lepton pt must be positive");
    const double pzL = lepPt * std::sinh(lepEta), eL = lepPt * std::cosh(lepEta);
    const double mu = 0.5 * wMass * wMass + lepPt * metPt * std::cos(metPhi - lepPhi);
    // Roots of pT_l^2 p_z^2 - 2 mu p_z,l p_z + (E_l^2 pT_nu^2 - mu^2) = 0, whose discriminant/4 is E_l^2 d.
    const double d = mu * mu - lepPt * lepPt * metPt * metPt;
    const double centre = mu * pzL / (lepPt * lepPt);
    NeutrinoResult r;
    if (d < 0) {
      r.pz = centre;
      r.status = kComplexRoots;
    } else {
      const double half = eL * std::sqrt(d) / (lepPt * lepPt);
      r.pz = centre >= 0 ? centre - half : centre + half;
      r.status = d == 0 ? kOneRealRoot : kTwoRealRoots;
    }
    r.px = metPt * std::cos(metPhi);
    r.py = metPt * std::sin(metPhi);
    r.e = std::hypot(metPt, r.pz);
    return r;
  }

}  // namespace evsel
