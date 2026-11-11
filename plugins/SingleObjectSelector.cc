#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/global/EDProducer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/transform.h"
#include "SRothman/EventSelection/plugins/Common.h"

// gamma + jets style: nMin <= (objects in src) <= nMax and no object in vetoSrcs. Products: bool and
// "objects" (the leading object if the event passes).
class SingleObjectEventSelector : public edm::global::EDProducer<> {
public:
  explicit SingleObjectEventSelector(const edm::ParameterSet& cfg)
      : src_(consumes(cfg.getParameter<edm::InputTag>("src"))),
        vetoes_(edm::vector_transform(cfg.getParameter<std::vector<edm::InputTag>>("vetoSrcs"),
                                      [this](const edm::InputTag& t) { return consumes<edm::View<reco::Candidate>>(t); })),
        flag_(produces()),
        objects_(produces("objects")),
        nMin_(cfg.getParameter<unsigned>("nMin")),
        nMax_(cfg.getParameter<unsigned>("nMax")) {}

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::InputTag>("src", edm::InputTag("tightPhotons"));
    desc.add<std::vector<edm::InputTag>>("vetoSrcs", {});
    desc.add<unsigned>("nMin", 1);
    desc.add<unsigned>("nMax", 1);
    descriptions.addWithDefaultLabel(desc);
  }

  void produce(edm::StreamID, edm::Event& event, const edm::EventSetup&) const override {
    const auto& objs = event.get(src_);
    bool pass = objs.size() >= nMin_ && objs.size() <= nMax_;
    for (const auto& t : vetoes_)
      pass &= event.get(t).empty();
    std::vector<reco::LeafCandidate> out;
    if (pass && !objs.empty()) {
      std::size_t lead = 0;
      for (std::size_t i = 1; i < objs.size(); ++i)
        if (objs[i].pt() > objs[lead].pt())
          lead = i;
      out.push_back(evsel::copy(objs[lead]));
    }
    event.emplace(flag_, pass);
    event.emplace(objects_, std::move(out));
  }

private:
  const edm::EDGetTokenT<edm::View<reco::Candidate>> src_;
  const std::vector<edm::EDGetTokenT<edm::View<reco::Candidate>>> vetoes_;
  const edm::EDPutTokenT<bool> flag_;
  const edm::EDPutTokenT<std::vector<reco::LeafCandidate>> objects_;
  const unsigned nMin_, nMax_;
};

DEFINE_FWK_MODULE(SingleObjectEventSelector);
