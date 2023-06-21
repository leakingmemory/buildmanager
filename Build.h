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

Build GetBuild(Ports &ports, const std::string &buildName, const std::vector<std::string> &flags);

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
    std::string nosysrootLdflags;
    std::string nobootstrapLdflags;
    std::vector<std::string> buildTargets;
    std::vector<std::string> installTargets;
    std::vector<std::vector<std::string>> beforeConfigure;
    std::vector<std::vector<std::string>> beforeBuild;
    std::vector<std::vector<std::string>> postInstall;
    std::vector<std::string> configureParams;
    std::vector<std::string> staticConfigureParams;
    std::vector<std::string> sysrootConfigureParams;
    std::map<std::string,std::string> sysrootEnv;
    std::map<std::string,std::string> nosysrootEnv;
    std::vector<std::string> patches;
    bool configureSkip;
    bool configureDefaultParameters;
    bool configureStaticOverrides;
    bool configureSysrootOverrides;
    bool requiresClang;
    bool valid;

    /* dynamic/user provided: */
    std::vector<std::string> flags;
public:
    Build() : port(), buildfile(), version(), distfiles(), builddir(), prefix(), tooling(), libc(), libcpp(), libcppHeaderBuild(), bootstrap(), staticBootstrap(), cxxflags(), ldflags(), sysrootCxxflags(), sysrootLdflags(), sysrootCmake(), nosysrootLdflags(), nobootstrapLdflags(), buildTargets(), beforeConfigure(), beforeBuild(), postInstall(), configureParams(), staticConfigureParams(), sysrootConfigureParams(), sysrootEnv(), nosysrootEnv(), installTargets(), patches(), configureSkip(false), configureStaticOverrides(false), requiresClang(false), valid(false), flags() {}
    Build(const std::shared_ptr<const Port> &port, path buildfile, const std::vector<std::string> &flags);
private:
    void ReplaceVars(const std::vector<std::string> &flags, std::string &str) const;
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
    void Configure();
    void MakeBootstrap();
    void Make();
    void Install();
    void Package();
};


#endif //BM_BUILD_H
