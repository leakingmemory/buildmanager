//
// Created by sigsegv on 3/24/23.
//

#ifndef BM_EXEC_H
#define BM_EXEC_H

#include <filesystem>
#include <vector>
#include <map>

class Exec {
private:
    std::filesystem::path bin;
public:
    Exec(const std::string &name);
    static void setenv(const std::map<std::string,std::string> &env);
    static std::map<std::string,std::string> getenv();
    void exec(const std::vector<std::string> &, const std::map<std::string,std::string> &);
};

#endif //BM_EXEC_H
