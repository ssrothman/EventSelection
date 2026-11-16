#ifndef SRothman_EventSelection_Neutrino_h
#define SRothman_EventSelection_Neutrino_h

// Framework-free neutrino reconstruction for W -> l nu.
namespace evsel {

  // Codes of NeutrinoResult::status for wMassNeutrino.
  enum NeutrinoStatus { kTwoRealRoots = 1, kOneRealRoot = 2, kComplexRoots = 3 };

  // Massless neutrino four-vector; the meaning of `status` belongs to the strategy that made it.
  struct NeutrinoResult {
    double px = 0, py = 0, pz = 0, e = 0;
    int status = 0;
  };

  // pT of the neutrino = MET; p_z from m(lepton, neutrino) = wMass, both massless: of the two real roots the one with
  // the smaller |p_z|, the real part when they are complex (m_T(lepton, MET) > wMass). Requires lepPt > 0.
  NeutrinoResult wMassNeutrino(double lepPt, double lepEta, double lepPhi, double metPt, double metPhi, double wMass);

}  // namespace evsel

#endif
