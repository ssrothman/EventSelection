#include "CommonTools/Utils/interface/StringCutObjectSelector.h"
#include "CommonTools/Utils/interface/StringObjectFunction.h"
#include "DataFormats/Common/interface/TriggerResults.h"
#include "DataFormats/Math/interface/deltaR.h"
#include "DataFormats/NanoAOD/interface/FlatTable.h"
#include "DataFormats/PatCandidates/interface/Electron.h"
#include "DataFormats/PatCandidates/interface/Muon.h"
#include "DataFormats/PatCandidates/interface/Photon.h"
#include "DataFormats/PatCandidates/interface/TriggerObjectStandAlone.h"
#include "FWCore/Common/interface/TriggerNames.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/global/EDProducer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "FWCore/Utilities/interface/RegexMatch.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Extension of the table `name` (one row per candidate in src): each candidate is matched to the nearest same-charge
// object in `objects` within matchDeltaR (no match is an error), and `variables` (PAT string expressions; a cut for
// bool columns) are evaluated on it. Built-in columns: undressedPt (the matched object's pt) and, if triggerPaths is not
// empty, triggerMatched (the candidate is within triggerMatchDeltaR of an HLT object from the last filter of a fired
// path matching triggerPaths).
template <typename T>
class ObjectExtensionTableProducer : public edm::global::EDProducer<> {
public:
  explicit ObjectExtensionTableProducer(const edm::ParameterSet& cfg)
      : src_(consumes(cfg.getParameter<edm::InputTag>("src"))),
        objects_(consumes(cfg.getParameter<edm::InputTag>("objects"))),
        put_(produces()),
        name_(cfg.getParameter<std::string>("name")),
        matchDR_(cfg.getParameter<double>("matchDeltaR")),
        paths_(cfg.getParameter<std::vector<std::string>>("triggerPaths")),
        trigDR_(cfg.getParameter<double>("triggerMatchDeltaR")) {
    if (!paths_.empty()) {
      results_ = consumes(cfg.getParameter<edm::InputTag>("triggerResults"));
      trigObjects_ = consumes(cfg.getParameter<edm::InputTag>("triggerObjects"));
    }
    for (const auto& v : cfg.getParameter<std::vector<edm::ParameterSet>>("variables")) {
      const auto expr = v.getParameter<std::string>("expr");
      Column c{v.getParameter<std::string>("name"),
               v.getParameter<std::string>("doc"),
               v.getParameter<std::string>("type"),
               v.getParameter<int>("precision"),
               nullptr,
               nullptr};
      if (c.type == "bool")
        c.cut = std::make_shared<StringCutObjectSelector<T>>(expr);
      else if (c.type == "float" || c.type == "int" || c.type == "int16" || c.type == "uint8")
        c.f = std::make_shared<StringObjectFunction<T>>(expr);
      else
        throw cms::Exception("Configuration") << "ObjectExtensionTableProducer: column " << c.name << ": type "
                                              << c.type << " is not float, int, int16, uint8 or bool";
      if (c.name == "undressedPt" || c.name == "triggerMatched")
        throw cms::Exception("Configuration") << "ObjectExtensionTableProducer: " << c.name << " is a built-in column";
      columns_.push_back(std::move(c));
    }
  }

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::InputTag>("src", edm::InputTag("ZmumuSel", "leptons"))
        ->setComment("candidates of the table to extend");
    desc.add<edm::InputTag>("objects", edm::InputTag("slimmedMuons"))->setComment("PAT objects to match to");
    desc.add<std::string>("name", "ZmumuLeptons");
    desc.add<double>("matchDeltaR", 0.1);
    desc.add<std::vector<std::string>>("triggerPaths", {})->setComment("HLT path globs; empty: no triggerMatched");
    desc.add<edm::InputTag>("triggerResults", edm::InputTag("TriggerResults", "", "HLT"));
    desc.add<edm::InputTag>("triggerObjects", edm::InputTag("slimmedPatTrigger"));
    desc.add<double>("triggerMatchDeltaR", 0.1);
    edm::ParameterSetDescription var;
    var.add<std::string>("name");
    var.add<std::string>("expr");
    var.add<std::string>("type")->setComment("float | int | int16 | uint8 | bool (expr is then a cut)");
    var.add<std::string>("doc");
    var.add<int>("precision")->setComment("float mantissa bits; -1 = full float");
    desc.addVPSet("variables", var, {});
    descriptions.addWithDefaultLabel(desc);
  }

  void produce(edm::StreamID, edm::Event& event, const edm::EventSetup&) const override {
    const auto& cands = event.get(src_);
    const auto& objects = event.get(objects_);
    std::vector<const T*> matched;
    std::vector<float> undressed;
    for (const auto& c : cands) {
      const T* best = nullptr;
      double bestDR2 = matchDR_ * matchDR_;
      for (const auto& o : objects) {
        const double dr2 = reco::deltaR2(c, o);
        if (o.charge() == c.charge() && dr2 < bestDR2) {
          best = &o;
          bestDR2 = dr2;
        }
      }
      if (!best)
        throw cms::Exception("ObjectExtensionTableProducer")
            << name_ << ": no object within dR " << matchDR_ << " of a candidate (pt " << c.pt() << ", eta " << c.eta()
            << ")";
      matched.push_back(best);
      undressed.push_back(best->pt());
    }
    auto table = std::make_unique<nanoaod::FlatTable>(cands.size(), name_, false, true);
    for (const auto& c : columns_) {
      if (c.type == "float")
        fill<float>(*table, c, matched);
      else if (c.type == "int")
        fill<int>(*table, c, matched);
      else if (c.type == "int16")
        fill<std::int16_t>(*table, c, matched);
      else if (c.type == "uint8")
        fill<std::uint8_t>(*table, c, matched);
      else
        fill<bool>(*table, c, matched);
    }
    table->addColumn<float>("undressedPt", undressed, "pt of the matched object (before dressing)");
    if (!paths_.empty())
      table->addColumn<bool>(
          "triggerMatched", triggerMatched(event, cands), "matched to an HLT object of the channel's trigger");
    event.put(put_, std::move(table));
  }

private:
  struct Column {
    std::string name, doc, type;
    int precision;
    std::shared_ptr<StringObjectFunction<T>> f;
    std::shared_ptr<StringCutObjectSelector<T>> cut;
  };

  template <typename V>
  static void fill(nanoaod::FlatTable& t, const Column& c, const std::vector<const T*>& objs) {
    std::vector<V> v;
    for (const auto* o : objs)
      v.push_back(c.cut ? V((*c.cut)(*o)) : static_cast<V>((*c.f)(*o)));
    if constexpr (std::is_same_v<V, float>)
      t.addColumn<V>(c.name, v, c.doc, c.precision);
    else
      t.addColumn<V>(c.name, v, c.doc);
  }

  std::vector<bool> triggerMatched(const edm::Event& event, const edm::View<reco::Candidate>& cands) const {
    const auto& results = event.get(results_);
    const auto& names = event.triggerNames(results);
    std::vector<std::string> fired;
    for (const auto& glob : paths_)
      for (const auto& it : edm::regexMatch(names.triggerNames(), glob))
        if (results.accept(names.triggerIndex(*it)))
          fired.push_back(*it);
    std::vector<pat::TriggerObjectStandAlone> objs;
    for (auto o : event.get(trigObjects_)) {
      o.unpackPathNames(names);
      if (std::any_of(fired.begin(), fired.end(), [&](const auto& p) { return o.hasPathName(p, true, true); }))
        objs.push_back(o);
    }
    std::vector<bool> out;
    for (const auto& c : cands)
      out.push_back(std::any_of(objs.begin(), objs.end(), [&](const auto& o) {
        return reco::deltaR2(c.eta(), c.phi(), o.eta(), o.phi()) < trigDR_ * trigDR_;
      }));
    return out;
  }

  const edm::EDGetTokenT<edm::View<reco::Candidate>> src_;
  const edm::EDGetTokenT<edm::View<T>> objects_;
  const edm::EDPutTokenT<nanoaod::FlatTable> put_;
  const std::string name_;
  const double matchDR_;
  const std::vector<std::string> paths_;
  const double trigDR_;
  edm::EDGetTokenT<edm::TriggerResults> results_;
  edm::EDGetTokenT<std::vector<pat::TriggerObjectStandAlone>> trigObjects_;
  std::vector<Column> columns_;
};

using MuonExtensionTableProducer = ObjectExtensionTableProducer<pat::Muon>;
using ElectronExtensionTableProducer = ObjectExtensionTableProducer<pat::Electron>;
using PhotonExtensionTableProducer = ObjectExtensionTableProducer<pat::Photon>;
DEFINE_FWK_MODULE(MuonExtensionTableProducer);
DEFINE_FWK_MODULE(ElectronExtensionTableProducer);
DEFINE_FWK_MODULE(PhotonExtensionTableProducer);
