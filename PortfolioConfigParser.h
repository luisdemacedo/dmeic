#ifndef PORTFOLIO_CONFIG_PARSER_H
#define PORTFOLIO_CONFIG_PARSER_H

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "DebugLog.h"

namespace openwbo {
class PortfolioMO;
}

namespace PortfolioConfigParser {
typedef void (*ParseOptionsFn)(int &argc, char **argv, bool strict);
inline int parseInt(const std::string &value);
openwbo::PortfolioMO *createPortfolioMO(const char *filename, int &argc,
                                        char **argv);
} // namespace PortfolioConfigParser

struct PortfolioSolverConfig {
  std::string type;
  std::unordered_map<std::string, std::string> args;

  int getIntOr(const std::string &key, int fallback) const {
    std::unordered_map<std::string, std::string>::const_iterator it =
        args.find(key);
    if (it == args.end())
      return fallback;
    return PortfolioConfigParser::parseInt(it->second);
  }
};

struct PortfolioConfig {
  std::unordered_map<std::string, std::string> global;
  std::vector<PortfolioSolverConfig> solvers;
};

namespace PortfolioConfigParser {

inline std::string trim(std::string value) {
  std::string::iterator first =
      std::find_if(value.begin(), value.end(),
                   [](unsigned char c) { return !std::isspace(c); });

  std::string::reverse_iterator last =
      std::find_if(value.rbegin(), value.rend(),
                   [](unsigned char c) { return !std::isspace(c); });

  if (first == value.end()) {
    return "";
  }

  return std::string(first, last.base());
}

inline bool parseBool(const std::string &value) {
  if (value == "true" || value == "1" || value == "yes" || value == "on") {
    return true;
  }

  if (value == "false" || value == "0" || value == "no" || value == "off") {
    return false;
  }

  throw std::runtime_error("invalid bool value: " + value);
}

inline int parseInt(const std::string &value) {
  std::size_t parsed = 0;
  int result = std::stoi(value, &parsed, 10);

  if (parsed != value.size()) {
    throw std::runtime_error("invalid int value: " + value);
  }

  return result;
}

inline const std::string &
require(const std::unordered_map<std::string, std::string> &values,
        const std::string &key) {
  std::unordered_map<std::string, std::string>::const_iterator it =
      values.find(key);

  if (it == values.end()) {
    throw std::runtime_error("missing required config key: " + key);
  }

  return it->second;
}

inline std::string normalizeGlobalPortfolioKey(const std::string &key) {
  if (key == "output_file")
    return "save-my-output";
  if (key == "stop_on_first_result")
    return "stop-on-first-result";
  if (key == "share_clauses")
    return "share-clauses";
  if (key == "share_solutions")
    return "share-solutions";
  if (key == "print_model")
    return "print-model";
  return key;
}

inline std::string normalizeSolverPortfolioKey(const std::string &key) {
  if (key == "weight_strategy")
    return "weight-strategy";
  if (key == "partition_strategy")
    return "partition-strategy";
  return key;
}

inline void parseGlobalPortfolioOptions(
    const std::unordered_map<std::string, std::string> &global,
    ParseOptionsFn parseOptionsFn) {
  std::vector<char *> params;
  params.push_back(strdup("dummy")); // parseOptions skips argv[0].

  for (std::unordered_map<std::string, std::string>::const_iterator it =
           global.begin();
       it != global.end(); ++it) {
    std::string key = normalizeGlobalPortfolioKey(it->first);
    const std::string &value = it->second;

    if (key == "workers")
      continue;

    if (key == "stop-on-first-result" || key == "share-clauses" ||
        key == "share-solutions" || key == "print-model") {
      if (parseBool(value)) {
        params.push_back(strdup(("-" + key).c_str()));
      } else {
        params.push_back(strdup(("-no-" + key).c_str()));
      }
      continue;
    }

    params.push_back(
        strdup((std::string("-") + key + std::string("=") + value).c_str()));
  }

  int params_count = params.size();
  parseOptionsFn(params_count, params.data(), false);

  for (char *param : params) {
    if (strncmp(param, "-save-my-output", 15) != 0)
      free(param);
  }
}

inline PortfolioConfig parsePortfolioConfig(const std::string &path) {
  std::ifstream file(path.c_str());
  if (!file) {
    throw std::runtime_error("failed to open portfolio config: " + path);
  }

  PortfolioConfig config;
  PortfolioSolverConfig *currentSolver = 0;

  std::string line;
  int lineNo = 0;

  while (std::getline(file, line)) {
    ++lineNo;

    std::size_t comment = line.find('#');
    if (comment != std::string::npos) {
      line = line.substr(0, comment);
    }

    line = trim(line);
    if (line.empty()) {
      continue;
    }

    if (line == "portfolio:") {
      currentSolver = 0;
      continue;
    }

    if (line == "[solver]" || line == "solver:") {
      config.solvers.push_back(PortfolioSolverConfig());
      currentSolver = &config.solvers.back();
      continue;
    }

    if (line == "end_solver") {
      currentSolver = 0;
      continue;
    }

    std::size_t eq = line.find('=');
    if (eq == std::string::npos) {
      eq = line.find(':');
    }
    if (eq == std::string::npos) {
      throw std::runtime_error("line " + std::to_string(lineNo) +
                               ": expected key=value");
    }

    std::string key = trim(line.substr(0, eq));
    std::string value = trim(line.substr(eq + 1));

    if (key.empty()) {
      throw std::runtime_error("line " + std::to_string(lineNo) +
                               ": empty key");
    }

    if (currentSolver != 0) {
      key = normalizeSolverPortfolioKey(key);
      if (key == "type" || key == "algorithm") {
        currentSolver->type = value;
      } else {
        currentSolver->args[key] = value;
      }
    } else {
      config.global[key] = value;
    }
  }

  for (std::size_t i = 0; i < config.solvers.size(); ++i) {
    if (config.solvers[i].type.empty()) {
      throw std::runtime_error("solver " + std::to_string(i) +
                               " missing required key: type");
    }
  }

  DLOG(stdout, "[portfolio-config] parsed:\n");
  DLOG(stdout, "  global:\n");
  for (std::unordered_map<std::string, std::string>::const_iterator it =
           config.global.begin();
       it != config.global.end(); ++it) {
    DLOG(stdout, "    %s = %s\n", it->first.c_str(), it->second.c_str());
  }
  DLOG(stdout, "  solvers:\n");
  for (std::size_t i = 0; i < config.solvers.size(); ++i) {
    const PortfolioSolverConfig &solver = config.solvers[i];
    DLOG(stdout, "  type = %s\n", solver.type.c_str());
    DLOG(stdout, "  args:\n");
    for (std::unordered_map<std::string, std::string>::const_iterator it =
             solver.args.begin();
         it != solver.args.end(); ++it) {
      DLOG(stdout, "    %s = %s\n", it->first.c_str(), it->second.c_str());
    }
  }
  return config;
}

} // namespace PortfolioConfigParser

#endif
