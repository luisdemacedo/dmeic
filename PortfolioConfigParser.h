#ifndef PORTFOLIO_CONFIG_PARSER_H
#define PORTFOLIO_CONFIG_PARSER_H

#include "clausesharing/AllClausesHeuristic.h"
#include "clausesharing/IClauseSharingHeuristic.h"
#include "clausesharing/SizeHeuristic.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <regex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "DebugLog.h"

namespace PortfolioConfigParser {
typedef void (*ParseOptionsFn)(int &argc, char **argv, bool strict);
inline int parseInt(const std::string &value);

using PortfolioSolverConfig = std::unordered_map<std::string, std::string>;
using PortfolioConfig =
    std::vector<PortfolioSolverConfig>; // vector of solver config

PortfolioConfig parsePortfolioConfig(const std::string &path) {
  std::ifstream file(path.c_str());
  PortfolioConfig config;
  if (!file) {
    throw std::runtime_error("failed to open portfolio config: " + path);
  }

  std::string line;

  while (std::getline(file, line)) {

    std::size_t comment = line.find('#');
    if (comment != std::string::npos) {
      line = line.substr(0, comment);
    }

    if (line.empty()) {
      continue;
    }

    std::regex re(R"(^solver(\d+)\.([A-Za-z0-9_-]+)=(.*)$)");
    std::smatch m;
    if (std::regex_match(line, m, re)) {
      int solverIndex = std::stoul(m[1].str());
      if (solverIndex == 0) {
        throw std::runtime_error(
            "solver index must be greater than 0 in portfolio config");
      }

      --solverIndex;
      std::string key = m[2].str();
      std::string value = m[3].str();
      if (solverIndex >= config.size())
        config.resize(solverIndex + 1);

      config[solverIndex][key] = value;
    }
  }

  DLOG(LogCategory::General, stdout, "[portfolio-config] parsed:\n");
  DLOG(LogCategory::General, stdout, "solvers:\n");
  for (std::size_t i = 0; i < config.size(); ++i) {
    auto &solver = config[i];
    for (std::unordered_map<std::string, std::string>::const_iterator it =
             solver.begin();
         it != solver.end(); ++it)
      DLOG(LogCategory::General, stdout, "solver%zu.%s = %s\n", i + 1,
           it->first.c_str(), it->second.c_str());
  }
  return config;
}

clausesharing::IClauseSharingHeuristic *
parseClauseSharingHeuristic(const PortfolioSolverConfig &solverConfig) {

  if (!solverConfig.contains("sharing_heuristic"))
    return new clausesharing::AllClausesHeuristic();

  std::string heuristicName = solverConfig.at("sharing_heuristic");
  if (heuristicName == "none")
    return new clausesharing::AllClausesHeuristic();

  else if (heuristicName == "size") {
    if (solverConfig.contains("sharing_size_cutoff"))
      return new clausesharing::SizeHeuristic(
          std::atoi(solverConfig.at("sharing_size_cutoff").c_str()));
    else
      return new clausesharing::SizeHeuristic();
  }

  return new clausesharing::AllClausesHeuristic();
}

} // namespace PortfolioConfigParser

#endif
