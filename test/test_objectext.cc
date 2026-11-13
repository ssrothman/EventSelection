#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "DataFormats/Candidate/interface/LeafCandidate.h"
#include "DataFormats/Common/interface/TriggerResults.h"
#include "DataFormats/NanoAOD/interface/FlatTable.h"
#include "DataFormats/PatCandidates/interface/Muon.h"
#include "DataFormats/PatCandidates/interface/TriggerObjectStandAlone.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/TestProcessor/interface/TestProcessor.h"
#include "FWCore/Utilities/interface/Exception.h"

#include <string>
#include <vector>

namespace {
  const std::vector<std::string> kPaths = {"HLT_IsoMu24_v15", "HLT_Mu50_v13"};
  using Leps = std::vector<reco::LeafCandidate>;
  using Mus = std::vector<pat::Muon>;
  using Objs = std::vector<pat::TriggerObjectStandAlone>;
  reco::LeafCandidate lep(double pt, double eta, double phi, int q) {
    return reco::LeafCandidate(q, reco::LeafCandidate::PolarLorentzVector(pt, eta, phi, 0.10566));
  }
  pat::Muon mu(double pt, double eta, double phi, int q) {
    return pat::Muon(
        reco::Muon(q, reco::Particle::LorentzVector(reco::LeafCandidate::PolarLorentzVector(pt, eta, phi, 0.10566))));
  }
  pat::TriggerObjectStandAlone obj(double eta, double phi, const std::string& path, bool last = true) {
    pat::TriggerObjectStandAlone o(reco::Particle::PolarLorentzVector(50, eta, phi, 0.10566), 83);
    o.addPathName(path, last, true);
    return o;
  }
  std::unique_ptr<edm::TriggerResults> results(std::vector<bool> accept) {
    edm::ParameterSet names;
    names.addParameter<std::vector<std::string>>("@trigger_paths", kPaths);
    names.registerIt();
    edm::HLTGlobalStatus status(kPaths.size());
    for (std::size_t i = 0; i < kPaths.size(); ++i)
      status[i] = edm::HLTPathStatus(accept[i] ? edm::hlt::Pass : edm::hlt::Fail);
    return std::make_unique<edm::TriggerResults>(status, names.id());
  }
  std::string config(const std::string& vars, const std::string& paths = "'HLT_IsoMu24_v*'") {
    return "from FWCore.TestProcessor.TestProcess import *\nprocess = TestProcess()\n"
           "process.toTest = cms.EDProducer('MuonExtensionTableProducer', src = cms.InputTag('leps'), objects = "
           "cms.InputTag('mus'), name = cms.string('ZmumuLeptons'), matchDeltaR = cms.double(0.1), triggerPaths = "
           "cms.vstring(" +
           paths +
           "), triggerResults = cms.InputTag('trig'), triggerObjects = cms.InputTag('objs'), "
           "triggerMatchDeltaR = cms.double(0.1), variables = cms.VPSet(" +
           vars + "))\nprocess.moduleToTest(process.toTest)\n";
  }
  std::string var(const std::string& n, const std::string& e, const std::string& t) {
    return "cms.PSet(name = cms.string('" + n + "'), expr = cms.string('" + e + "'), type = cms.string('" + t +
           "'), doc = cms.string(''), precision = cms.int32(-1))";
  }
  double col(const nanoaod::FlatTable& t, const std::string& name, unsigned row) {
    const int c = t.columnIndex(name);
    REQUIRE(c >= 0);
    return t.getAnyValue(row, c);
  }
}  // namespace

TEST_CASE("MuonExtensionTableProducer: matched PAT columns, undressedPt, triggerMatched", "[objext]") {
  edm::test::TestProcessor::Config cfg{config(var("matchedPt", "pt", "float") + ", " +
                                              var("central", "abs(eta) < 1", "bool") + ", " +
                                              var("q", "charge", "int16"))};
  auto putL = cfg.produces<Leps>("leps");
  auto putM = cfg.produces<Mus>("mus");
  auto putT = cfg.produces<edm::TriggerResults>("trig");
  auto putO = cfg.produces<Objs>("objs");
  edm::test::TestProcessor tester(cfg);
  // Lepton copies with dressed (larger) pt; an opposite-charge muon sits nearer to the first lepton.
  auto run = [&](Objs objs, std::vector<bool> acc) {
    return tester.test(
        std::make_pair(putL, std::make_unique<Leps>(Leps{lep(55, 0.51, 1.0, +1), lep(42, -1.5, -2.0, -1)})),
        std::make_pair(
            putM, std::make_unique<Mus>(Mus{mu(30, 0.505, 1.0, -1), mu(50, 0.5, 1.0, +1), mu(40, -1.5, -2.0, -1)})),
        std::make_pair(putT, results(acc)),
        std::make_pair(putO, std::make_unique<Objs>(objs)));
  };
  SECTION("columns from the matched same-charge muon") {
    auto ev = run({obj(0.5, 1.0, kPaths[0])}, {true, false});
    const auto& t = *ev.get<nanoaod::FlatTable>();
    CHECK(t.name() == "ZmumuLeptons");
    CHECK(t.extension());
    REQUIRE(t.nRows() == 2);
    CHECK(col(t, "matchedPt", 0) == Approx(50));
    CHECK(col(t, "undressedPt", 0) == Approx(50));
    CHECK(col(t, "undressedPt", 1) == Approx(40));
    CHECK(col(t, "central", 0) == 1);
    CHECK(col(t, "central", 1) == 0);
    CHECK(col(t, "q", 0) == 1);
    CHECK(col(t, "triggerMatched", 0) == 1);
    CHECK(col(t, "triggerMatched", 1) == 0);
  }
  SECTION("path not fired, not the last filter, or another path: not trigger-matched") {
    for (auto [objs, acc] : {std::make_pair(Objs{obj(0.5, 1.0, kPaths[0])}, std::vector<bool>{false, true}),
                             std::make_pair(Objs{obj(0.5, 1.0, kPaths[0], false)}, std::vector<bool>{true, false}),
                             std::make_pair(Objs{obj(0.5, 1.0, kPaths[1])}, std::vector<bool>{true, true})}) {
      auto ev = run(objs, acc);
      CHECK(col(*ev.get<nanoaod::FlatTable>(), "triggerMatched", 0) == 0);
    }
  }
  SECTION("no object within dR: error") {
    CHECK_THROWS_AS(tester.test(std::make_pair(putL, std::make_unique<Leps>(Leps{lep(55, 0.51, 1.0, +1)})),
                                std::make_pair(putM, std::make_unique<Mus>(Mus{mu(50, 0.5, 1.0, -1)})),
                                std::make_pair(putT, results({true, false})),
                                std::make_pair(putO, std::make_unique<Objs>())),
                    cms::Exception);
  }
}

TEST_CASE("MuonExtensionTableProducer: no triggerMatched without paths; configuration errors", "[objext]") {
  edm::test::TestProcessor::Config cfg{config(var("matchedPt", "pt", "float"), "")};
  auto putL = cfg.produces<Leps>("leps");
  auto putM = cfg.produces<Mus>("mus");
  edm::test::TestProcessor tester(cfg);
  auto ev = tester.test(std::make_pair(putL, std::make_unique<Leps>(Leps{lep(55, 0.51, 1.0, +1)})),
                        std::make_pair(putM, std::make_unique<Mus>(Mus{mu(50, 0.5, 1.0, +1)})));
  CHECK(ev.get<nanoaod::FlatTable>()->columnIndex("triggerMatched") < 0);
  for (const auto& bad :
       {var("a", "pt", "double"), var("undressedPt", "pt", "float"), var("a", "noSuchMethod()", "float")}) {
    edm::test::TestProcessor::Config c{config(bad, "")};
    c.produces<Leps>("leps");
    c.produces<Mus>("mus");
    CHECK_THROWS(edm::test::TestProcessor(c));
  }
}
