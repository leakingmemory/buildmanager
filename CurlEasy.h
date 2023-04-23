//
// Created by sigsegv on 3/17/23.
//

#ifndef BM_CURLEASY_H
#define BM_CURLEASY_H

#include <string>
#include <functional>
#include <optional>

typedef void CURL;

enum class Curlsocktype {
    IPCXN, ACCEPT
};

class CurlEasy;

class CurlEasyCallbacks {
    friend CurlEasy;
private:
    CurlEasy &ce;
    CurlEasyCallbacks(CurlEasy &ce) : ce(ce) {}
public:
    size_t writefunc(char *ptr, size_t size, size_t nmemb);
    size_t readfunc(char *buffer, size_t size, size_t nitems);
    int seekfunc(off_t offset, int origin);
    int sockoptfunc(int curlfd, Curlsocktype purpose);
    int xferinfofunc(off_t dltotal, off_t dlnow, off_t ultotal, off_t ulnow);
};

class CurlEasy : private CurlEasyCallbacks {
    friend CurlEasyCallbacks;
private:
    CURL *handle;
    constexpr static size_t writeError = 0xFFFFFFFF;
    std::function<size_t (const void *, size_t)> write;

    void SetupHandleDatapointers();
public:
    CurlEasy();
    ~CurlEasy();
    CurlEasy(CurlEasy &&) noexcept;
    CurlEasy(const CurlEasy &);
    void Swap(CurlEasy &other) {
        CURL *tmp = other.handle;
        other.handle = handle;
        handle = tmp;
    }
    CurlEasy &operator =(CurlEasy &&) noexcept;
    CurlEasy &operator =(const CurlEasy &);
    void SetUrl(const std::string &url);
    void SetWriteCallback(const std::function<size_t (const void *, size_t)> &write);
    bool Perform();
    long GetResponseCode();
    std::optional<std::string> PotentialRedirectUrl();
};


#endif //BM_CURLEASY_H
