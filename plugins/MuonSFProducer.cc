#include "DataFormats/Common/interface/TriggerResults.h"
#include "DataFormats/Math/interface/deltaR.h"
#include "DataFormats/NanoAOD/interface/FlatTable.h"
#include "DataFormats/PatCandidates/interface/Muon.h"
#include "DataFormats/PatCandidates/interface/TriggerObjectStandAlone.h"
#include "FWCore/Common/interface/TriggerNames.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Framework/interface/global/EDProducer.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/Utilities/interface/Exception.h"
#include "FWCore/Utilities/interface/RegexMatch.h"
#include "correction.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Muon efficiency scale factors of one channel as an event weight (double) and a singleton NanoAOD table `name` with
// columns nominal and <term>_{stat,syst}{Up,Down} (the event weight with that term's SFs shifted by their stat or syst
// uncertainty); all are 1 for events failing the channel selection `selected`. Each lepton in src is matched to the
// nearest same-charge muon in `muons` (which carries userInt highPtRegime); its pt and eta are used, and only the terms
// of its regime apply.
//  - Per-muon terms: the event weight is the product of the SFs.
//  - Trigger terms (at most one per regime, with HLT path globs `paths`): only the triggering muon gets a trigger SF,
//    i.e. the highest-pt muon matched within triggerMatchDeltaR to an HLT object of the last filter of a fired path of
//    its regime's trigger term. Without such a muon there is no trigger SF, with a warning if a path of the trigger
//    term of a selected muon's regime fired.
// outOfRange: a muon outside a term's binning raises an error, or with "skip" has SF 1.
class MuonSFProducer : public edm::global::EDProducer<> {
public:
  explicit MuonSFProducer(const edm::ParameterSet& cfg)
      : selected_(consumes(cfg.getParameter<edm::InputTag>("selected"))),
        src_(consumes(cfg.getParameter<edm::InputTag>("src"))),
        muons_(consumes(cfg.getParameter<edm::InputTag>("muons"))),
        weight_(produces()),
        table_(produces()),
        name_(cfg.getParameter<std::string>("name")),
        matchDR_(cfg.getParameter<double>("matchDeltaR")),
        trigMatchDR_(cfg.getParameter<double>("triggerMatchDeltaR")) {
    std::map<std::string, std::shared_ptr<const correction::CorrectionSet>> files;
    bool trigger[2] = {false, false};
    for (const auto& t : cfg.getParameter<std::vector<edm::ParameterSet>>("terms")) {
      Term term;
      term.name = t.getParameter<std::string>("name");
      const auto regime = t.getParameter<std::string>("regime");
      if (regime != "medium" && regime != "high")
        throw cms::Exception("Configuration")
            << "MuonSFProducer: term " << term.name << ": regime must be medium or high";
      term.regime = regime == "high";
      term.trigger = t.getParameter<bool>("trigger");
      const auto oor = t.getParameter<std::string>("outOfRange");
      if (oor != "error" && oor != "skip")
        throw cms::Exception("Configuration")
            << "MuonSFProducer: term " << term.name << ": outOfRange must be error or skip";
      term.skip = oor == "skip";
      if (term.trigger && trigger[term.regime])
        throw cms::Exception("Configuration") << "MuonSFProducer: more than one trigger term for regime " << regime;
      trigger[term.regime] |= term.trigger;
      term.paths = t.getParameter<std::vector<std::string>>("paths");
      if (term.trigger == term.paths.empty())
        throw cms::Exception("Configuration")
            << "MuonSFProducer: term " << term.name << ": paths must be set exactly for trigger terms";
      for (const auto& other : terms_)
        if (other.name == term.name)
          throw cms::Exception("Configuration") << "MuonSFProducer: duplicate term name " << term.name;
      const auto file = t.getParameter<std::string>("file");
      auto& cs = files[file];
      if (!cs)
        cs = correction::CorrectionSet::from_file(file);
      term.corr = cs->at(t.getParameter<std::string>("key"));
      for (const auto& in : term.corr->inputs()) {
        const auto& n = in.name();
        if (n == "eta" || n == "abseta" || n == "pt" || n == "p" || n == "scale_factors")
          term.inputs.push_back(n);
        else
          throw cms::Exception("Configuration") << "MuonSFProducer: term " << term.name << ": unsupported input " << n;
      }
      terms_.push_back(std::move(term));
    }
    if (trigger[0] || trigger[1]) {
      trigResults_ = consumes(cfg.getParameter<edm::InputTag>("triggerResults"));
      trigObjects_ = consumes(cfg.getParameter<edm::InputTag>("triggerObjects"));
    }
  }

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    edm::ParameterSetDescription desc;
    desc.add<edm::InputTag>("selected", edm::InputTag("Zmumu"))->setComment("the channel's event selection flag");
    desc.add<edm::InputTag>("src", edm::InputTag("ZmumuSel", "leptons"))->setComment("the channel's selected leptons");
    desc.add<edm::InputTag>("muons", edm::InputTag("correctedMuons"))->setComment("muons with userInt highPtRegime");
    desc.add<std::string>("name", "ZmumuMuonSF")->setComment("NanoAOD table name");
    desc.add<double>("matchDeltaR", 0.1);
    desc.add<edm::InputTag>("triggerResults", edm::InputTag("TriggerResults", "", "HLT"));
    desc.add<edm::InputTag>("triggerObjects", edm::InputTag("slimmedPatTrigger"));
    desc.add<double>("triggerMatchDeltaR", 0.1);
    edm::ParameterSetDescription term;
    term.add<std::string>("name");
    term.add<std::string>("regime")->setComment("medium | high");
    term.add<std::string>("file");
    term.add<std::string>("key");
    term.add<bool>("trigger");
    term.add<std::vector<std::string>>("paths")->setComment("trigger terms only: HLT path globs");
    term.add<std::string>("outOfRange")->setComment("error | skip");
    desc.addVPSet("terms", term, {});
    descriptions.addWithDefaultLabel(desc);
  }

  void produce(edm::StreamID, edm::Event& event, const edm::EventSetup&) const override {
    const auto& muons = event.get(muons_);
    std::vector<Kin> kin;
    if (event.get(selected_))
      for (const auto& l : event.get(src_)) {
        const pat::Muon* best = nullptr;
        double bestDR2 = matchDR_ * matchDR_;
        for (const auto& m : muons) {
          const double dr2 = reco::deltaR2(l, m);
          if (m.charge() == l.charge() && dr2 < bestDR2) {
            best = &m;
            bestDR2 = dr2;
          }
        }
        if (!best)
          throw cms::Exception("MuonSFProducer")
              << name_ << ": no muon within dR " << matchDR_ << " of a selected lepton (pt " << l.pt() << ", eta "
              << l.eta() << ")";
        if (!best->hasUserInt("highPtRegime"))
          throw cms::Exception("MuonSFProducer") << name_ << ": muons lack userInt highPtRegime";
        kin.push_back({best->pt(), best->eta(), best->phi(), best->userInt("highPtRegime") != 0});
      }
    const auto [trigMuon, trigTerm] = triggeringMuon(event, kin);
    // Shift of term `shifted` by `sign` times its `unc` uncertainty; shifted < 0 is the nominal weight.
    auto weight = [&](int shifted, const char* unc, int sign) {
      double w = 1;
      for (std::size_t m = 0; m < kin.size(); ++m)
        for (std::size_t t = 0; t < terms_.size(); ++t) {
          const auto& term = terms_[t];
          if (term.regime != kin[m].high || (term.trigger && !(int(m) == trigMuon && int(t) == trigTerm)))
            continue;
          double sf;
          if (!evaluate(term, kin[m], "nominal", sf))
            continue;
          if (int(t) == shifted) {
            double u;
            evaluate(term, kin[m], unc, u);
            sf += sign * u;
          }
          w *= sf;
        }
      return w;
    };
    auto table = std::make_unique<nanoaod::FlatTable>(1, name_, true);
    const double nominal = weight(-1, "", 0);
    table->addColumnValue<float>("nominal", nominal, "muon SF event weight");
    for (std::size_t t = 0; t < terms_.size(); ++t)
      for (const char* unc : {"stat", "syst"})
        for (int sign : {+1, -1}) {
          const std::string col = terms_[t].name + "_" + unc + (sign > 0 ? "Up" : "Down");
          table->addColumnValue<float>(
              col,
              weight(t, unc, sign),
              "event weight with " + terms_[t].name + " SFs shifted by their " + unc + " uncertainty");
        }
    event.emplace(weight_, nominal);
    event.put(table_, std::move(table));
  }

private:
  struct Term {
    std::string name;
    bool regime = false, trigger = false, skip = false;
    correction::Correction::Ref corr;
    std::vector<std::string> inputs, paths;
  };
  struct Kin {
    double pt, eta, phi;
    bool high;
  };

  // (muon index, term index) of the triggering muon, or (-1, -1) if there is none.
  std::pair<int, int> triggeringMuon(const edm::Event& event, const std::vector<Kin>& kin) const {
    if (trigResults_.isUninitialized() || kin.empty())
      return {-1, -1};
    const auto& results = event.get(trigResults_);
    const auto& names = event.triggerNames(results);
    std::vector<std::vector<std::string>> fired(terms_.size());
    for (std::size_t t = 0; t < terms_.size(); ++t)
      for (const auto& glob : terms_[t].paths)
        for (const auto& it : edm::regexMatch(names.triggerNames(), glob))
          if (results.accept(names.triggerIndex(*it)))
            fired[t].push_back(*it);
    std::vector<pat::TriggerObjectStandAlone> objects(event.get(trigObjects_).begin(), event.get(trigObjects_).end());
    for (auto& o : objects)
      o.unpackPathNames(names);
    std::pair<int, int> best{-1, -1};
    bool anyFired = false;
    for (std::size_t m = 0; m < kin.size(); ++m)
      for (std::size_t t = 0; t < terms_.size(); ++t) {
        if (!terms_[t].trigger || terms_[t].regime != kin[m].high)
          continue;
        anyFired |= !fired[t].empty();
        if (best.first >= 0 && kin[m].pt <= kin[best.first].pt)
          continue;
        for (const auto& o : objects)
          if (reco::deltaR2(kin[m].eta, kin[m].phi, o.eta(), o.phi()) < trigMatchDR_ * trigMatchDR_ &&
              std::any_of(
                  fired[t].begin(), fired[t].end(), [&](const auto& p) { return o.hasPathName(p, true, true); })) {
            best = {int(m), int(t)};
            break;
          }
      }
    if (anyFired && best.first < 0)
      edm::LogWarning("MuonSFProducer")
          << name_ << ": " << event.id()
          << ": a trigger path fired but no selected muon matches its HLT objects; no trigger SF";
    return best;
  }

  // False if the muon is outside the term's binning and the term skips such muons.
  bool evaluate(const Term& term, const Kin& k, const std::string& what, double& out) const {
    std::vector<correction::Variable::Type> args;
    for (const auto& n : term.inputs) {
      if (n == "eta")
        args.emplace_back(k.eta);
      else if (n == "abseta")
        args.emplace_back(std::abs(k.eta));
      else if (n == "pt")
        args.emplace_back(k.pt);
      else if (n == "p")
        args.emplace_back(k.pt * std::cosh(k.eta));
      else
        args.emplace_back(what);
    }
    try {
      out = term.corr->evaluate(args);
    } catch (const std::exception& e) {
      // correctionlib reports binning overflow as "Index below/above bounds"; anything else is a real error.
      if (term.skip && std::string(e.what()).find("bounds") != std::string::npos)
        return false;
      throw cms::Exception("MuonSFProducer") << name_ << ": term " << term.name << " (" << what << ", pt " << k.pt
                                             << ", eta " << k.eta << "): " << e.what();
    }
    return true;
  }

  const edm::EDGetTokenT<bool> selected_;
  const edm::EDGetTokenT<edm::View<reco::Candidate>> src_;
  const edm::EDGetTokenT<edm::View<pat::Muon>> muons_;
  const edm::EDPutTokenT<double> weight_;
  const edm::EDPutTokenT<nanoaod::FlatTable> table_;
  const std::string name_;
  const double matchDR_, trigMatchDR_;
  edm::EDGetTokenT<edm::TriggerResults> trigResults_;
  edm::EDGetTokenT<std::vector<pat::TriggerObjectStandAlone>> trigObjects_;
  std::vector<Term> terms_;
};

DEFINE_FWK_MODULE(MuonSFProducer);
