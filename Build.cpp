//
// Created by sigsegv on 3/13/23.
//

#include "Build.h"
#include "Port.h"
#include "PortGroup.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include "Fork.h"
#include "Shell.h"
#include "Buildenv.h"
#include "Sysconfig.h"
#include "Ports.h"
#include "Unpack.h"
#include "sha2alg.h"
#include "FindLib.h"
#include <sstream>
extern "C" {
#include <unistd.h>
}

Build GetBuild(Ports &ports, const std::string &buildName, const std::vector<std::string> &flags) {
    std::string groupName{};
    std::string portName{};
    std::string version{};
    {
        path buildpath{buildName};
        auto iterator = buildpath.begin();
        if (iterator == buildpath.end()) {
            std::cerr << "Invalid build name: " << buildName << "\n";
            return {};
        }
        groupName = *iterator;
        ++iterator;
        if (iterator == buildpath.end()) {
            std::cerr << "Invalid build name: " << buildName << "\n";
            return {};
        }
        portName = *iterator;
        ++iterator;
        if (iterator == buildpath.end()) {
            std::cerr << "Invalid build name: " << buildName << "\n";
            return {};
        }
        version = *iterator;
        ++iterator;
        if (iterator != buildpath.end()) {
            std::cerr << "Invalid build name: " << buildName << "\n";
            return {};
        }
    }
    auto group = ports.GetGroup(groupName);
    if (!group || !group->IsValid()) {
        std::cerr << buildName << ": Group " << groupName << " not found.\n";
        return {};
    }
    auto port = group->GetPort(portName);
    if (!port || !port->IsValid()) {
        std::cerr << buildName << ": Port " << portName << " not found.\n";
        return {};
    }
    auto build = port->GetBuild(version, flags);
    if (!build.IsValid()) {
        std::cerr << buildName << ": Build " << version << " not fount.\n";
        return {};
    }
    return build;
}

class BuildException : public std::exception {
private:
    const char *error;
public:
    BuildException(const char *error) : error(error) {}
    const char * what() const noexcept override {
        return error;
    }
};

Build::Build(const std::shared_ptr<const Port> &port, path buildfile, const std::vector<std::string> &flags) : port(port), buildfile(buildfile), version(), distfiles(), prefix("/usr"), tooling("configure"), libc(), libcpp(), libcppHeaderBuild(), bootstrap(), staticBootstrap(), cflags(), cxxflags(), ldflags(), sysrootCxxflags(), sysrootLdflags(), sysrootCmake(), nosysrootLdflags(), nobootstrapLdflags(), buildTargets(), installTargets(), beforeConfigure(), beforeBuild(), postInstall(), configureParams(), staticConfigureParams(), sysrootConfigureParams(), sysrootEnv(), patches(), configureSkip(false), configureDefaultParameters(true), configureStaticOverrides(false), configureSysrootOverrides(false), requiresClang(false), valid(false), flags(flags) {
    std::string filename{buildfile.filename()};
    std::string portName{port->GetName()};
    const std::string end{".build"};
    if (filename.starts_with(portName + "-") && filename.ends_with(end)) {
        std::string substr{filename.c_str() + portName.size() + 1, filename.size() - portName.size() - end.size() - 1};
        version = substr;
    }
    std::ifstream buildfileInput{buildfile};
    nlohmann::json jsonData{};
    try {
        jsonData = nlohmann::json::parse(buildfileInput);
        {
            auto iterator = jsonData.find("distfiles");
            if (iterator != jsonData.end()) {
                auto distfilesObject = iterator.value();
                auto iterator = distfilesObject.begin();
                while (iterator != distfilesObject.end()) {
                    std::string filename = iterator.key();
                    std::vector<std::string> source{};
                    auto filenameObject = iterator.value();
                    {
                        auto iterator = filenameObject.find("src");
                        if (iterator != filenameObject.end()) {
                            auto sources = iterator.value();
                            if (sources.is_array()) {
                                for (auto &item : sources) {
                                    if (item.is_string()) {
                                        std::string s = item;
                                        source.push_back(s);
                                    }
                                }
                            }
                        }
                    }
                    std::string type{};
                    {
                        auto iterator = filenameObject.find("type");
                        if (iterator != filenameObject.end()) {
                            auto typeObject = *iterator;
                            if (typeObject.is_string()) {
                                type = typeObject;
                            }
                        }
                    }
                    distfiles.emplace_back(filename, type, source);
                    ++iterator;
                }
            }
        }
        {
            auto iterator = jsonData.find("builddir");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_string()) {
                    builddir = item;
                }
            }
        }
        {
            auto iterator = jsonData.find("prefix");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_string()) {
                    prefix = item;
                }
            }
        }
        {
            auto iterator = jsonData.find("tooling");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_string()) {
                    tooling = item;
                }
            }
        }
        {
            auto iterator = jsonData.find("libc");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_string()) {
                    libc = item;
                }
            }
        }
        {
            auto iterator = jsonData.find("libcpp");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_string()) {
                    libcpp = item;
                }
            }
        }
        {
            auto iterator = jsonData.find("libcppHeaderBuild");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_string()) {
                    libcppHeaderBuild = item;
                }
            }
        }
        {
            auto iterator = jsonData.find("requiresClang");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_boolean()) {
                    requiresClang = item;
                }
            }
        }
        {
            auto iterator = jsonData.find("bootstrap");
            if (iterator != jsonData.end()) {
                auto &bootstrapItem = *iterator;
                if (bootstrapItem.is_array()) {
                    auto iterator = bootstrapItem.begin();
                    while (iterator != bootstrapItem.end()) {
                        auto bootstrapElement = *iterator;
                        if (bootstrapElement.is_string()) {
                            std::string bootstrap = bootstrapElement;
                            this->bootstrap.emplace_back(bootstrap);
                        }
                        ++iterator;
                    }
                }
            }
        }
        {
            auto iterator = jsonData.find("cflags");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_string()) {
                    cflags = item;
                    ReplaceVars(flags, cflags);
                }
            }
        }
        {
            auto iterator = jsonData.find("cxxflags");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_string()) {
                    cxxflags = item;
                    ReplaceVars(flags, cxxflags);
                }
            }
        }
        {
            auto iterator = jsonData.find("ldflags");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_string()) {
                    ldflags = item;
                    ReplaceVars(flags, ldflags);
                }
            }
        }
        {
            auto iterator = jsonData.find("buildTargets");
            if (iterator != jsonData.end()) {
                auto &buildTargets = *iterator;
                if (buildTargets.is_array()) {
                    auto iterator = buildTargets.begin();
                    while (iterator != buildTargets.end()) {
                        auto &buildTarget = *iterator;
                        if (buildTarget.is_string()) {
                            std::string target = buildTarget;
                            this->buildTargets.emplace_back(target);
                        }
                        ++iterator;
                    }
                }
            }
        }
        {
            auto iterator = jsonData.find("installTargets");
            if (iterator != jsonData.end()) {
                auto &installTargets = *iterator;
                if (installTargets.is_array()) {
                    auto iterator = installTargets.begin();
                    while (iterator != installTargets.end()) {
                        auto &installTarget = *iterator;
                        if (installTarget.is_string()) {
                            std::string target = installTarget;
                            this->installTargets.emplace_back(target);
                        }
                        ++iterator;
                    }
                }
            } else {
                installTargets.emplace_back("install");
            }
        }
        {
            auto iterator = jsonData.find("configure");
            if (iterator != jsonData.end()) {
                auto &configure = *iterator;
                {
                    auto iterator = configure.find("before");
                    if (iterator != configure.end()) {
                        auto &before = *iterator;
                        if (before.is_array()) {
                            auto iterator = before.begin();
                            while (iterator != before.end()) {
                                auto &cmd = *iterator;
                                if (cmd.is_array()) {
                                    std::vector<std::string> argv{};
                                    auto iterator = cmd.begin();
                                    while (iterator != cmd.end()) {
                                        auto c = *iterator;
                                        if (c.is_string()) {
                                            std::string cstr = c;
                                            argv.emplace_back(cstr);
                                        }
                                        ++iterator;
                                    }
                                    if (!argv.empty()) {
                                        beforeConfigure.emplace_back(argv);
                                    }
                                }
                                ++iterator;
                            }
                        }
                    }
                }
                {
                    auto iterator = configure.find("parameters");
                    if (iterator != configure.end()) {
                        auto &parameters = *iterator;
                        if (parameters.is_array()) {
                            auto iterator = parameters.begin();
                            while (iterator != parameters.end()) {
                                auto &value = *iterator;
                                if (value.is_string()) {
                                    std::string str = value;
                                    configureParams.emplace_back(str);
                                }
                                ++iterator;
                            }
                        }
                    }
                }
                {
                    auto iterator = configure.find("defaultParameters");
                    if (iterator != configure.end()) {
                        auto &defparams = *iterator;
                        if (defparams.is_boolean()) {
                            bool defparamsValue = defparams;
                            configureDefaultParameters = defparamsValue;
                        }
                    }
                }
                {
                    auto skip = configure.find("skip");
                    if (skip != configure.end()) {
                        auto skipValue = *skip;
                        if (skipValue.is_boolean()) {
                            this->configureSkip = skipValue;
                        }
                    }
                }
            }
        }
        {
            auto iterator = jsonData.find("build");
            if (iterator != jsonData.end()) {
                auto &build = *iterator;
                {
                    auto iterator = build.find("before");
                    if (iterator != build.end()) {
                        auto &before = *iterator;
                        if (before.is_array()) {
                            auto iterator = before.begin();
                            while (iterator != before.end()) {
                                auto &cmd = *iterator;
                                if (cmd.is_array()) {
                                    std::vector<std::string> argv{};
                                    auto iterator = cmd.begin();
                                    while (iterator != cmd.end()) {
                                        auto c = *iterator;
                                        if (c.is_string()) {
                                            std::string cstr = c;
                                            argv.emplace_back(cstr);
                                        }
                                        ++iterator;
                                    }
                                    if (!argv.empty()) {
                                        beforeBuild.emplace_back(argv);
                                    }
                                }
                                ++iterator;
                            }
                        }
                    }
                }
            }
        }
        {
            auto iterator = jsonData.find("install");
            if (iterator != jsonData.end()) {
                auto &install = *iterator;
                {
                    auto iterator = install.find("after");
                    if (iterator != install.end()) {
                        auto &after = *iterator;
                        if (after.is_array()) {
                            auto iterator = after.begin();
                            while (iterator != after.end()) {
                                auto &cmd = *iterator;
                                if (cmd.is_array()) {
                                    std::vector<std::string> argv{};
                                    auto iterator = cmd.begin();
                                    while (iterator != cmd.end()) {
                                        auto c = *iterator;
                                        if (c.is_string()) {
                                            std::string cstr = c;
                                            argv.emplace_back(cstr);
                                        }
                                        ++iterator;
                                    }
                                    if (!argv.empty()) {
                                        postInstall.emplace_back(argv);
                                    }
                                }
                                ++iterator;
                            }
                        }
                    }
                }
            }
        }
        {
            auto iterator = jsonData.find("static");
            if (iterator != jsonData.end()) {
                auto &staticItem = *iterator;
                if (staticItem.is_object()) {
                    auto iterator = staticItem.find("configure");
                    if (iterator != staticItem.end()) {
                        auto &configure = *iterator;
                        {
                            auto iterator = configure.find("parameters");
                            if (iterator != configure.end()) {
                                auto &parameters = *iterator;
                                if (parameters.is_array()) {
                                    auto iterator = parameters.begin();
                                    while (iterator != parameters.end()) {
                                        auto &value = *iterator;
                                        if (value.is_string()) {
                                            std::string str = value;
                                            staticConfigureParams.emplace_back(str);
                                        }
                                        ++iterator;
                                    }
                                }
                            }
                        }
                        {
                            auto iterator = configure.find("overrides");
                            if (iterator != configure.end()) {
                                auto &defparams = *iterator;
                                if (defparams.is_boolean()) {
                                    bool defparamsValue = defparams;
                                    configureStaticOverrides = defparamsValue;
                                }
                            }
                        }
                    }
                }
                if (staticItem.is_array()) {
                    auto iterator = staticItem.begin();
                    while (iterator != staticItem.end()) {
                        auto &staticBuild = *iterator;
                        if (staticBuild.is_string()) {
                            std::string str = staticBuild;
                            staticBootstrap.emplace_back(str);
                        }
                        ++iterator;
                    }
                }
            }
        }
        {
            auto iterator = jsonData.find("sysroot");
            if (iterator != jsonData.end()) {
                auto &sysrootItem = *iterator;
                if (sysrootItem.is_object()) {
                    {
                        auto iterator = sysrootItem.find("configure");
                        if (iterator != sysrootItem.end()) {
                            auto &configure = *iterator;
                            {
                                auto iterator = configure.find("parameters");
                                if (iterator != configure.end()) {
                                    auto &parameters = *iterator;
                                    if (parameters.is_array()) {
                                        auto iterator = parameters.begin();
                                        while (iterator != parameters.end()) {
                                            auto &value = *iterator;
                                            if (value.is_string()) {
                                                std::string str = value;
                                                sysrootConfigureParams.emplace_back(str);
                                            }
                                            ++iterator;
                                        }
                                    }
                                }
                            }
                            {
                                auto iterator = configure.find("overrides");
                                if (iterator != configure.end()) {
                                    auto &defparams = *iterator;
                                    if (defparams.is_boolean()) {
                                        bool defparamsValue = defparams;
                                        configureSysrootOverrides = defparamsValue;
                                    }
                                }
                            }
                        }
                    }
                    {
                        auto iterator = sysrootItem.find("env");
                        if (iterator != sysrootItem.end()) {
                            auto &sysrootEnv = *iterator;
                            if (sysrootEnv.is_object()) {
                                for (const auto &item : sysrootEnv.items()) {
                                    std::string key = item.key();
                                    const auto &value = item.value();
                                    if (!value.is_string()) {
                                        continue;
                                    }
                                    std::string valueAsString = value;
                                    this->sysrootEnv.insert_or_assign(key, valueAsString);
                                }
                            }
                        }
                    }
                    {
                        auto iterator = sysrootItem.find("cxxflags");
                        if (iterator != sysrootItem.end()) {
                            auto &cxxflags = *iterator;
                            if (cxxflags.is_string()) {
                                sysrootCxxflags = cxxflags;
                                ReplaceVars(flags, sysrootCxxflags);
                            }
                        }
                    }
                    {
                        auto iterator = sysrootItem.find("ldflags");
                        if (iterator != sysrootItem.end()) {
                            auto &ldflags = *iterator;
                            if (ldflags.is_string()) {
                                sysrootLdflags = ldflags;
                                ReplaceVars(flags, sysrootLdflags);
                            }
                        }
                    }
                    {
                        auto iterator = sysrootItem.find("cmake");
                        if (iterator != sysrootItem.end()) {
                            auto &ldflags = *iterator;
                            if (ldflags.is_string()) {
                                sysrootCmake = ldflags;
                                ReplaceVars(flags, sysrootCmake);
                            }
                        }
                    }
                }
            }
        }
        {
            auto iterator = jsonData.find("nosysroot");
            if (iterator != jsonData.end()) {
                auto &nosysrootItem = *iterator;
                if (nosysrootItem.is_object()) {
                    {
                        auto iterator = nosysrootItem.find("ldflags");
                        if (iterator != nosysrootItem.end()) {
                            auto &ldflags = *iterator;
                            if (ldflags.is_string()) {
                                nosysrootLdflags = ldflags;
                                ReplaceVars(flags, nosysrootLdflags);
                            }
                        }
                    }
                }
                {
                    auto iterator = nosysrootItem.find("env");
                    if (iterator != nosysrootItem.end()) {
                        auto &sysrootEnv = *iterator;
                        if (sysrootEnv.is_object()) {
                            for (const auto &item : sysrootEnv.items()) {
                                std::string key = item.key();
                                const auto &value = item.value();
                                if (!value.is_string()) {
                                    continue;
                                }
                                std::string valueAsString = value;
                                this->nosysrootEnv.insert_or_assign(key, valueAsString);
                            }
                        }
                    }
                }
            }
        }
        {
            auto iterator = jsonData.find("nobootstrap");
            if (iterator != jsonData.end()) {
                auto &nobootstrapItem = *iterator;
                if (nobootstrapItem.is_object()) {
                    {
                        auto iterator = nobootstrapItem.find("ldflags");
                        if (iterator != nobootstrapItem.end()) {
                            auto &ldflags = *iterator;
                            if (ldflags.is_string()) {
                                nobootstrapLdflags = ldflags;
                                ReplaceVars(flags, nobootstrapLdflags);
                            }
                        }
                    }
                }
            }
        }
        {
            auto iterator = jsonData.find("patches");
            if (iterator != jsonData.end()) {
                auto &patches = *iterator;
                if (patches.is_array()) {
                    auto iterator = patches.begin();
                    while (iterator != patches.end()) {
                        auto &patch = *iterator;
                        if (patch.is_string()) {
                            std::string patchFile = patch;
                            this->patches.emplace_back(patchFile);
                        }
                        ++iterator;
                    }
                }
            }
        }
    } catch (std::exception &e) {
        std::cerr << buildfile << ": json parse: " << e.what() << "\n";
        return;
    }
    valid = true;
}

static void ReplaceVars(std::string &str, const std::string &key, const std::function<std::string ()> &valueFunc) {
    std::string::size_type pos = 0;
    while (true) {
        pos = str.find(key, pos);
        if (pos == std::string::npos) {
            break;
        }
        str.erase(pos, key.length());
        std::string value = valueFunc();
        str.insert(pos, value);
        pos += value.length();
    }
}

static bool IsBootstrapping(const std::vector<std::string> &flags) {
    for (const auto &flag : flags) {
        if (flag == "bootstrapping") {
            return true;
        }
    }
    return false;
}

void Build::ReplaceVars(const std::vector<std::string> &flags, std::string &str) const {
    ::ReplaceVars(str, "{WORKDIR}", [this] () {
        std::string workdirStr;
        {
            path workdir = port->GetRoot() / "work";
            workdirStr = workdir;
        }
        return workdirStr;
    });
    ::ReplaceVars(str, "{STATIC_ZLIB}", [] () {
        return FindLib("libz.a").Find();
    });
    ::ReplaceVars(str, "{SYSROOT}", [] () {
        auto env = Exec::getenv();
        std::string sysroot{};
        for (const auto &pair : env) {
            std::string key = pair.first;
            std::transform(key.begin(), key.end(), key.begin(), [] (const char c) { return std::tolower(c); });
            if (key == "sysroot") {
                sysroot = pair.second;
                continue;
            }
        }
        return sysroot;
    });
    ::ReplaceVars(str, "{CFLAGS}", [this, &flags] () {
        auto env = Exec::getenv();
        Sysconfig sysconfig{};
        Buildenv buildenv{sysconfig, cflags, cxxflags, ldflags, sysrootCxxflags, sysrootLdflags, nosysrootLdflags, nobootstrapLdflags, requiresClang, IsBootstrapping(flags)};
        buildenv.FilterEnv(env);
        std::string cflags{};
        for (const auto &pair : env) {
            std::string key = pair.first;
            std::transform(key.begin(), key.end(), key.begin(), [] (const char c) { return std::tolower(c); });
            if (key == "cflags") {
                cflags = pair.second;
                continue;
            }
        }
        return cflags;
    });
    ::ReplaceVars(str, "{BUILDDIR}", [this] () {
        std::string builddirStr{};
        {
            path workdir = port->GetRoot() / "work";
            path builddir = workdir / this->builddir;
            builddirStr = builddir;
        }
        return builddirStr;
    });
    ::ReplaceVars(str, "{DISTFILES}", [this] () {
        std::string distfilesStr{};
        {
            auto ports = port->GetGroup()->GetPortsRoot();
            auto distfilesPath = ports->GetRoot() / "distfiles";
            distfilesStr = distfilesPath;
        }
        return distfilesStr;
    });
}

void Build::ApplyEnv(const std::string &sysroot, std::map<std::string,std::string> &env) {
    if (sysroot.empty()) {
        for (const auto &pair : nosysrootEnv) {
            std::string key = pair.first;
            std::transform(key.begin(), key.end(), key.begin(), [] (const char c) { return std::tolower(c); });
            std::string value = pair.second;
            bool found{false};
            for (auto &pair : env) {
                std::string foundKey = pair.first;
                std::transform(foundKey.begin(), foundKey.end(), foundKey.begin(), [] (const char c) { return std::tolower(c); });
                if (key == foundKey) {
                    found = true;
                    ReplaceVars(flags, value);
                    pair.second = value;
                    break;
                }
            }
            key = pair.first;
            if (!found) {
                ReplaceVars(flags, value);
                env.insert_or_assign(key, value);
            }
        }
    } else {
        for (const auto &pair : sysrootEnv) {
            std::string key = pair.first;
            std::transform(key.begin(), key.end(), key.begin(), [] (const char c) { return std::tolower(c); });
            std::string value = pair.second;
            bool found{false};
            for (auto &pair : env) {
                std::string foundKey = pair.first;
                std::transform(foundKey.begin(), foundKey.end(), foundKey.begin(), [] (const char c) { return std::tolower(c); });
                if (key == foundKey) {
                    found = true;
                    ReplaceVars(flags, value);
                    pair.second = value;
                    break;
                }
            }
            key = pair.first;
            if (!found) {
                ReplaceVars(flags, value);
                env.insert_or_assign(key, value);
            }
        }
    }
}

std::string Build::GetName() const {
    return port->GetName();
}

void Build::Fetch() {
    for (const auto &distfile : distfiles) {
        std::cout << "Fetching " << distfile.GetName() << "\n";
        distfile.Fetch(*(port->GetGroup()->GetPortsRoot()));
    }
}

void Build::Extract() {
    Fetch();
    path workdir = port->GetRoot() / "work";
    if (exists(workdir)) {
        return;
    }
    std::cout << "==> Extracting " << GetName() << "-" << GetVersion() << "\n";
    for (const auto &distfile : distfiles) {
        distfile.Extract(*(port->GetGroup()->GetPortsRoot()), port->GetRoot());
    }
    path builddir = workdir / this->builddir;
    if (!patches.empty() && exists(builddir)) {
        std::cout << "==> Applying patches to " << GetName() << "-" << GetVersion() << "\n";
        for (const auto &patch : patches) {
            std::cout << "*** " << patch << "\n";
            std::fstream inputStream{};
            inputStream.open(this->port->GetRoot() / "patches" / patch, std::ios_base::in | std::ios_base::binary);
            if (!inputStream.is_open()) {
                throw BuildException("Failed to open patch file");
            }
            Fork patcher{[&builddir] () {
                if (chdir(builddir.c_str()) != 0) {
                    std::cerr << "chdir: build dir: " << builddir << "\n";
                    return 1;
                }
                Exec exec{"patch"};
                std::vector<std::string> args{};
                args.emplace_back("-p1");
                exec.exec(args, Exec::getenv());
                return 0;
            }, ForkInputOutput::INPUT};
            patcher << inputStream;
            patcher.CloseInput();
            patcher.Require();
        }
    }
}

enum class Tooling {
    CONFIGURE,
    CMAKE,
    BOOTSTRAP,
    STATIC_FILES,
    MESON,
    CUSTOM
};

Tooling Build::GetTooling() const {
    if (this->tooling == "configure") {
        return Tooling::CONFIGURE;
    } else if (this->tooling == "cmake") {
        return Tooling::CMAKE;
    } else if (this->tooling == "bootstrap") {
        return Tooling::BOOTSTRAP;
    } else if (this->tooling == "static-files") {
        return Tooling::STATIC_FILES;
    } else if (this->tooling == "meson") {
        return Tooling::MESON;
    } else if (this->tooling == "custom") {
        return Tooling::CUSTOM;
    }
    throw BuildException("Tooling not found");
}

void Build::Clean() {
    path workdir = port->GetRoot() / "work";
    if (exists(workdir)) {
        std::cout << "==> Cleaning " << GetName() << "-" << GetVersion() << "\n";
        Fork fork{[this] () {
            if (chdir(port->GetRoot().c_str()) != 0) {
                std::cerr << "chdir: build dir: " << port->GetRoot() << "\n";
                return 1;
            }
            Exec exec{"rm"};
            std::vector<std::string> args{};
            args.emplace_back("-rf");
            args.emplace_back("work");
            auto env = Exec::getenv();
            exec.exec(args, env);
            return 0;
        }};
    }
}

void Build::Configure() {
    path workdir = port->GetRoot() / "work";
    if (exists(workdir / "configured")) {
        return;
    }
    Extract();
    bool staticBuild = (std::find(flags.begin(), flags.end(), "static") != flags.end());
    if (!exists(workdir)) {
        if (!create_directory(workdir)) {
            throw BuildException("Create workdir");
        }
    }
    path builddir = workdir / this->builddir;
    if (!exists(builddir)) {
        if (!create_directory(builddir)) {
            throw BuildException("Create builddir");
        }
    }
    path cmakeDir;
    {
        std::string cm{this->builddir};
        cm.append("-cmake");
        cmakeDir = port->GetRoot() / "work" / cm;
    }
    std::cout << "==> Configuring " << GetName() << "-" << GetVersion() << "\n";
    if (configureSkip) {
        create_directory(workdir / "configured");
        return;
    }
    auto tooling = GetTooling();
    Sysconfig sysconfig{};
    bool chownSourceDirs{true};
    if (tooling == Tooling::CMAKE || tooling == Tooling::MESON) {
        if (!exists(cmakeDir)) {
            if (!create_directory(cmakeDir)) {
                throw BuildException("Unable to create cmake directory");
            }
        }
        if (!is_directory(cmakeDir)) {
            throw BuildException("Cmake directory: Is not a directory");
        }
        sysconfig.Chown(cmakeDir);
        chownSourceDirs = false;
    }
    if (tooling == Tooling::BOOTSTRAP) {
        return;
    }
    if (tooling == Tooling::STATIC_FILES) {
        return;
    }
    auto env = Exec::getenv();
    Buildenv buildenv{sysconfig, cflags, cxxflags, ldflags, sysrootCxxflags, sysrootLdflags, nosysrootLdflags,
                      nobootstrapLdflags, requiresClang, IsBootstrapping(flags)};
    buildenv.FilterEnv(env);
    std::string sysroot = buildenv.Sysroot();
    ApplyEnv(sysroot, env);
    if (exists(builddir) && is_directory(builddir)) {
        if (chownSourceDirs) {
            sysconfig.ChownDirTree(builddir);
        }
        for (const auto &cmd : beforeConfigure) {
            auto iterator = cmd.begin();
            if (iterator == cmd.end()) {
                continue;
            }
            auto cmdexec = *iterator;
            ++iterator;
            std::vector<std::string> args{};
            while (iterator != cmd.end()) {
                args.emplace_back(*iterator);
                ++iterator;
            }
            Fork f{[&sysconfig, &builddir, &cmdexec, &args, &env] () {
                sysconfig.SetUserAndGroup();
                if (chdir(builddir.c_str()) != 0) {
                    std::cerr << "chdir: build dir: " << builddir << "\n";
                    return 1;
                }
                Exec e{cmdexec};
                e.exec(args, env);
                return 0;
            }};
            f.Require();
        }
        Fork f{[this, &sysconfig, &builddir, &cmakeDir, &tooling, staticBuild, &env, &sysroot] () {
            sysconfig.SetUserAndGroup();
            if (tooling == Tooling::CONFIGURE) {
                if (chdir(builddir.c_str()) != 0) {
                    std::cerr << "chdir: build dir: " << builddir << "\n";
                    return 1;
                }
                std::string configureName;
                for (auto diritem : std::filesystem::directory_iterator(builddir)) {
                    std::string filename = diritem.path().filename();
                    if (filename == "configure") {
                        configureName = filename;
                        break;
                    }
                    std::string lowercase = filename;
                    std::transform(lowercase.begin(), lowercase.end(), lowercase.begin(), [] (const char c) { return std::tolower(c); });
                    if (lowercase == "configure") {
                        configureName = filename;
                    }
                }
                if (configureName.empty()) {
                    std::cerr << "error: configure script not found\n";
                    return 1;
                }
                {
                    std::string str{"./"};
                    str.append(configureName);
                    configureName = str;
                }
                Exec sh{configureName};
                std::vector<std::string> conf{};
                if (configureDefaultParameters) {
                    std::string prefix{"--prefix="};
                    prefix.append(this->prefix);
                    conf.emplace_back(prefix);
                }
                for (const std::string &param: configureParams) {
                    conf.emplace_back(param);
                }
                if (!sysroot.empty()) {
                    for (const std::string &param : sysrootConfigureParams) {
                        conf.emplace_back(param);
                    }
                }
                if (staticBuild) {
                    for (const std::string &param: staticConfigureParams) {
                        conf.emplace_back(param);
                    }
                }
                for (auto &param : conf) {
                    ReplaceVars(flags, param);
                }
                sh.exec(conf, env);
            } else if (tooling == Tooling::CMAKE) {
                if (chdir(cmakeDir.c_str()) != 0) {
                    std::cerr << "chdir: build dir: " << builddir << "\n";
                    return 1;
                }
                std::string cmake{"cmake"};
                if (!sysrootCmake.empty()) {
                    cmake = sysrootCmake;
                    ReplaceVars(flags, cmake);
                }
                Exec exec{cmake};
                std::vector<std::string> cmk{};
                if (configureDefaultParameters) {
                    {
                        std::string prefix{"-DCMAKE_INSTALL_PREFIX="};
                        prefix.append(this->prefix);
                        cmk.emplace_back(prefix);
                    }
                    cmk.emplace_back("-DCMAKE_BUILD_TYPE=Release");
                }
                for (const std::string &param: configureParams) {
                    cmk.emplace_back(param);
                }
                if (!sysroot.empty()) {
                    for (const std::string &param : sysrootConfigureParams) {
                        cmk.emplace_back(param);
                    }
                }
                if (staticBuild) {
                    for (const std::string &param: staticConfigureParams) {
                        cmk.emplace_back(param);
                    }
                }
                if (configureDefaultParameters) {
                    cmk.emplace_back(builddir);
                }
                for (auto &param : cmk) {
                    ReplaceVars(flags, param);
                }
                exec.exec(cmk, env);
            } else if (tooling == Tooling::MESON) {
                if (chdir(cmakeDir.c_str()) != 0) {
                    std::cerr << "chdir: build dir: " << builddir << "\n";
                    return 1;
                }
                Exec sh{"meson"};
                std::vector<std::string> conf{};
                for (const std::string &param: configureParams) {
                    conf.emplace_back(param);
                }
                if (!sysroot.empty()) {
                    for (const std::string &param : sysrootConfigureParams) {
                        conf.emplace_back(param);
                    }
                }
                if (staticBuild) {
                    for (const std::string &param: staticConfigureParams) {
                        conf.emplace_back(param);
                    }
                }
                if (configureDefaultParameters) {
                    {
                        std::string prefix{"-Dprefix="};
                        prefix.append(this->prefix);
                        conf.emplace_back(prefix);
                    }
                    {
                        std::string bd = cmakeDir;
                        conf.emplace_back(bd);
                    }
                    {
                        std::string sd = builddir;
                        conf.emplace_back(sd);
                    }
                }
                for (auto &param : conf) {
                    ReplaceVars(flags, param);
                }
                sh.exec(conf, env);
            }
            return 0;
        }};
        f.Require();
    } else {
        throw BuildException("Build dir not found");
    }
    create_directory(workdir / "configured");
}

std::vector<std::filesystem::path> ListFiles(const std::filesystem::path &root,
                                             const std::function<bool (const std::filesystem::path &)> &match = [] (const std::filesystem::path &) {return true;}) {
    std::vector<std::filesystem::path> subitems{};
    try {
        for (const auto &subitem: std::filesystem::directory_iterator{root}) {
            std::string filename = subitem.path().filename();
            if (filename == "." || filename == "..") {
                continue;
            }
            if (!match(subitem.path())) {
                continue;
            }
            subitems.emplace_back(filename);
            if (!is_symlink(subitem) && is_directory(subitem)) {
                for (const auto &subsubitem: ListFiles(subitem, match)) {
                    path p = filename / subsubitem;
                    subitems.emplace_back(p);
                }
            }
        }
    } catch (...) {
        return {};
    }
    return subitems;
}

std::string FileListString(const std::vector<std::filesystem::path> &paths) {
    std::string fileListStr{};
    {
        for (const auto &file : paths) {
            if (!fileListStr.empty()) {
                fileListStr.append("\n");
            }
            fileListStr.append(file);
        }
    }
    return fileListStr;
}

void Build::MakeBootstrap() {
    path workdir = port->GetRoot() / "work";
    path builddir = workdir / this->builddir;
    if (chdir(builddir.c_str()) != 0) {
        std::cerr << "chdir: build dir: " << builddir << "\n";
        throw BuildException("bootstrap chdir to build");
    }
    auto ports = Ports::Create(port->GetGroup()->GetPortsRoot()->GetRoot().c_str());
    std::vector<std::string> bootstrapFlags{};
    std::vector<std::string> staticFlags{};
    for (const auto &flag : flags) {
        bootstrapFlags.emplace_back(flag);
        staticFlags.emplace_back(flag);
    }
    bootstrapFlags.emplace_back("bootstrapping");
    staticFlags.emplace_back("static");
    auto build = GetBuild(*ports, libc, bootstrapFlags);
    if (!build.IsValid()) {
        std::cerr << "Bootstrap libc was not found: " << libc << "\n";
        throw BuildException("Bootstrap libc not found");
    }
    auto libcppHeaderOnlyBuild = GetBuild(*ports, libcppHeaderBuild, staticFlags);
    if (!libcppHeaderOnlyBuild.IsValid()) {
        std::cerr << "Bootstrap libcpp header build was not found: " << libcpp << "\n";
        throw BuildException("Bootstrap libcpp header not found");
    }
    auto libcppBuild = GetBuild(*ports, libcpp, bootstrapFlags);
    if (!libcppBuild.IsValid()) {
        std::cerr << "Bootstrap libcpp was not found: " << libcpp << "\n";
        throw BuildException("Bootstrap libcpp not found");
    }
    std::vector<Build> bootstrapBuilds{};
    std::vector<Build> bootstrapStaticBuilds{};
    path bootstrapdir = builddir / "bootstrap";
    std::string bootstrapdirStr = bootstrapdir;
    {
        auto originalEnv = Exec::getenv();
        auto env = originalEnv;
        env.insert_or_assign("SYSROOT", bootstrapdirStr);
        Exec::setenv(env);
        for (const auto &bootstrapName: this->bootstrap) {
            auto build = GetBuild(*ports, bootstrapName, bootstrapFlags);
            if (!build.IsValid()) {
                std::cerr << "Build " << bootstrapName << " not found.\n";
                throw BuildException("Build not found");
            }
            bootstrapBuilds.emplace_back(build);
        }
        Exec::setenv(originalEnv);
    }
    for (const auto &bootstrapName: this->staticBootstrap) {
        auto build = GetBuild(*ports, bootstrapName, staticFlags);
        if (!build.IsValid()) {
            std::cerr << "Build " << bootstrapName << " not found.\n";
            throw BuildException("Build not found");
        }
        bootstrapStaticBuilds.emplace_back(build);
    }
    std::cout << "==> Fetching distfiles for bootstrapping\n";
    for (auto &bb: bootstrapBuilds) {
        bb.Fetch();
    }
    auto restoreEnv = Exec::getenv();
    for (auto &bb: bootstrapStaticBuilds) {
        bb.Clean();
        bb.Package();
        bb.Clean();
        Exec::setenv(restoreEnv);
    }
    libcppHeaderOnlyBuild.Clean();
    libcppHeaderOnlyBuild.Package();
    libcppHeaderOnlyBuild.Clean();
    Exec::setenv(restoreEnv);
    build.Clean();
    build.Package();
    build.Clean();
    Exec::setenv(restoreEnv);
    if (!exists(bootstrapdir)) {
        if (!create_directory(bootstrapdir)) {
            throw BuildException("Create bootstrapdir");
        }
    }
    if (!is_directory(bootstrapdir)) {
        throw BuildException("Bootstrapdir is not directory");
    }
    {
        std::cout << "==> Copying ports dir to booststrap\n";
        path bootstrapPortsdir = bootstrapdir;
        for (const auto &part: ports->GetRoot()) {
            std::string p = part;
            if (p == "/") {
                continue;
            }
            bootstrapPortsdir = bootstrapPortsdir / p;
        }
        if (!create_directories(bootstrapPortsdir)) {
            throw BuildException("Create bootstrap ports dir");
        }
        std::string fileList = FileListString(ListFiles(ports->GetRoot(), [](const auto &p) {
            std::string filename = p.filename();
            return filename != "work";
        }));
        Fork unpacking{[bootstrapPortsdir]() {
            std::string dir = bootstrapPortsdir;
            if (chdir(dir.c_str()) != 0) {
                std::cerr << "chdir: build dir: " << dir << "\n";
                return 1;
            }
            Exec exec{"cpio"};
            std::vector<std::string> args{};
            args.emplace_back("--extract");
            auto env = Exec::getenv();
            exec.exec(args, env);
            return 0;
        }, ForkInputOutput::INPUT};
        Fork packing{[ports]() {
            if (chdir(ports->GetRoot().c_str()) != 0) {
                std::cerr << "chdir: build dir: " << ports->GetRoot() << "\n";
                return 1;
            }
            Exec exec{"cpio"};
            std::vector<std::string> args{};
            args.emplace_back("--create");
            auto env = Exec::getenv();
            exec.exec(args, env);
            return 0;
        }, ForkInputOutput::INPUTOUTPUT};
        Fork submit{[&packing, &fileList]() {
            packing << fileList;
            packing.CloseInput();
            return 0;
        }};
        packing.CloseInput();
        unpacking << packing;
        unpacking.CloseInput();
        packing.Require();
        unpacking.Require();
    }
    {
        path bootstrapdirToRoot = "..";
        {
            auto iterator = builddir.begin();
            if (iterator != builddir.end()) {
                ++iterator;
            }
            if (iterator != builddir.end()) {
                ++iterator;
            }
            while (iterator != builddir.end()) {
                bootstrapdirToRoot = bootstrapdirToRoot / "..";
                ++iterator;
            }
        }
        path bootstrapdirBootstrapLink = bootstrapdir;
        for (const auto &part: bootstrapdir) {
            std::string p = part;
            if (p == "/") {
                continue;
            }
            std::cout << p << "\n";
            bootstrapdirBootstrapLink = bootstrapdirBootstrapLink / p;
        }
        {
            path crdir = bootstrapdir;
            for (const auto &part: builddir) {
                std::string p = part;
                if (p == "/") {
                    continue;
                }
                std::cout << p << "\n";
                crdir = crdir / p;
            }
            if (!create_directories(crdir)) {
                std::cerr << "Bootstrap link failed: mkdir: " << crdir << "\n";
                throw BuildException("Bootstrap link failed");
            }
        }
        create_directory_symlink(bootstrapdirToRoot, bootstrapdirBootstrapLink);
    }
    {
        std::cout << "==> Unpacking libc to bootstrap\n";
        std::string pkg{build.GetName()};
        pkg.append("-");
        pkg.append(build.GetVersion());
        pkg.append(".pkg");
        pkg = builddir / pkg;
        Unpack unpack{pkg, bootstrapdirStr};
    }
    {
        std::cout << "==> Unpacking preliminary libcpp to bootstrap\n";
        std::string pkg{libcppHeaderOnlyBuild.GetName()};
        pkg.append("-");
        pkg.append(libcppHeaderOnlyBuild.GetVersion());
        pkg.append(".pkg");
        pkg = builddir / pkg;
        Unpack unpack{pkg, bootstrapdirStr};
    }
    {
        std::cout << "==> Unpacking static builds to bootstrap\n";
        for (auto &bb: bootstrapStaticBuilds) {
            std::string pkg{bb.GetName()};
            pkg.append("-");
            pkg.append(bb.GetVersion());
            pkg.append(".pkg");
            pkg = builddir / pkg;
            Unpack unpack{pkg, bootstrapdirStr};
        }
        auto originalEnv = Exec::getenv();
        auto env = originalEnv;
        env.insert_or_assign("SYSROOT", bootstrapdirStr);
        Exec::setenv(env);
        std::cout << "==> Rebuilding libcpp\n";
        libcppBuild.Clean();
        libcppBuild.Package();
        libcppBuild.Clean();
        {
            std::string pkg{libcppBuild.GetName()};
            pkg.append("-");
            pkg.append(libcppBuild.GetVersion());
            pkg.append(".pkg");
            pkg = builddir / pkg;
            Unpack unpack{pkg, bootstrapdirStr};
        }
        env.insert_or_assign("LIBCPP_BOOTSTRAP", bootstrapdirStr);
        Exec::setenv(env);
        std::cout << "==> Building and installing bootstrapping builds\n";
        for (auto &bb: bootstrapBuilds) {
            bb.Clean();
            bb.Package();
            bb.Clean();
            Exec::setenv(env);
            {
                std::string pkg{bb.GetName()};
                pkg.append("-");
                pkg.append(bb.GetVersion());
                pkg.append(".pkg");
                pkg = builddir / pkg;
                Unpack unpack{pkg, bootstrapdirStr};
            }
        }
        std::cout << "==> Bootstrap build complete\n";
        Exec::setenv(originalEnv);
    }
}

void Build::Make() {
    path workdir = port->GetRoot() / "work";
    if (exists(workdir / "built")) {
        return;
    }
    Configure();
    std::cout << "==> Building " << GetName() << "-" << GetVersion() << "\n";
    path builddir = workdir / this->builddir;
    auto tooling = GetTooling();
    if (tooling == Tooling::CMAKE || tooling == Tooling::MESON) {
        std::string cm{this->builddir};
        cm.append("-cmake");
        builddir = port->GetRoot() / "work" / cm;
    }
    if (exists(builddir) && is_directory(builddir)) {
        if (tooling == Tooling::BOOTSTRAP) {
            Fork f{[this] () {
                MakeBootstrap();
                return 0;
            }};
            f.Require();
        } else {
            auto env = Exec::getenv();
            Sysconfig sysconfig{};
            Buildenv buildenv{sysconfig, cflags, cxxflags, ldflags, sysrootCxxflags, sysrootLdflags, nosysrootLdflags, nobootstrapLdflags, requiresClang, IsBootstrapping(flags)};
            buildenv.FilterEnv(env);
            ApplyEnv(buildenv.Sysroot(), env);
            for (const auto &cmd : beforeBuild) {
                auto iterator = cmd.begin();
                if (iterator == cmd.end()) {
                    continue;
                }
                auto cmdexec = *iterator;
                ReplaceVars(flags, cmdexec);
                ++iterator;
                std::vector<std::string> args{};
                while (iterator != cmd.end()) {
                    auto &arg = args.emplace_back(*iterator);
                    ReplaceVars(flags, arg);
                    ++iterator;
                }
                Fork f{[&sysconfig, &builddir, &cmdexec, &args, &env] () {
                    sysconfig.SetUserAndGroup();
                    if (chdir(builddir.c_str()) != 0) {
                        std::cerr << "chdir: build dir: " << builddir << "\n";
                        return 1;
                    }
                    Exec e{cmdexec};
                    e.exec(args, env);
                    return 0;
                }};
                f.Require();
            }
            Fork f{[this, &sysconfig, builddir, tooling, &env]() {
                if (chdir(builddir.c_str()) != 0) {
                    std::cerr << "chdir: build dir: " << builddir << "\n";
                    return 1;
                }
                if (tooling == Tooling::STATIC_FILES) {
                    Fork extract([]() {
                        Exec exec{"cpio"};
                        std::vector<std::string> args{};
                        args.emplace_back("--extract");
                        exec.exec(args, Exec::getenv());
                        return 0;
                    }, ForkInputOutput::INPUT);
                    path files = port->GetRoot() / "files";
                    Fork packing{[&files]() {
                        if (chdir(files.c_str()) != 0) {
                            std::cerr << "chdir: build dir: " << files << "\n";
                            return 1;
                        }
                        Exec exec{"cpio"};
                        std::vector<std::string> args{};
                        args.emplace_back("--create");
                        exec.exec(args, Exec::getenv());
                        return 0;
                    }, ForkInputOutput::INPUTOUTPUT};
                    std::string list = FileListString(ListFiles(files));
                    Fork submit{[&packing, &list]() {
                        packing << list;
                        packing.CloseInput();
                        return 0;
                    }};
                    packing.CloseInput();
                    extract << packing;
                    extract.CloseInput();
                    submit.Require();
                    packing.Require();
                    extract.Require();
                } else if (tooling != Tooling::CUSTOM) {
                    sysconfig.SetUserAndGroup();
                    std::string makecmd{};
                    if (exists(path("Makefile"))) {
                        makecmd = "make";
                    } else if (exists(path("build.ninja"))) {
                        makecmd = "ninja";
                    } else {
                        throw BuildException("Makefile not found");
                    }
                    Exec make{makecmd};
                    std::vector<std::string> args{};
                    if (tooling == Tooling::CMAKE) {
                        args.emplace_back("VERBOSE=1");
                    }
                    for (auto &target: buildTargets) {
                        std::string targetVal{target};
                        ReplaceVars(flags, targetVal);
                        args.emplace_back(targetVal);
                    }
                    make.exec(args, env);
                }
                return 0;
            }};
            f.Require();
        }
    } else {
        throw BuildException("Build dir not found");
    }
    create_directory(workdir / "built");
}

void Build::Install() {
    path workdir = port->GetRoot() / "work";
    if (exists(workdir / "installed")) {
        return;
    }
    Make();
    std::cout << "==> Installing " << GetName() << "-" << GetVersion() << "\n";
    path builddir = workdir / this->builddir;
    auto tooling = GetTooling();
    if (tooling == Tooling::CMAKE || tooling == Tooling::MESON) {
        std::string cm{this->builddir};
        cm.append("-cmake");
        builddir = port->GetRoot() / "work" / cm;
    } else if (tooling == Tooling::BOOTSTRAP) {
        builddir = builddir / "bootstrap";
    }
    path installdir = port->GetRoot() / "work" / "install";
    if (!exists(installdir)) {
        if (!create_directory(installdir)) {
            throw BuildException("Installdir could not be created");
        }
    } else if (!is_directory(installdir)) {
        throw BuildException("Installdir is not a directory");
    }
    if (exists(builddir) && is_directory(builddir)) {
        if (tooling == Tooling::STATIC_FILES || tooling == Tooling::BOOTSTRAP) {
            Fork extract([&installdir] () {
                if (chdir(installdir.c_str()) != 0) {
                    std::cerr << "chdir: build dir: " << installdir << "\n";
                    return 1;
                }
                Exec exec{"cpio"};
                std::vector<std::string> args{};
                args.emplace_back("--extract");
                exec.exec(args, Exec::getenv());
                return 0;
            }, ForkInputOutput::INPUT);
            Fork packing{[&builddir] () {
                if (chdir(builddir.c_str()) != 0) {
                    std::cerr << "chdir: build dir: " << builddir << "\n";
                    return 1;
                }
                Exec exec{"cpio"};
                std::vector<std::string> args{};
                args.emplace_back("--create");
                exec.exec(args, Exec::getenv());
                return 0;
            }, ForkInputOutput::INPUTOUTPUT};
            std::string list = FileListString(ListFiles(builddir));
            Fork submit{[&packing, &list] () {
                packing << list;
                packing.CloseInput();
                return 0;
            }};
            packing.CloseInput();
            extract << packing;
            extract.CloseInput();
            submit.Require();
            packing.Require();
            extract.Require();
        } else if (tooling != Tooling::CUSTOM) {
            Fork f{[this, builddir, installdir]() {
                if (chdir(builddir.c_str()) != 0) {
                    std::cerr << "chdir: build dir: " << builddir << "\n";
                    return 1;
                }
                std::string makecmd{};
                if (exists(path("Makefile"))) {
                    makecmd = "make";
                } else if (exists(path("build.ninja"))) {
                    makecmd = "ninja";
                } else {
                    throw BuildException("Makefile not found");
                }
                Exec make{makecmd};
                std::vector<std::string> args{};
                if (makecmd != "ninja") {
                    std::string idir = installdir;
                    std::string destdir{"DESTDIR="};
                    destdir.append(idir);
                    args.push_back(destdir);
                }
                for (const auto &target: installTargets) {
                    std::string targetVal{target};
                    ReplaceVars(flags, targetVal);
                    args.emplace_back(targetVal);
                }
                auto env = Exec::getenv();
                Sysconfig sysconfig{};
                Buildenv buildenv{sysconfig, cflags, cxxflags, ldflags, sysrootCxxflags, sysrootLdflags, nosysrootLdflags, nobootstrapLdflags, requiresClang, IsBootstrapping(flags)};
                buildenv.FilterEnv(env);
                ApplyEnv(buildenv.Sysroot(), env);
                if (makecmd == "ninja") {
                    bool found_destdir{false};
                    for (auto &ev : env) {
                        std::string key = ev.first;
                        std::transform(key.begin(), key.end(), key.begin(), [] (auto c) { return std::tolower(c); });
                        if (key == "destdir") {
                            std::string idir = installdir;
                            ev.second = idir;
                            found_destdir = true;
                        }
                    }
                    if (!found_destdir) {
                        std::string idir = installdir;
                        env.insert_or_assign("DESTDIR", idir);
                    }
                }
                make.exec(args, env);
                return 0;
            }};
        }
         auto env = Exec::getenv();
        std::string runInDir = installdir;
        for (const auto &cmd : postInstall) {
            auto iterator = cmd.begin();
            if (iterator == cmd.end()) {
                continue;
            }
            auto cmdexec = *iterator;
            ReplaceVars(flags, cmdexec);
            ++iterator;
            std::vector<std::string> args{};
            while (iterator != cmd.end()) {
                auto &arg = args.emplace_back(*iterator);
                ReplaceVars(flags, arg);
                ++iterator;
            }
            if (cmdexec == "cd" && args.size() == 1) {
                auto dir = args[0];
                path runInDirP = runInDir;
                if (dir.starts_with("/")) {
                    runInDirP = dir;
                } else {
                    runInDirP = runInDirP / dir;
                }
                runInDir = runInDirP;
                continue;
            }
            Fork f{[&runInDir, &cmdexec, &args, &env] () {
                if (chdir(runInDir.c_str()) != 0) {
                    std::cerr << "chdir: build dir: " << runInDir << "\n";
                    return 1;
                }
                Exec e{cmdexec};
                e.exec(args, env);
                return 0;
            }};
            f.Require();
        }
    } else {
        throw BuildException("Build dir not found");
    }
    create_directory(workdir / "installed");
}

void Build::Package() {
    Install();
    std::cout << "==> Packaging " << GetName() << "-" << GetVersion() << "\n";
    path installdir = port->GetRoot() / "work" / "install";
    if (!exists(installdir) || !is_directory(installdir)) {
        throw BuildException("Install dir not found");
    }
    auto files = ListFiles(installdir);
    std::string filesWHash{};
    {
        std::stringstream sstr{};
        for (const auto &file: files) {
            std::string sp = file;
            path p = installdir / file;
            if (is_symlink(p)) {
                sstr << "link " << sp << "\n";
            } else if (is_directory(p)) {
                sstr << "dir " << sp << "\n";
            } else {
                std::fstream inputStream{};
                inputStream.open(p, std::ios_base::in | std::ios_base::binary);
                if (!inputStream.is_open()) {
                    throw BuildException("Failed to open file for hashing");
                }
                sha256 hash{};
                while (true) {
                    sha256::chunk_type chunk;
                    inputStream.read((char *) &(chunk[0]), sizeof(chunk));
                    if (inputStream) {
                        hash.Consume(chunk);
                    } else {
                        auto sz = inputStream.gcount();
                        if (sz > 0) {
                            hash.Final((uint8_t *) &(chunk[0]), sz);
                        }
                        break;
                    }
                }
                std::string hex{hash.Hex()};
                sstr << hex << " " << sp << "\n";
            }
        }
        filesWHash = sstr.str();
    }
    std::cout << filesWHash;
    std::string fileListStr = FileListString(files);
    Fork fork{[installdir] () {
        std::string directory = installdir;
        if (chdir(directory.c_str()) != 0) {
            std::cerr << "chdir: build dir: " << directory << "\n";
            return 1;
        }
        Exec exec{"cpio"};
        std::vector<std::string> args{};
        args.emplace_back("--create");
        auto env = Exec::getenv();
        exec.exec(args, env);
        return 0;
    }, ForkInputOutput::INPUTOUTPUT};
    Fork submit{[&fork, &fileListStr] () {
        fork << fileListStr;
        fork.CloseInput();
        return 0;
    }};
    fork.CloseInput();
    std::string packageFilename{GetName()};
    packageFilename.append("-");
    packageFilename.append(GetVersion());
    packageFilename.append(".pkg");
    std::fstream outputStream{};
    outputStream.open(packageFilename, std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
    if (!outputStream.is_open()) {
        throw BuildException("Failed to open package file for writing");
    }
    uint32_t fileListLen = filesWHash.size();
    outputStream.write((const char *) &fileListLen, sizeof(fileListLen));
    outputStream.write(filesWHash.c_str(), fileListLen);
    fork >> outputStream;
}
