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
#include "correction.h"

#include <cmath>
#include <string>
#include <vector>

// Expected values are evaluated here with correctionlib directly, from the Summer22EE MUO files on cvmfs.
namespace {
  const std::string kDir = "/cvmfs/cms-griddata.cern.ch/cat/metadata/MUO/Run3-22EFGSep23-Summer22EE-NanoAODv12/latest/";
  const std::string kZ = kDir + "muon_Z.json.gz", kHighPt = kDir + "muon_HighPt.json.gz";
  const std::string kId = "NUM_TightID_DEN_TrackerMuons";
  const std::string kTrig = "NUM_IsoMu24_DEN_CutBasedIdTight_and_PFIsoTight";
  const std::string kHighReco = "NUM_GlobalMuons_DEN_TrackerMuonProbes";
  const std::string kHighTrig =
      "NUM_IsoMu24_or_Mu50_or_CascadeMu100_or_HighPtTkMu100_DEN_CutBasedIdGlobalHighPt_and_TkIsoLoose";
  const std::vector<std::string> kPaths = {"HLT_IsoMu24_v15", "HLT_Mu50_v13", "HLT_Other_v1"};

  std::string term(const std::string& name,
                   const std::string& regime,
                   const std::string& file,
                   const std::string& key,
                   const std::string& paths,
                   const std::string& oor) {
    return "cms.PSet(name = cms.string('" + name + "'), regime = cms.string('" + regime + "'), file = cms.string('" +
           file + "'), key = cms.string('" + key + "'), trigger = cms.bool(" + (paths.empty() ? "False" : "True") +
           "), paths = cms.vstring(" + paths + "), outOfRange = cms.string('" + oor + "'))";
  }
  std::string config(const std::string& terms) {
    return "from FWCore.TestProcessor.TestProcess import *\nprocess = TestProcess()\n"
           "process.toTest = cms.EDProducer('MuonSFProducer', selected = cms.InputTag('sel'), src = "
           "cms.InputTag('leps'), muons = "
           "cms.InputTag('mus'), name = cms.string('ZmumuMuonSF'), matchDeltaR = cms.double(0.1), triggerResults = "
           "cms.InputTag('trig'), triggerObjects = cms.InputTag('objs'), triggerMatchDeltaR = cms.double(0.1), terms = "
           "cms.VPSet(" +
           terms + "))\nprocess.moduleToTest(process.toTest)\n";
  }
  reco::LeafCandidate lep(double pt, double eta, double phi, int q) {
    return reco::LeafCandidate(q, reco::LeafCandidate::PolarLorentzVector(pt, eta, phi, 0.10566));
  }
  pat::Muon mu(double pt, double eta, double phi, int q, int regime) {
    pat::Muon m(
        reco::Muon(q, reco::Particle::LorentzVector(reco::LeafCandidate::PolarLorentzVector(pt, eta, phi, 0.10566))));
    m.addUserInt("highPtRegime", regime);
    return m;
  }
  // HLT object at (eta, phi) attached to `path`, as the path's last filter or not.
  pat::TriggerObjectStandAlone obj(double eta, double phi, const std::string& path, bool lastFilter = true) {
    pat::TriggerObjectStandAlone o(reco::Particle::PolarLorentzVector(50, eta, phi, 0.10566), 83);
    o.addPathName(path, lastFilter, true);
    return o;
  }
  // TriggerResults over kPaths with the given accept bits.
  std::unique_ptr<edm::TriggerResults> results(std::vector<bool> accept) {
    edm::ParameterSet names;
    names.addParameter<std::vector<std::string>>("@trigger_paths", kPaths);
    names.registerIt();
    edm::HLTGlobalStatus status(kPaths.size());
    for (std::size_t i = 0; i < kPaths.size(); ++i)
      status[i] = edm::HLTPathStatus(accept[i] ? edm::hlt::Pass : edm::hlt::Fail);
    return std::make_unique<edm::TriggerResults>(status, names.id());
  }
  using Leps = std::vector<reco::LeafCandidate>;
  using Mus = std::vector<pat::Muon>;
  using Objs = std::vector<pat::TriggerObjectStandAlone>;
  double col(const nanoaod::FlatTable& t, const std::string& name) {
    const int i = t.columnIndex(name);
    REQUIRE(i >= 0);
    return t.getAnyValue(0, i);
  }
}  // namespace

TEST_CASE("MuonSFProducer: per-muon SFs by regime, trigger SF of the triggering muon, variations", "[muonsf]") {
  auto csZ = correction::CorrectionSet::from_file(kZ);
  auto csH = correction::CorrectionSet::from_file(kHighPt);
  auto z = [&](const std::string& key, double eta, double pt, const std::string& v) {
    return csZ->at(key)->evaluate({eta, pt, v});
  };
  auto h = [&](const std::string& key, double eta, double p, const std::string& v) {
    return csH->at(key)->evaluate({eta, p, v});
  };

  edm::test::TestProcessor::Config cfg{config(term("id", "medium", kZ, kId, "", "error") + ", " +
                                              term("trig", "medium", kZ, kTrig, "'HLT_IsoMu24_v*'", "skip") + ", " +
                                              term("hreco", "high", kHighPt, kHighReco, "", "error") + ", " +
                                              term("htrig", "high", kZ, kHighTrig, "'HLT_Mu50_v*'", "skip"))};
  auto putS = cfg.produces<bool>("sel");
  auto putL = cfg.produces<Leps>("leps");
  auto putM = cfg.produces<Mus>("mus");
  auto putT = cfg.produces<edm::TriggerResults>("trig");
  auto putO = cfg.produces<Objs>("objs");
  edm::test::TestProcessor tester(cfg);

  // Medium muon (45, 0.5), high muon (300, -1.5). The lepton copies are slightly displaced (as after dressing); the
  // muon collection also holds an opposite-charge muon nearer to the first lepton, which must not be matched.
  const double e1 = 0.5, p1 = 45, f1 = 1.0, e2 = -1.5, p2 = 300, f2 = -2.0;
  const double mom2 = p2 * std::cosh(e2);
  // Discriminating inputs: p rather than pt, and the matched muon's pt rather than the lepton's.
  REQUIRE(h(kHighReco, e2, mom2, "nominal") != h(kHighReco, e2, p2, "nominal"));
  REQUIRE(z(kId, e1, 52, "nominal") != z(kId, e1, p1, "nominal"));
  const double sfId = z(kId, e1, p1, "nominal"), sfReco = h(kHighReco, e2, mom2, "nominal");
  const double sf1 = z(kTrig, e1, p1, "nominal"), sf2 = z(kHighTrig, e2, p2, "nominal");
  REQUIRE(std::abs(sf1 / sf2 - 1) > 1e-4);
  auto run = [&](Objs objs, std::vector<bool> accept) {
    return tester.test(
        std::make_pair(putS, std::make_unique<bool>(true)),
        std::make_pair(putL, std::make_unique<Leps>(Leps{lep(52, e1 + 0.01, f1, +1), lep(310, e2 - 0.01, f2, -1)})),
        std::make_pair(
            putM,
            std::make_unique<Mus>(Mus{mu(30, e1 + 0.011, f1, -1, 0), mu(p1, e1, f1, +1, 0), mu(p2, e2, f2, -1, 1)})),
        std::make_pair(putT, results(accept)),
        std::make_pair(putO, std::make_unique<Objs>(objs)));
  };

  SECTION("both muons matched: only the leading one gets a trigger SF") {
    auto ev = run({obj(e1 + 0.02, f1, kPaths[0]), obj(e2, f2 + 0.02, kPaths[1])}, {true, true, false});
    const double nominal = sfId * sfReco * sf2;
    CHECK(*ev.get<double>() == Approx(nominal).epsilon(1e-12));
    const auto& t = *ev.get<nanoaod::FlatTable>();
    CHECK(t.name() == "ZmumuMuonSF");
    CHECK(col(t, "nominal") == Approx(nominal).epsilon(1e-6));
    CHECK(col(t, "id_statUp") == Approx((sfId + z(kId, e1, p1, "stat")) * sfReco * sf2).epsilon(1e-6));
    CHECK(col(t, "id_systDown") == Approx((sfId - z(kId, e1, p1, "syst")) * sfReco * sf2).epsilon(1e-6));
    CHECK(col(t, "hreco_systUp") == Approx(sfId * (sfReco + h(kHighReco, e2, mom2, "syst")) * sf2).epsilon(1e-6));
    CHECK(col(t, "htrig_systDown") == Approx(sfId * sfReco * (sf2 - z(kHighTrig, e2, p2, "syst"))).epsilon(1e-6));
    CHECK(col(t, "trig_statUp") == Approx(nominal).epsilon(1e-6));  // the medium muon is not the triggering one
    CHECK(col(t, "id_statUp") != Approx(nominal).epsilon(1e-9));
    CHECK(col(t, "htrig_systDown") != Approx(nominal).epsilon(1e-9));
    CHECK(t.nColumns() == 1 + 4 * 4);
  }
  SECTION("both muons matched, leading muon first in the inputs") {
    auto ev = tester.test(
        std::make_pair(putS, std::make_unique<bool>(true)),
        std::make_pair(putL, std::make_unique<Leps>(Leps{lep(310, e2 - 0.01, f2, -1), lep(52, e1 + 0.01, f1, +1)})),
        std::make_pair(putM, std::make_unique<Mus>(Mus{mu(p2, e2, f2, -1, 1), mu(p1, e1, f1, +1, 0)})),
        std::make_pair(putT, results({true, true, false})),
        std::make_pair(putO, std::make_unique<Objs>(Objs{obj(e2, f2, kPaths[1]), obj(e1, f1, kPaths[0])})));
    CHECK(*ev.get<double>() == Approx(sfId * sfReco * sf2).epsilon(1e-12));
  }
  // Each of these leaves only the medium muon matched to its regime's term.
  const double onlyMedium = sfId * sfReco * sf1;
  SECTION("high muon's path did not fire") {
    CHECK(*run({obj(e1, f1, kPaths[0]), obj(e2, f2, kPaths[1])}, {true, false, true}).get<double>() ==
          Approx(onlyMedium).epsilon(1e-12));
  }
  SECTION("high muon's object is not from the path's last filter") {
    CHECK(*run({obj(e1, f1, kPaths[0]), obj(e2, f2, kPaths[1], false)}, {true, true, false}).get<double>() ==
          Approx(onlyMedium).epsilon(1e-12));
  }
  SECTION("high muon's object is outside dR 0.1") {
    CHECK(*run({obj(e1, f1, kPaths[0]), obj(e2 + 0.15, f2, kPaths[1])}, {true, true, false}).get<double>() ==
          Approx(onlyMedium).epsilon(1e-12));
  }
  SECTION("high muon's object belongs to the medium term's path only") {
    auto ev = run({obj(e1, f1, kPaths[0]), obj(e2, f2, kPaths[0])}, {true, true, false});
    CHECK(*ev.get<double>() == Approx(onlyMedium).epsilon(1e-12));
    CHECK(col(*ev.get<nanoaod::FlatTable>(), "trig_statUp") ==
          Approx(sfId * sfReco * (sf1 + z(kTrig, e1, p1, "stat"))).epsilon(1e-6));
  }
  SECTION("event failing the channel selection: unit weights, no matching") {
    auto ev =
        tester.test(std::make_pair(putS, std::make_unique<bool>(false)),
                    std::make_pair(putL, std::make_unique<Leps>(Leps{lep(52, e1, f1, +1), lep(310, e2, f2, -1)})),
                    std::make_pair(putM, std::make_unique<Mus>(Mus{mu(p1, e1, f1, +1, 0), mu(p2, e2, f2, -1, 1)})),
                    std::make_pair(putT, results({false, false, false})),
                    std::make_pair(putO, std::make_unique<Objs>()));
    CHECK(*ev.get<double>() == 1.0);
    CHECK(col(*ev.get<nanoaod::FlatTable>(), "htrig_systDown") == 1.0);
  }
  SECTION("no muon matched (with or without a fired path): no trigger SF") {
    CHECK(*run({obj(e1, f1, kPaths[2]), obj(e2, f2, kPaths[2])}, {true, true, true}).get<double>() ==
          Approx(sfId * sfReco).epsilon(1e-12));
    CHECK(*run({}, {false, false, false}).get<double>() == Approx(sfId * sfReco).epsilon(1e-12));
  }
  SECTION("triggering muon below the trigger binning (26 GeV): trigger SF 1") {
    auto ev =
        tester.test(std::make_pair(putS, std::make_unique<bool>(true)),
                    std::make_pair(putL, std::make_unique<Leps>(Leps{lep(45, e1, f1, +1), lep(20, -0.3, -2.0, -1)})),
                    std::make_pair(putM, std::make_unique<Mus>(Mus{mu(45, e1, f1, +1, 0), mu(20, -0.3, -2.0, -1, 0)})),
                    std::make_pair(putT, results({true, false, false})),
                    std::make_pair(putO, std::make_unique<Objs>(Objs{obj(-0.3, -2.0, kPaths[0])})));
    CHECK(*ev.get<double>() == Approx(z(kId, e1, 45, "nominal") * z(kId, -0.3, 20, "nominal")).epsilon(1e-9));
  }
  SECTION("no leptons: unit weight") {
    auto ev = tester.test(std::make_pair(putS, std::make_unique<bool>(true)),
                          std::make_pair(putL, std::make_unique<Leps>()),
                          std::make_pair(putM, std::make_unique<Mus>()),
                          std::make_pair(putT, results({false, false, false})),
                          std::make_pair(putO, std::make_unique<Objs>()));
    CHECK(*ev.get<double>() == 1.0);
    CHECK(col(*ev.get<nanoaod::FlatTable>(), "trig_statUp") == 1.0);
  }
  SECTION("ID binning error and unmatched lepton") {
    auto objs = [] { return std::make_unique<Objs>(Objs{obj(0.5, 1.0, kPaths[0])}); };
    CHECK_THROWS_AS(tester.test(std::make_pair(putS, std::make_unique<bool>(true)),
                                std::make_pair(putL, std::make_unique<Leps>(Leps{lep(10, e1, f1, +1)})),
                                std::make_pair(putM, std::make_unique<Mus>(Mus{mu(10, e1, f1, +1, 0)})),
                                std::make_pair(putT, results({true, false, false})),
                                std::make_pair(putO, objs())),
                    cms::Exception);
    CHECK_THROWS_AS(tester.test(std::make_pair(putS, std::make_unique<bool>(true)),
                                std::make_pair(putL, std::make_unique<Leps>(Leps{lep(45, e1, f1, +1)})),
                                std::make_pair(putM, std::make_unique<Mus>(Mus{mu(45, e1, f1, -1, 0)})),
                                std::make_pair(putT, results({true, false, false})),
                                std::make_pair(putO, objs())),
                    cms::Exception);
  }
}

TEST_CASE("MuonSFProducer: no trigger SF for muons whose regime has no trigger term", "[muonsf]") {
  edm::test::TestProcessor::Config cfg{config(term("trig", "medium", kZ, kTrig, "'HLT_IsoMu24_v*'", "skip") + ", " +
                                              term("hreco", "high", kHighPt, kHighReco, "", "error"))};
  auto putS = cfg.produces<bool>("sel");
  auto putL = cfg.produces<Leps>("leps");
  auto putM = cfg.produces<Mus>("mus");
  auto putT = cfg.produces<edm::TriggerResults>("trig");
  auto putO = cfg.produces<Objs>("objs");
  edm::test::TestProcessor tester(cfg);
  auto ev = tester.test(std::make_pair(putS, std::make_unique<bool>(true)),
                        std::make_pair(putL, std::make_unique<Leps>(Leps{lep(300, -1.5, -2.0, -1)})),
                        std::make_pair(putM, std::make_unique<Mus>(Mus{mu(300, -1.5, -2.0, -1, 1)})),
                        std::make_pair(putT, results({false, false, false})),
                        std::make_pair(putO, std::make_unique<Objs>()));
  auto csH = correction::CorrectionSet::from_file(kHighPt);
  CHECK(*ev.get<double>() ==
        Approx(csH->at(kHighReco)->evaluate({-1.5, 300 * std::cosh(-1.5), "nominal"})).epsilon(1e-12));
}

TEST_CASE("MuonSFProducer: configuration errors", "[muonsf]") {
  const auto t = term("id", "medium", kZ, kId, "", "error");
  auto trig = [](const std::string& n) { return term(n, "medium", kZ, kTrig, "'HLT_IsoMu24_v*'", "skip"); };
  auto raw = [](const std::string& trigger, const std::string& paths) {
    return "cms.PSet(name = cms.string('x'), regime = cms.string('medium'), file = cms.string('" + kZ +
           "'), key = cms.string('" + kTrig + "'), trigger = cms.bool(" + trigger + "), paths = cms.vstring(" + paths +
           "), outOfRange = cms.string('skip'))";
  };
  REQUIRE_NOTHROW(edm::test::TestProcessor(edm::test::TestProcessor::Config{config(raw("True", "'HLT_IsoMu24_v*'"))}));
  for (const auto& bad : {config(t + ", " + t),                                            // duplicate name
                          config(trig("a") + ", " + trig("b")),                            // two triggers
                          config(term("id", "low", kZ, kId, "", "error")),                 // bad regime
                          config(term("id", "medium", kZ, kId, "", "clamp")),              // bad outOfRange
                          config(term("id", "medium", kZ, "NUM_NoSuchKey", "", "error")),  // unknown key
                          config(raw("True", "")),                                         // trigger term without paths
                          config(raw("False", "'HLT_IsoMu24_v*'"))}) {                     // paths on a per-muon term
    edm::test::TestProcessor::Config cfg{bad};
    cfg.produces<Leps>("leps");
    cfg.produces<Mus>("mus");
    CHECK_THROWS(edm::test::TestProcessor(cfg));
  }
}
