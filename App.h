//
// Created by sigsegv on 8/1/23.
//

#ifndef BM_APP_H
#define BM_APP_H

#include <string>
#include <vector>

class Ports;

class App {
protected:
    std::string cmd{};
    std::vector<std::string> args{};
public:
    App(int argc, const char * const argv[]);
    int main();
    virtual int usage() = 0;
    virtual void ConsumeCmd(std::vector<std::string> &args) = 0;
    virtual bool IsValidCmd() = 0;
    virtual int RunCmd(Ports &ports, std::vector<std::string> &args) = 0;
};


#endif //BM_APP_H
