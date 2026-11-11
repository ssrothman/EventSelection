#include "DataFormats/Common/interface/TriggerResults.h"
#include "FWCore/Common/interface/TriggerNames.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/global/EDProducer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "FWCore/Utilities/interface/RegexMatch.h"

// bool = OR (or AND, with requireAll) over the trigger paths matching `paths` (glob patterns, e.g. HLT_IsoMu24_v*).
// A pattern matching no path in the event's menu throws, unless allowMissing (then it counts as failed).
class TriggerFlagProducer : public edm::global::EDProducer<> {
public:
  explicit TriggerFlagProducer(const edm::ParameterSet& cfg)
      : src_(consumes(cfg.getParameter<edm::InputTag>("src"))),
        put_(produces()),
        patterns_(cfg.getParameter<std::vector<std::string>>("paths")),
        requireAll_(cfg.getParameter<bool>("requireAll")),
        allowMissing_(cfg.getParameter<bool>("allowMissing")) {
    if (patterns_.empty())
      throw cms::Exception("Configuration") << "TriggerFlagProducer: no paths";
  }

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::InputTag>("src", edm::InputTag("TriggerResults", "", "HLT"));
    desc.add<std::vector<std::string>>("paths", {});
    desc.add<bool>("requireAll", false);
    desc.add<bool>("allowMissing", false);
    descriptions.addWithDefaultLabel(desc);
  }

  void produce(edm::StreamID, edm::Event& event, const edm::EventSetup&) const override {
    const auto& tr = event.get(src_);
    const auto& names = event.triggerNames(tr);
    bool any = false, all = true;
    for (const auto& pat : patterns_) {
      const auto matches = edm::regexMatch(names.triggerNames(), pat);
      if (matches.empty() && !allowMissing_)
        throw cms::Exception("TriggerFlagProducer") << "no trigger path matches '" << pat << "'";
      bool fired = false;
      for (const auto& m : matches)
        fired |= tr.accept(names.triggerIndex(*m));
      any |= fired;
      all &= fired;
    }
    event.emplace(put_, requireAll_ ? all : any);
  }

private:
  const edm::EDGetTokenT<edm::TriggerResults> src_;
  const edm::EDPutTokenT<bool> put_;
  const std::vector<std::string> patterns_;
  const bool requireAll_, allowMissing_;
};

DEFINE_FWK_MODULE(TriggerFlagProducer);
