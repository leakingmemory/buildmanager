//
// Created by sigsegv on 6/14/23.
//

#include "Sysconfig.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>
#include <filesystem>

using path = std::filesystem::path;

Sysconfig::Sysconfig() : cflags(), cxxflags(), ldflags(), cc(), cxx() {
    std::string configfile{"/etc/buildmanager.conf"};
    path configfilePath{};
    configfilePath = configfile;
    if (!exists(configfilePath)) {
        return;
    }
    std::ifstream configfileInput{configfilePath};
    nlohmann::json jsonData{};
    try {
        jsonData = nlohmann::json::parse(configfileInput);
        {
            auto iterator = jsonData.find("cflags");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_string()) {
                    cflags = item;
                }
            }
        }
        {
            auto iterator = jsonData.find("cxxflags");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_string()) {
                    cxxflags = item;
                }
            }
        }
        {
            auto iterator = jsonData.find("ldflags");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_string()) {
                    ldflags = item;
                }
            }
        }
        {
            auto iterator = jsonData.find("cc");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_string()) {
                    cc = item;
                }
            }
        }
        {
            auto iterator = jsonData.find("cxx");
            if (iterator != jsonData.end()) {
                auto &item = *iterator;
                if (item.is_string()) {
                    cxx = item;
                }
            }
        }
    } catch (std::exception &e) {
        std::cerr << "warning: parse config file: " << configfile << ": " << e.what() << "\n";
    }
}