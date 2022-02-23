/*
 * Copyright (c) 2020-2021 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProjMgr.h"
#include "ProjMgrParser.h"
#include "ProjMgrLogger.h"
#include "ProductInfo.h"
#include "RteFsUtils.h"

#include <algorithm>
#include <cxxopts.hpp>
#include <functional>

using namespace std;

static constexpr const char* USAGE = "\
Usage:\n\
  csolution <command> [<args>] [OPTIONS...]\n\n\
Commands:\n\
  list packs            Print list of installed packs\n\
       devices          Print list of available device names\n\
       components       Print list of available components\n\
       dependencies     Print list of unresolved project dependencies\n\
       contexts         Print list of contexts in a csolution.yml\n\
       generators       Print list of code generators of a given context\n\
  convert               Convert cproject.yml or csolution.yml in cprj files\n\
  run                   Run code generator\n\
  help                  Print usage\n\n\
Options:\n\
  -s, --solution arg    Input csolution.yml file\n\
  -p, --project arg     Input cproject.yml file\n\
  -c, --context arg     Input context name <cproject>[.<build-type>][+<target-type>]\n\
  -f, --filter arg      Filter words\n\
  -g, --generator arg   Code generator identifier\n\
  -n, --no-check-schema Skip schema check\n\
  -o, --output arg      Output directory\n\
  -h, --help            Print usage\n\
";

ProjMgr::ProjMgr(void) : m_checkSchema(false) {
  // Reserved
}

ProjMgr::~ProjMgr(void) {
  // Reserved
}

void ProjMgr::PrintUsage(void) {
  cout << PRODUCT_NAME << " " << VERSION_STRING << " " << COPYRIGHT_NOTICE << " " << endl;
  cout << USAGE << endl;
}

int ProjMgr::RunProjMgr(int argc, char **argv) {
  ProjMgr manager;

  // Command line options
  cxxopts::Options options(ORIGINAL_FILENAME);
  cxxopts::ParseResult parseResult;

  try {
    options.add_options()
      ("command", "", cxxopts::value<string>())
      ("args", "", cxxopts::value<string>())
      ("s,solution", "", cxxopts::value<string>())
      ("p,project", "", cxxopts::value<string>())
      ("c,context", "", cxxopts::value<string>())
      ("f,filter", "", cxxopts::value<string>())
      ("g,generator", "", cxxopts::value<string>())
      ("n,no-check-schema", "", cxxopts::value<bool>()->default_value("false"))
      ("o,output", "", cxxopts::value<string>())
      ("h,help", "")
      ;
    options.parse_positional({ "command", "args"});

    parseResult = options.parse(argc, argv);
    manager.m_checkSchema = !parseResult.count("n");

    if (parseResult.count("command")) {
      manager.m_command = parseResult["command"].as<string>();
    } else {
      // No command was given, print usage and return success
      manager.PrintUsage();
      return 0;
    }
    if (parseResult.count("args")) {
      manager.m_args = parseResult["args"].as<string>();
    }
    if (parseResult.count("solution")) {
      manager.m_csolutionFile = parseResult["solution"].as<string>();
      error_code ec;
      if (!fs::exists(manager.m_csolutionFile, ec)) {
        ProjMgrLogger::Error(manager.m_csolutionFile, "csolution file was not found");
        return 1;
      }
      manager.m_csolutionFile = fs::canonical(manager.m_csolutionFile, ec).generic_string();
      manager.m_rootDir = fs::path(manager.m_csolutionFile).parent_path().generic_string();
    }
    if (parseResult.count("project")) {
      manager.m_cprojectFile = parseResult["project"].as<string>();
      error_code ec;
      if (!fs::exists(manager.m_cprojectFile, ec)) {
        ProjMgrLogger::Error(manager.m_cprojectFile, "cproject file was not found");
        return 1;
      }
      manager.m_cprojectFile = fs::canonical(manager.m_cprojectFile, ec).generic_string();
      manager.m_rootDir = fs::path(manager.m_cprojectFile).parent_path().generic_string();
    }
    if (parseResult.count("context")) {
      manager.m_context = parseResult["context"].as<string>();
    }
    if (parseResult.count("filter")) {
      manager.m_filter = parseResult["filter"].as<string>();
    }
    if (parseResult.count("generator")) {
      manager.m_codeGenerator = parseResult["generator"].as<string>();
    }
    if (parseResult.count("output")) {
      manager.m_outputDir = parseResult["output"].as<string>();
      manager.m_outputDir = fs::path(manager.m_outputDir).generic_string();
    }
  } catch (cxxopts::OptionException& e) {
    ProjMgrLogger::Error(e.what());
    return 1;
  }

  // Unmatched items in the parse result
  if (!parseResult.unmatched().empty()) {
    ProjMgrLogger::Error("too many command line arguments");
    return 1;
  }

  // Parse commands
  if ((manager.m_command == "help") || parseResult.count("help")) {
    // Print usage
    manager.PrintUsage();
    return 0;
  } else if (manager.m_command == "list") {
    // Process 'list' command
    if (manager.m_args.empty()) {
      ProjMgrLogger::Error("list <args> was not specified");
      return 1;
    }
    // Process argument
    if (manager.m_args == "packs") {
      if (!manager.RunListPacks()) {
        return 1;
      }
    } else if (manager.m_args == "devices") {
      if (!manager.RunListDevices()) {
        return 1;
      }
    } else if (manager.m_args == "components") {
      if (!manager.RunListComponents()) {
        return 1;
      }
    } else if (manager.m_args == "dependencies") {
      if (!manager.RunListDependencies()) {
        return 1;
      }
    } else if (manager.m_args == "contexts") {
      if (!manager.RunListContexts()) {
        return 1;
      }
    } else if (manager.m_args == "generators") {
      if (!manager.RunListGenerators()) {
        return 1;
      }
    }
    else {
      ProjMgrLogger::Error("list <args> was not found");
      return 1;
    }
  } else if (manager.m_command == "convert") {
    // Process 'convert' command
    if (!manager.RunConvert()) {
      return 1;
    }
  } else if (manager.m_command == "run") {
    // Process 'run' command
    if (!manager.RunCodeGenerator()) {
      return 1;
    }
  } else {
    ProjMgrLogger::Error("<command> was not found");
    return 1;
  }
  return 0;
}

bool ProjMgr::PopulateContexts(void) {
  if (!m_csolutionFile.empty()) {
    // Parse csolution
    if (!m_parser.ParseCsolution(m_csolutionFile, m_checkSchema)) {
      return false;
    }
    // Parse cprojects
    for (const auto& cproject : m_parser.GetCsolution().cprojects) {
      error_code ec;
      string const& cprojectFile = fs::canonical(m_rootDir + "/" + cproject, ec).generic_string();
      if (cprojectFile.empty()) {
        ProjMgrLogger::Error(cproject, "cproject file was not found");
        return false;
      }
      if (!m_parser.ParseCproject(cprojectFile, m_checkSchema)) {
        return false;
      }
    }
  } else if (!m_cprojectFile.empty()) {
    // Parse single cproject
    if (!m_parser.ParseCproject(m_cprojectFile, m_checkSchema, true)) {
      return false;
    }
  } else {
    ProjMgrLogger::Error("input yml files were not specified");
    return false;
  }

  // Parse clayers
  for (const auto& cproject : m_parser.GetCprojects()) {
    string const& cprojectFile = cproject.first;
    for (const auto& clayer : cproject.second.clayers) {
      error_code ec;
      string const& clayerFile = fs::canonical(fs::path(cprojectFile).parent_path().append(clayer.layer), ec).generic_string();
      if (clayerFile.empty()) {
        ProjMgrLogger::Error(clayer.layer, "clayer file was not found");
        return false;
      }
      if (!m_parser.ParseClayer(clayerFile, m_checkSchema)) {
        return false;
      }
    }
  }

  // Set output directory
  m_worker.SetOutputDir(m_outputDir);

  // Add contexts
  for (auto& descriptor : m_parser.GetCsolution().contexts) {
    error_code ec;
    const string& cprojectFile = fs::path(descriptor.cproject).is_absolute() ?
      descriptor.cproject : fs::canonical(m_rootDir + "/" + descriptor.cproject, ec).generic_string();
    if (!m_worker.AddContexts(m_parser, descriptor, cprojectFile)) {
      return false;
    }
  }

  return true;
}

bool ProjMgr::RunConvert(void) {
  // Parse all input files and populate contexts inputs
  if (!PopulateContexts()) {
    return false;
  }

  // Process contexts
  map<string, ContextItem>* contexts = nullptr;
  m_worker.GetContexts(contexts);
  for (auto& [contextName, contextItem] : *contexts) {
    if (!m_worker.ProcessContext(contextItem, true)) {
      ProjMgrLogger::Error("processing context '" + contextName + "' failed");
      return false;
    }
  }

  // Generate Cprjs
  for (auto& [contextName, contextItem] : *contexts) {
    error_code ec;
    const string& directory = m_outputDir.empty() ? contextItem.directories.cproject : m_outputDir + "/" + contextName;
    const string& filename = fs::weakly_canonical(directory + "/" + contextName + ".cprj", ec).generic_string();
    RteFsUtils::CreateDirectories(directory);
    if (m_generator.GenerateCprj(contextItem, filename)) {
      ProjMgrLogger::Info(filename, "file generated successfully");
    } else {
      ProjMgrLogger::Error(filename, "file cannot be written");
      return false;
    }
    if (!m_worker.CopyContextFiles(contextItem, directory, m_outputDir.empty())) {
      ProjMgrLogger::Error("files cannot be copied into output directory '" + directory + "'");
      return false;
    }
  }
  return true;
}

bool ProjMgr::RunListPacks(void) {
  vector<string> packs;
  if (!m_worker.ListPacks(packs, m_filter)) {
    ProjMgrLogger::Error("processing pack list failed");
    return false;
  }
  for (const auto& pack : packs) {
    cout << pack << endl;
  }
  return true;
}

bool ProjMgr::RunListDevices(void) {
  if (!m_cprojectFile.empty()) {
    if (!m_parser.ParseCproject(m_cprojectFile, m_checkSchema, true)) {
      return false;
    }
    ContextItem context;
    ContextDesc descriptor;
    if (!m_worker.AddContexts(m_parser, descriptor, m_cprojectFile)) {
      ProjMgrLogger::Error(m_cprojectFile, "adding project context failed");
      return false;
    }
  }
  vector<string> devices;
  if (!m_worker.ListDevices(devices, m_filter)) {
    ProjMgrLogger::Error("processing devices list failed");
    return false;
  }
  for (const auto& device : devices) {
    cout << device << endl;
  }
  return true;
}

bool ProjMgr::RunListComponents(void) {
  if (!m_cprojectFile.empty()) {
    if (!m_parser.ParseCproject(m_cprojectFile, m_checkSchema, true)) {
      return false;
    }
    ContextDesc descriptor;
    if (!m_worker.AddContexts(m_parser, descriptor, m_cprojectFile)) {
      return false;
    }
  }
  vector<string> components;
  if (!m_worker.ListComponents(components, m_filter)) {
    ProjMgrLogger::Error("processing components list failed");
    return false;
  }
  for (const auto& component : components) {
    cout << component << endl;
  }
  return true;
}

bool ProjMgr::RunListDependencies(void) {
  if (m_cprojectFile.empty()) {
    ProjMgrLogger::Error("cproject.yml file was not specified");
    return false;
  }
  if (!m_parser.ParseCproject(m_cprojectFile, m_checkSchema, true)) {
    return false;
  }
  ContextDesc descriptor;
  if (!m_worker.AddContexts(m_parser, descriptor, m_cprojectFile)) {
    return false;
  }
  vector<string> dependencies;
  if (!m_worker.ListDependencies(dependencies, m_filter)) {
    ProjMgrLogger::Error("processing dependencies list failed");
    return false;
  }
  for (const auto& dependency : dependencies) {
    cout << dependency << endl;
  }
  return true;
}

bool ProjMgr::RunListContexts(void) {
  if (m_csolutionFile.empty()) {
    ProjMgrLogger::Error("csolution.yml file was not specified");
    return false;
  }
  if (!m_parser.ParseCsolution(m_csolutionFile, m_checkSchema)) {
    return false;
  }
  for (const auto& cproject : m_parser.GetCsolution().cprojects) {
    error_code ec;
    string const& cprojectFile = fs::canonical(m_rootDir + "/" + cproject, ec).generic_string();
    if (!m_parser.ParseCproject(cprojectFile, m_checkSchema)) {
      return false;
    }
  }
  for (auto& descriptor : m_parser.GetCsolution().contexts) {
    error_code ec;
    const string& cprojectFile = fs::path(descriptor.cproject).is_absolute() ?
      descriptor.cproject : fs::canonical(m_rootDir + "/" + descriptor.cproject, ec).generic_string();
    if (!m_worker.AddContexts(m_parser, descriptor, cprojectFile)) {
      return false;
    }
  }
  vector<string> contexts;
  if (!m_worker.ListContexts(contexts, m_filter)) {
    ProjMgrLogger::Error("processing contexts list failed");
    return false;
  }
  for (const auto& context : contexts) {
    cout << context << endl;
  }
  return true;
}

bool ProjMgr::RunListGenerators(void) {
  // Parse all input files and create contexts
  if (!PopulateContexts()) {
    return false;
  }

  // Process context
  vector<string> contexts;
  m_worker.ListContexts(contexts);
  if (m_context.empty()) {
    if (contexts.size() == 1) {
      m_context = contexts.front();
    } else {
      ProjMgrLogger::Error("context was not specified");
      return false;
    }
  } else {
    if (find(contexts.begin(), contexts.end(), m_context) == contexts.end()) {
      ProjMgrLogger::Error("context '" + m_context + "' was not found");
      return false;
    }
  }

  // Get generators
  vector<string> generators;
  if (!m_worker.ListGenerators(m_context, generators)) {
    return false;
  }
  for (const auto& generator : generators) {
    cout << generator << endl;
  }
  return true;
}

bool ProjMgr::RunCodeGenerator(void) {
  // Parse all input files and create contexts
  if (!PopulateContexts()) {
    return false;
  }

  // Check input options
  if (m_codeGenerator.empty()) {
    ProjMgrLogger::Error("generator identifier was not specified");
    return false;
  }

  // Process context
  vector<string> contexts;
  m_worker.ListContexts(contexts);
  if (m_context.empty()) {
    if (contexts.size() == 1) {
      m_context = contexts.front();
    } else {
      ProjMgrLogger::Error("context was not specified");
      return false;
    }
  } else {
    if (find(contexts.begin(), contexts.end(), m_context) == contexts.end()) {
      ProjMgrLogger::Error("context '" + m_context + "' was not found");
      return false;
    }
  }

  // Run code generator
  if (!m_worker.ExecuteGenerator(m_context, m_codeGenerator)) {
    return false;
  }

  return true;
}
