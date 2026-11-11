#ifndef SRothman_EventSelection_plugins_Common_h
#define SRothman_EventSelection_plugins_Common_h

#include "DataFormats/Candidate/interface/Candidate.h"
#include "DataFormats/Candidate/interface/LeafCandidate.h"
#include "DataFormats/Common/interface/View.h"
#include "SRothman/EventSelection/interface/Selection.h"

#include <vector>

namespace evsel {

  inline Obj toObj(const reco::Candidate& c) { return {c.pt(), c.eta(), c.phi(), c.mass(), c.charge()}; }

  inline std::vector<Obj> toObjs(const edm::View<reco::Candidate>& v) {
    std::vector<Obj> out;
    out.reserve(v.size());
    for (const auto& c : v)
      out.push_back(toObj(c));
    return out;
  }

  inline reco::LeafCandidate copy(const reco::Candidate& c) { return reco::LeafCandidate(c.charge(), c.p4(), c.vertex(), c.pdgId()); }

}  // namespace evsel

#endif
