#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "DataFormats/Candidate/interface/LeafCandidate.h"
#include "DataFormats/NanoAOD/interface/FlatTable.h"
#include "FWCore/TestProcessor/interface/TestProcessor.h"
#include "FWCore/Utilities/interface/Exception.h"

#include <cmath>
#include <string>

using Cands = std::vector<reco::LeafCandidate>;

namespace {
  const std::string kHeader = "from FWCore.TestProcessor.TestProcess import *\nprocess = TestProcess()\n";

  reco::LeafCandidate cand(double pt, double eta, double phi, int q, double m = 0.0) {
    return reco::LeafCandidate(q, reco::LeafCandidate::PolarLorentzVector(pt, eta, phi, m));
  }
  std::unique_ptr<Cands> cands(std::initializer_list<reco::LeafCandidate> l) { return std::make_unique<Cands>(l); }
}  // namespace

TEST_CASE("DileptonSelector", "[modules]") {
  edm::test::TestProcessor::Config config{kHeader +
                                          "process.toTest = cms.EDProducer('DileptonSelector', src = "
                                          "cms.InputTag('leps'))\nprocess.moduleToTest(process.toTest)\n"};
  auto put = config.produces<Cands>("leps");
  edm::test::TestProcessor tester(config);
  {
    // Back-to-back massless pair: m = 2 sqrt(40 * 52) = 91.2. A third lepton gives worse pairs.
    auto ev = tester.test(std::make_pair(put, cands({cand(40, 0, 0, 1), cand(52, 0, M_PI, -1), cand(20, 0, 1.5, -1)})));
    CHECK(*ev.get<bool>());
    const auto& z = *ev.get<Cands>("boson");
    REQUIRE(z.size() == 1);
    CHECK(z[0].mass() == Approx(2 * std::sqrt(40.0 * 52)).epsilon(1e-6));
    CHECK(z[0].charge() == 0);
    const auto& l = *ev.get<Cands>("leptons");
    REQUIRE(l.size() == 2);
    CHECK(l[0].pt() == Approx(52));
    CHECK(l[1].pt() == Approx(40));
  }
  {
    // Same sign.
    auto ev = tester.test(std::make_pair(put, cands({cand(40, 0, 0, 1), cand(52, 0, M_PI, 1)})));
    CHECK_FALSE(*ev.get<bool>());
    CHECK(ev.get<Cands>("boson")->empty());
    CHECK(ev.get<Cands>("leptons")->empty());
  }
  {
    // Leading below 26.
    auto ev = tester.test(std::make_pair(put, cands({cand(25, 0, 0, 1), cand(25, 2.0, M_PI, -1)})));
    CHECK_FALSE(*ev.get<bool>());
  }
}

TEST_CASE("LeptonMETSelector", "[modules]") {
  edm::test::TestProcessor::Config config{kHeader +
                                          "process.toTest = cms.EDProducer('LeptonMETSelector', src = "
                                          "cms.InputTag('leps'), met = cms.InputTag('met'), vetoSrcs = "
                                          "cms.VInputTag('veto'))\nprocess.moduleToTest(process.toTest)\n"};
  auto putL = config.produces<Cands>("leps");
  auto putM = config.produces<Cands>("met");
  auto putV = config.produces<Cands>("veto");
  edm::test::TestProcessor tester(config);
  const auto mu = cand(45, 0.5, 0.0, -1);
  {
    // mT = sqrt(2 * 45 * 50 * (1 - cos(pi))) = 94.9; veto contains the selected muon itself only.
    auto ev = tester.test(std::make_pair(putL, cands({mu})), std::make_pair(putM, cands({cand(50, 0, M_PI, 0)})),
                          std::make_pair(putV, cands({mu})));
    CHECK(*ev.get<bool>());
    const auto& w = *ev.get<Cands>("boson");
    REQUIRE(w.size() == 1);
    CHECK(w[0].pt() == Approx(5).margin(1e-6));
    CHECK(w[0].charge() == -1);
  }
  {
    // An extra veto lepton fails the event.
    auto ev = tester.test(std::make_pair(putL, cands({mu})), std::make_pair(putM, cands({cand(50, 0, M_PI, 0)})),
                          std::make_pair(putV, cands({mu, cand(12, -1, 2, 1)})));
    CHECK_FALSE(*ev.get<bool>());
  }
  {
    // mT too low: MET parallel to the muon.
    auto ev = tester.test(std::make_pair(putL, cands({mu})), std::make_pair(putM, cands({cand(50, 0, 0.1, 0)})),
                          std::make_pair(putV, cands({mu})));
    CHECK_FALSE(*ev.get<bool>());
  }
  {
    // Two leptons above ptMin.
    auto ev = tester.test(std::make_pair(putL, cands({mu, cand(35, -0.5, 2, 1)})),
                          std::make_pair(putM, cands({cand(50, 0, M_PI, 0)})), std::make_pair(putV, cands({})));
    CHECK_FALSE(*ev.get<bool>());
  }
}

TEST_CASE("SingleObjectEventSelector", "[modules]") {
  edm::test::TestProcessor::Config config{kHeader +
                                          "process.toTest = cms.EDProducer('SingleObjectEventSelector', src = "
                                          "cms.InputTag('pho'), vetoSrcs = cms.VInputTag('veto'))\n"
                                          "process.moduleToTest(process.toTest)\n"};
  auto putP = config.produces<Cands>("pho");
  auto putV = config.produces<Cands>("veto");
  edm::test::TestProcessor tester(config);
  {
    auto ev = tester.test(std::make_pair(putP, cands({cand(250, 0.2, 1, 0)})), std::make_pair(putV, cands({})));
    CHECK(*ev.get<bool>());
    CHECK(ev.get<Cands>("objects")->size() == 1);
  }
  {
    auto ev = tester.test(std::make_pair(putP, cands({cand(250, 0.2, 1, 0)})), std::make_pair(putV, cands({cand(20, 0, 0, 1)})));
    CHECK_FALSE(*ev.get<bool>());
    CHECK(ev.get<Cands>("objects")->empty());
  }
  {
    auto ev = tester.test(std::make_pair(putP, cands({cand(250, 0.2, 1, 0), cand(230, -1, 2, 0)})),
                          std::make_pair(putV, cands({})));
    CHECK_FALSE(*ev.get<bool>());  // nMax = 1
  }
}

TEST_CASE("FlagCombiner, EventFlagFilter and EventFlagTableProducer", "[modules]") {
  for (std::string mode : {"and", "or"}) {
    edm::test::TestProcessor::Config config{kHeader +
                                            "process.toTest = cms.EDProducer('FlagCombiner', srcs = cms.VInputTag('a', "
                                            "'b'), mode = cms.string('" +
                                            mode + "'))\nprocess.moduleToTest(process.toTest)\n"};
    auto a = config.produces<bool>("a");
    auto b = config.produces<bool>("b");
    edm::test::TestProcessor tester(config);
    for (bool x : {false, true})
      for (bool y : {false, true}) {
        auto ev = tester.test(std::make_pair(a, std::make_unique<bool>(x)), std::make_pair(b, std::make_unique<bool>(y)));
        CHECK(*ev.get<bool>() == (mode == "and" ? (x && y) : (x || y)));
      }
  }
  {
    edm::test::TestProcessor::Config config{kHeader +
                                            "process.toTest = cms.EDFilter('EventFlagFilter', srcs = cms.VInputTag('a', "
                                            "'b'), mode = cms.string('or'))\nprocess.moduleToTest(process.toTest)\n"};
    auto a = config.produces<bool>("a");
    auto b = config.produces<bool>("b");
    edm::test::TestProcessor tester(config);
    CHECK(tester.test(std::make_pair(a, std::make_unique<bool>(false)), std::make_pair(b, std::make_unique<bool>(true)))
              .modulePassed());
    CHECK_FALSE(tester.test(std::make_pair(a, std::make_unique<bool>(false)), std::make_pair(b, std::make_unique<bool>(false)))
                    .modulePassed());
  }
  {
    edm::test::TestProcessor::Config config{kHeader +
                                            "process.toTest = cms.EDProducer('EventFlagTableProducer', srcs = "
                                            "cms.VInputTag('a', 'b'), names = cms.vstring('first', 'second'), name = "
                                            "cms.string('Sel'))\nprocess.moduleToTest(process.toTest)\n"};
    auto a = config.produces<bool>("a");
    auto b = config.produces<bool>("b");
    edm::test::TestProcessor tester(config);
    auto ev = tester.test(std::make_pair(a, std::make_unique<bool>(true)), std::make_pair(b, std::make_unique<bool>(false)));
    const auto& t = *ev.get<nanoaod::FlatTable>();
    CHECK(t.name() == "Sel");
    CHECK(t.singleton());
    CHECK(t.getAnyValue(0, t.columnIndex("first")) == 1);
    CHECK(t.getAnyValue(0, t.columnIndex("second")) == 0);
  }
  edm::test::TestProcessor::Config bad{kHeader +
                                       "process.toTest = cms.EDProducer('FlagCombiner', srcs = cms.VInputTag('a'), mode "
                                       "= cms.string('xor'))\nprocess.moduleToTest(process.toTest)\n"};
  bad.produces<bool>("a");
  CHECK_THROWS_AS(edm::test::TestProcessor(bad), cms::Exception);
}
