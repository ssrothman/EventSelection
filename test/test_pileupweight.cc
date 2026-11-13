#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "DataFormats/NanoAOD/interface/FlatTable.h"
#include "FWCore/TestProcessor/interface/TestProcessor.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "SimDataFormats/PileupSummaryInfo/interface/PileupSummaryInfo.h"
#include "correction.h"

#include <string>
#include <vector>

namespace {
  const std::string kFile =
      "/cvmfs/cms-griddata.cern.ch/cat/metadata/LUM/Run3-22EFGSep23-Summer22EE-NanoAODv12/latest/puWeights.json.gz";
  const std::string kKey = "Collisions2022_359022_362760_eraEFG_GoldenJson";
  PileupSummaryInfo pu(int bx, float nTrue) {
    return PileupSummaryInfo(0, {}, {}, {}, {}, {}, {}, {}, {}, bx, nTrue, 25);
  }
  double col(const nanoaod::FlatTable& t, const std::string& n) {
    const int i = t.columnIndex(n);
    REQUIRE(i >= 0);
    return t.getAnyValue(0, i);
  }
}  // namespace

TEST_CASE("PileupWeightTableProducer: in-time true interactions, three weights", "[pu]") {
  edm::test::TestProcessor::Config cfg{
      "from FWCore.TestProcessor.TestProcess import *\nprocess = TestProcess()\n"
      "process.toTest = cms.EDProducer('PileupWeightTableProducer', src = cms.InputTag('pu'), file = cms.string('" +
      kFile + "'), key = cms.string('" + kKey +
      "'), name = cms.string('Pileup'), extension = cms.bool(True))\nprocess.moduleToTest(process.toTest)\n"};
  auto put = cfg.produces<std::vector<PileupSummaryInfo>>("pu");
  edm::test::TestProcessor tester(cfg);
  auto c = correction::CorrectionSet::from_file(kFile)->at(kKey);
  // The out-of-time entries carry a different true number, so using them would change the weight.
  REQUIRE(c->evaluate({31.5, "nominal"}) != c->evaluate({20.0, "nominal"}));
  auto ev = tester.test(std::make_pair(put,
                                       std::make_unique<std::vector<PileupSummaryInfo>>(
                                           std::vector<PileupSummaryInfo>{pu(-1, 20), pu(0, 31.5), pu(1, 20)})));
  const auto& t = *ev.get<nanoaod::FlatTable>();
  CHECK(t.name() == "Pileup");
  CHECK(t.extension());
  CHECK(t.singleton());
  CHECK(col(t, "weight") == Approx(c->evaluate({31.5, "nominal"})).epsilon(1e-6));
  CHECK(col(t, "weightUp") == Approx(c->evaluate({31.5, "up"})).epsilon(1e-6));
  CHECK(col(t, "weightDown") == Approx(c->evaluate({31.5, "down"})).epsilon(1e-6));
  CHECK(col(t, "weightUp") != Approx(col(t, "weightDown")));
  CHECK_THROWS_AS(
      tester.test(std::make_pair(
          put, std::make_unique<std::vector<PileupSummaryInfo>>(std::vector<PileupSummaryInfo>{pu(-1, 20)}))),
      cms::Exception);
}
