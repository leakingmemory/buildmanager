//
// Created by sigsegv on 3/13/23.
//

#ifndef BM_BUILD_H
#define BM_BUILD_H

#include <filesystem>
#include <map>
#include "Distfile.h"

using path = std::filesystem::path;

class Port;

enum class Tooling;

class Build;

Build GetBuild(Ports &ports, const std::string &buildName);

class Build {
    std::shared_ptr<const Port> port;
    path buildfile;
    std::string version;
    std::vector<Distfile> distfiles;
    std::string builddir;
    std::string prefix;
    std::string tooling;
    /* bootstrap: */
    std::string libc;
    std::string libcpp;
    std::string libcppHeaderBuild;
    std::vector<std::string> bootstrap;
    std::vector<std::string> staticBootstrap;
    /* normal: */
    std::string cxxflags;
    std::string ldflags;
    std::string sysrootCxxflags;
    std::string sysrootLdflags;
    std::string sysrootCmake;
    std::vector<std::string> buildTargets;
    std::vector<std::string> installTargets;
    std::vector<std::vector<std::string>> beforeConfigure;
    std::vector<std::vector<std::string>> postInstall;
    std::vector<std::string> configureParams;
    std::vector<std::string> staticConfigureParams;
    std::vector<std::string> sysrootConfigureParams;
    std::map<std::string,std::string> sysrootEnv;
    std::vector<std::string> patches;
    bool configureSkip;
    bool configureDefaultParameters;
    bool configureStaticOverrides;
    bool configureSysrootOverrides;
    bool requiresClang;
    bool valid;
public:
    Build() : port(), buildfile(), version(), distfiles(), builddir(), prefix(), tooling(), libc(), libcpp(), libcppHeaderBuild(), bootstrap(), staticBootstrap(), cxxflags(), ldflags(), sysrootCxxflags(), sysrootLdflags(), sysrootCmake(), buildTargets(), beforeConfigure(), postInstall(), configureParams(), staticConfigureParams(), sysrootConfigureParams(), sysrootEnv(), installTargets(), patches(), configureSkip(false), configureStaticOverrides(false), requiresClang(false), valid(false) {}
    Build(const std::shared_ptr<const Port> &port, path buildfile);
private:
    void ReplaceVars(std::string &str) const;
    void ApplyEnv(const std::string &sysroot, std::map<std::string,std::string> &env);
public:
    [[nodiscard]] std::string GetName() const;
    [[nodiscard]] std::string GetVersion() const {
        return version;
    }
    bool IsValid() const {
        return valid;
    }
    void Fetch();
    void Extract();
private:
    Tooling GetTooling() const;
public:
    void Clean();
    void Configure(const std::vector<std::string> &flags = {});
    void MakeBootstrap(const std::vector<std::string> &flags = {});
    void Make(const std::vector<std::string> &flags = {});
    void Install(const std::vector<std::string> &flags = {});
    void Package(const std::vector<std::string> &flags = {});
};


#endif //BM_BUILD_H
