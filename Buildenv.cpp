//
// Created by sigsegv on 4/10/23.
//

#include "Buildenv.h"
#include "Sysconfig.h"
#include <cctype>
#include <algorithm>

Buildenv::Buildenv(const Sysconfig &sysconfig, const std::string &cxxflags, const std::string &ldflags, const std::string &sysrootCxxflags, const std::string &sysrootLdflags, const std::string &nosysrootLdflags, const std::string &nobootstrapLdflags, bool requiresClang, bool bootstrappingBuild) : cflags(), cxxflags(cxxflags), ldflags(ldflags), sysrootCxxflags(sysrootCxxflags), sysrootLdflags(sysrootLdflags), nosysrootLdflags(nosysrootLdflags), nobootstrapLdflags(nobootstrapLdflags), sysroot(), cc(), cxx(), requiresClang(requiresClang), bootstrappingBuild(bootstrappingBuild) {
    sysconfig.AppendCflags(this->cflags);
    sysconfig.AppendCxxflags(this->cxxflags);
    sysconfig.AppendLdflags(this->ldflags);
    if (!bootstrappingBuild && !nobootstrapLdflags.empty()) {
        this->ldflags.append(" ");
        this->ldflags.append(nobootstrapLdflags);
    }
    cc = sysconfig.GetCc();
    cxx = sysconfig.GetCxx();
}

void Buildenv::FilterEnv(std::map<std::string,std::string> &env) {
    std::map<std::string,std::string>::iterator end = env.end();
    std::map<std::string,std::string>::iterator bootstrap_sysroot = end;
    std::map<std::string,std::string>::iterator libcpp_bootstrap = end;
    std::map<std::string,std::string>::iterator cflags_iter = end;
    std::map<std::string,std::string>::iterator cxxflags_iter = end;
    std::map<std::string,std::string>::iterator ldflags_iter = end;
    std::map<std::string,std::string>::iterator ld_library_path_iter = end;
    std::map<std::string,std::string>::iterator cc_iter = end;
    std::map<std::string,std::string>::iterator cxx_iter = end;
    auto iterator = env.begin();
    while (iterator != end) {
        auto key = iterator->first;
        std::transform(key.begin(), key.end(), key.begin(), [] (auto c) { return std::tolower(c); });
        if (key == "sysroot") {
            bootstrap_sysroot = iterator;
            this->sysroot = bootstrap_sysroot->second;
        } else if (key == "libcpp_bootstrap") {
            libcpp_bootstrap = iterator;
        } else if (key == "cflags") {
            cflags_iter = iterator;
        } else if (key == "cxxflags") {
            cxxflags_iter = iterator;
        } else if (key == "ldflags") {
            ldflags_iter = iterator;
        } else if (key == "ld_library_path") {
            ld_library_path_iter = iterator;
        } else if (key == "cc") {
            cc_iter = iterator;
        } else if (key == "cxx") {
            cxx_iter = iterator;
        }
        ++iterator;
    }
    std::string cflags{};
    std::string cxxflags{};
    std::string ldflags{};
    std::string ld_library_path{};
    std::string cc{};
    std::string cxx{};
    if (cflags_iter != end) {
        cflags = cflags_iter->second;
    }
    if (cxxflags_iter != end) {
        cxxflags = cxxflags_iter->second;
    }
    if (ldflags_iter != end) {
        ldflags = ldflags_iter->second;
    }
    if (ld_library_path_iter != end) {
        ld_library_path = ld_library_path_iter->second;
    }
    if (cc_iter != end) {
        cc = cc_iter->second;
    }
    if (cxx_iter != end) {
        cxx = cxx_iter->second;
    }
    bool add_cflags{false};
    bool add_cxxflags{false};
    bool add_ldflags{false};
    bool add_ld_library_path{false};
    bool add_cc{false};
    bool add_cxx{false};
    if (requiresClang) {
        if (cc.find("clang") == std::string::npos) {
            cc = "clang";
            add_cc = true;
        }
        if (cxx.find("clang") == std::string::npos) {
            cxx = "clang++";
            add_cxx = true;
        }
    }
    if (!this->cc.empty() && cc.empty()) {
        cc = this->cc;
        add_cc = true;
    }
    if (!this->cxx.empty() && cxx.empty()) {
        cxx = this->cxx;
        add_cxx = true;
    }
    if (bootstrap_sysroot != end) {
        std::string musl_cflags = " --sysroot=";
        musl_cflags.append(bootstrap_sysroot->second);
        musl_cflags.append(" -static-libgcc ");
//        musl_cflags.append(" --rtlib=compiler-rt ");
        auto musl_cxxflags = musl_cflags;
        musl_cxxflags.append(" -isystem ");
        musl_cxxflags.append(bootstrap_sysroot->second);
        musl_cxxflags.append("/usr/include ");
        musl_cflags.append(cflags);
        musl_cxxflags.append(cxxflags);
        cflags = musl_cflags;
        cxxflags = musl_cxxflags;

        add_cflags = true;
        add_cxxflags = true;

        std::string musl_libdir = bootstrap_sysroot->second;
        musl_libdir.append("/lib");
        std::string musl = musl_libdir;
        musl.append("/libc.so");
        std::string musl_ldflags = " -g -L";
        musl_ldflags.append(musl_libdir);
        musl_ldflags.append(" -L");
        musl_ldflags.append(bootstrap_sysroot->second);
        musl_ldflags.append("/usr/lib64 -L");
        musl_ldflags.append(bootstrap_sysroot->second);
        musl_ldflags.append("/usr/lib -Wl,--sysroot=");
        musl_ldflags.append(bootstrap_sysroot->second);
        musl_ldflags.append(" -Wl,--dynamic-linker=");
        musl_ldflags.append(musl);
        musl_ldflags.append(" -static-libgcc ");
        //musl_ldflags.append(" --rtlib=compiler-rt ");
        musl_ldflags.append(ldflags);
        ldflags = musl_ldflags;
        add_ldflags = true;

        std::string muslld = musl_libdir;
        muslld.append(":");
        muslld.append(bootstrap_sysroot->second);
        muslld.append("/usr/lib64");
        muslld.append(":");
        muslld.append(bootstrap_sysroot->second);
        muslld.append("/usr/lib");
        if (!ld_library_path.empty()) {
            muslld.append(":");
            muslld.append(ld_library_path);
        }
        ld_library_path = muslld;
        add_ld_library_path = true;
    }
    if (libcpp_bootstrap != end) {
        std::string musl_cxxflags{" -g"};
        //musl_cxxflags.append(" -stdlib=libc++");
        musl_cxxflags.append(" -isystem ");
        musl_cxxflags.append(bootstrap_sysroot->second);
        musl_cxxflags.append("/usr/include/c++/v1 ");
        musl_cxxflags.append(cxxflags);
        musl_cxxflags.append(" -static-libgcc ");
        //musl_cxxflags.append(" --rtlib=compiler-rt ");
        //musl_cxxflags.append(" -Wl,-lc++ -Wl,-lc -Wl,-nostdlib -nostdlib -Wl,");
        //musl_cxxflags.append(bootstrap_sysroot->second);
        //musl_cxxflags.append("/lib/crt1.o -Wl,");
        //musl_cxxflags.append(bootstrap_sysroot->second);
        //musl_cxxflags.append("/lib/crti.o -Wl,");
        //musl_cxxflags.append(bootstrap_sysroot->second);
        //musl_cxxflags.append("/lib/crtn.o ");
        cxxflags = musl_cxxflags;

        add_cxxflags = true;
    }
    if (!(this->cflags.empty())) {
        cflags.append(" ");
        cflags.append(this->cflags);
        add_cflags = true;
    }
    if (!(this->cxxflags.empty())) {
        cxxflags.append(" ");
        cxxflags.append(this->cxxflags);
        add_cxxflags = true;
    }
    if (!(this->ldflags.empty())) {
        ldflags.append(" ");
        ldflags.append(this->ldflags);
        add_ldflags = true;
    }
    if (!this->sysroot.empty() && !this->sysrootCxxflags.empty()) {
        cxxflags.append(" ");
        cxxflags.append(this->sysrootCxxflags);
        add_cxxflags = true;
    }
    if (this->sysroot.empty()) {
        if (!this->nosysrootLdflags.empty()) {
            ldflags.append(" ");
            ldflags.append(this->nosysrootLdflags);
            add_ldflags = true;
        }
    } else {
        if (!this->sysrootLdflags.empty()) {
            ldflags.append(" ");
            ldflags.append(this->sysrootLdflags);
            add_ldflags = true;
        }
    }
    if (add_cflags && cflags_iter != end) {
        cflags_iter->second = cflags;
        add_cflags = false;
    }
    if (add_cxxflags && cxxflags_iter != end) {
        cxxflags_iter->second = cxxflags;
        add_cxxflags = false;
    }
    if (add_ldflags && ldflags_iter != end) {
        ldflags_iter->second = ldflags;
        add_ldflags = false;
    }
    if (add_ld_library_path && ld_library_path_iter != end) {
        ld_library_path_iter->second = ld_library_path;
        add_ld_library_path = false;
    }
    if (add_cc && cc_iter != end) {
        cc_iter->second = cc;
        add_cc = false;
    }
    if (add_cxx && cxx_iter != end) {
        cxx_iter->second = cxx;
        add_cxx = false;
    }
    /* iterators invalid mark */
    if (add_cflags) {
        env.insert_or_assign("CFLAGS", cflags);
    }
    if (add_cxxflags) {
        env.insert_or_assign("CXXFLAGS", cxxflags);
    }
    if (add_ldflags) {
        env.insert_or_assign("LDFLAGS", ldflags);
    }
    if (add_ld_library_path) {
        env.insert_or_assign("LD_LIBRARY_PATH", ld_library_path);
    }
    if (add_cc) {
        env.insert_or_assign("CC", cc);
    }
    if (add_cxx) {
        env.insert_or_assign("CXX", cxx);
    }
}
