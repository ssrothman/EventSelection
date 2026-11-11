#include "DataFormats/NanoAOD/interface/FlatTable.h"
#include "FWCore/Framework/interface/ConsumesCollector.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/global/EDFilter.h"
#include "FWCore/Framework/interface/global/EDProducer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "FWCore/Utilities/interface/transform.h"

#include <vector>

namespace {
  using Tokens = std::vector<edm::EDGetTokenT<bool>>;

  Tokens tokens(const edm::ParameterSet& cfg, edm::ConsumesCollector cc) {
    auto t = edm::vector_transform(cfg.getParameter<std::vector<edm::InputTag>>("srcs"),
                                   [&](const edm::InputTag& tag) { return cc.template consumes<bool>(tag); });
    if (t.empty())
      throw cms::Exception("Configuration") << "event flag module: srcs is empty";
    return t;
  }

  bool parseMode(const std::string& m) {
    if (m == "and")
      return true;
    if (m == "or")
      return false;
    throw cms::Exception("Configuration") << "event flag module: mode must be 'and' or 'or', got " << m;
  }

  bool combine(const edm::Event& ev, const Tokens& ts, bool isAnd) {
    bool r = isAnd;
    for (const auto& t : ts)
      r = isAnd ? (r && ev.get(t)) : (r || ev.get(t));
    return r;
  }

  void describe(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<std::vector<edm::InputTag>>("srcs", {});
    desc.add<std::string>("mode", "and")->setComment("and | or");
    descriptions.addWithDefaultLabel(desc);
  }
}  // namespace

// bool = AND / OR of the bool products in srcs.
class FlagCombiner : public edm::global::EDProducer<> {
public:
  explicit FlagCombiner(const edm::ParameterSet& cfg)
      : srcs_(tokens(cfg, consumesCollector())), isAnd_(parseMode(cfg.getParameter<std::string>("mode"))), put_(produces()) {}
  static void fillDescriptions(edm::ConfigurationDescriptions& d) { describe(d); }
  void produce(edm::StreamID, edm::Event& ev, const edm::EventSetup&) const override {
    ev.emplace(put_, combine(ev, srcs_, isAnd_));
  }

private:
  const Tokens srcs_;
  const bool isAnd_;
  const edm::EDPutTokenT<bool> put_;
};

// Filter on the AND / OR of the bool products in srcs.
class EventFlagFilter : public edm::global::EDFilter<> {
public:
  explicit EventFlagFilter(const edm::ParameterSet& cfg)
      : srcs_(tokens(cfg, consumesCollector())), isAnd_(parseMode(cfg.getParameter<std::string>("mode"))) {}
  static void fillDescriptions(edm::ConfigurationDescriptions& d) { describe(d); }
  bool filter(edm::StreamID, edm::Event& ev, const edm::EventSetup&) const override {
    return combine(ev, srcs_, isAnd_);
  }

private:
  const Tokens srcs_;
  const bool isAnd_;
};

// Singleton NanoAOD table `name` with one bool column per (names[i], srcs[i]).
class EventFlagTableProducer : public edm::global::EDProducer<> {
public:
  explicit EventFlagTableProducer(const edm::ParameterSet& cfg)
      : srcs_(tokens(cfg, consumesCollector())),
        names_(cfg.getParameter<std::vector<std::string>>("names")),
        name_(cfg.getParameter<std::string>("name")),
        put_(produces()) {
    if (names_.size() != srcs_.size())
      throw cms::Exception("Configuration") << "EventFlagTableProducer: names and srcs differ in length";
  }
  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<std::vector<edm::InputTag>>("srcs", {});
    desc.add<std::vector<std::string>>("names", {});
    desc.add<std::string>("name", "Selection");
    descriptions.addWithDefaultLabel(desc);
  }
  void produce(edm::StreamID, edm::Event& ev, const edm::EventSetup&) const override {
    auto t = std::make_unique<nanoaod::FlatTable>(1, name_, true);
    for (std::size_t i = 0; i < srcs_.size(); ++i)
      t->addColumnValue<bool>(names_[i], ev.get(srcs_[i]), "event selection flag");
    ev.put(put_, std::move(t));
  }

private:
  const Tokens srcs_;
  const std::vector<std::string> names_;
  const std::string name_;
  const edm::EDPutTokenT<nanoaod::FlatTable> put_;
};

DEFINE_FWK_MODULE(FlagCombiner);
DEFINE_FWK_MODULE(EventFlagFilter);
DEFINE_FWK_MODULE(EventFlagTableProducer);
