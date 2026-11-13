#include "DataFormats/NanoAOD/interface/FlatTable.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/global/EDProducer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "SimDataFormats/PileupSummaryInfo/interface/PileupSummaryInfo.h"
#include "correction.h"

#include <memory>
#include <string>
#include <vector>

// Pileup weights (nominal, up, down) from a LUM correctionlib key evaluated at the in-time true number of interactions,
// as a singleton table `name` (an extension of the standard Pileup table when `extension`).
class PileupWeightTableProducer : public edm::global::EDProducer<> {
public:
  explicit PileupWeightTableProducer(const edm::ParameterSet& cfg)
      : src_(consumes(cfg.getParameter<edm::InputTag>("src"))),
        put_(produces()),
        name_(cfg.getParameter<std::string>("name")),
        extension_(cfg.getParameter<bool>("extension")),
        corr_(correction::CorrectionSet::from_file(cfg.getParameter<std::string>("file"))
                  ->at(cfg.getParameter<std::string>("key"))) {}

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::InputTag>("src", edm::InputTag("slimmedAddPileupInfo"));
    desc.add<std::string>("file");
    desc.add<std::string>("key");
    desc.add<std::string>("name", "Pileup");
    desc.add<bool>("extension", true);
    descriptions.addWithDefaultLabel(desc);
  }

  void produce(edm::StreamID, edm::Event& event, const edm::EventSetup&) const override {
    double nTrue = -1;
    for (const auto& p : event.get(src_))
      if (p.getBunchCrossing() == 0)
        nTrue = p.getTrueNumInteractions();
    if (nTrue < 0)
      throw cms::Exception("PileupWeightTableProducer") << "no in-time PileupSummaryInfo";
    auto table = std::make_unique<nanoaod::FlatTable>(1, name_, true, extension_);
    for (const auto& [col, var] :
         {std::pair<std::string, std::string>{"weight", "nominal"}, {"weightUp", "up"}, {"weightDown", "down"}})
      table->addColumnValue<float>(col, corr_->evaluate({nTrue, var}), "pileup weight (" + var + ")");
    event.put(put_, std::move(table));
  }

private:
  const edm::EDGetTokenT<std::vector<PileupSummaryInfo>> src_;
  const edm::EDPutTokenT<nanoaod::FlatTable> put_;
  const std::string name_;
  const bool extension_;
  const correction::Correction::Ref corr_;
};

DEFINE_FWK_MODULE(PileupWeightTableProducer);
