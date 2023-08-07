//
// Created by sigsegv on 8/2/23.
//

#ifndef BM_BASICCMD_H
#define BM_BASICCMD_H

#include <string>

class Ports;

int ListGroups(Ports &ports);
int ListPorts(Ports &ports, const std::string &groupName);
int ListBuilds(Ports &ports, const std::string &portName);
int ListInstalled(const std::string &rootDir);

#endif //BM_BASICCMD_H
