//
// Created by sigsegv on 8/8/23.
//

#include "versioncmp.h"

constexpr char delimiters[] = {'.', ',', '-'};

constexpr int DelimiterNum(char ch) {
    typedef typeof(sizeof(delimiters)) size_type;
    for (size_type i = 0; i < sizeof(delimiters); i++) {
        if (delimiters[i] == ch) {
            return (int) i;
        }
    }
    return -1;
}

constexpr bool IsDelimiter(char ch) {
    typedef typeof(sizeof(delimiters)) size_type;
    for (size_type i = 0; i < sizeof(delimiters); i++) {
        if (delimiters[i] == ch) {
            return true;
        }
    }
    return false;
}

constexpr int DelimiterCompare(char a, char b) {
    if (a == b) {
        return 0;
    }
    return DelimiterNum(b) - DelimiterNum(a);
}

constexpr std::string::size_type OtherCharactersLength(const std::string &str) {
    for (std::string::size_type i = 0; i < str.size(); i++) {
        typeof(str.at(i)) ch = str.at(i);
        if (ch >= '0' && ch <= '9') {
            return i;
        }
        if (IsDelimiter(ch)) {
            return i;
        }
    }
    return str.size();
}

constexpr std::string::size_type IntLength(const std::string &str) {
    for (std::string::size_type i = 0; i < str.size(); i++) {
        typeof(str.at(i)) ch = str.at(i);
        if (ch < '0' || ch > '9') {
            return i;
        }
    }
    return str.size();
}

static_assert(IntLength("") == 0);
static_assert(IntLength("a0a") == 0);
static_assert(IntLength("0a") == 1);
static_assert(IntLength("1b") == 1);
static_assert(IntLength("090c") == 3);

constexpr long StrToL(const std::string &str) {
    long result = 0;
    for (std::string::size_type i = 0; i < str.size(); i++) {
        result = result * 10;
        result += str.at(i) - '0';
    }
    return result;
}

static_assert(StrToL("0") == 0);
static_assert(StrToL("1") == 1);
static_assert(StrToL("10") == 10);
static_assert(StrToL("101") == 101);
static_assert(StrToL("0101") == 101);

constexpr int versioncmp_ce(const std::string &a, const std::string &b) {
    std::string str1{a};
    std::string str2{b};
    while (!str1.empty() && !str2.empty()) {
        long v1;
        long v2;
        bool valid1;
        bool valid2;
        {
            size_t s1;
            size_t s2;
            {
                s1 = IntLength(str1);
                v1 = StrToL(str1.substr(0, s1));
                s2 = IntLength(str2);
                v2 = StrToL(str2.substr(0, s2));
            }
            valid1 = (s1 > 0);
            valid2 = (s2 > 0);
            if (valid1) {
                str1 = str1.substr(s1);
            }
            if (valid2) {
                str2 = str2.substr(s2);
            }
        }
        if (valid1) {
            if (valid2) {
                if (v1 > v2) {
                    return 1;
                } else if (v1 < v2) {
                    return -1;
                }
            } else {
                return 1;
            }
        } else if (valid2) {
            return -1;
        } else {
            char s1ch0 = str1.at(0);
            char s2ch0 = str2.at(0);
            if (IsDelimiter(s1ch0)) {
                if (IsDelimiter(s2ch0)) {
                    if (s1ch0 != s2ch0) {
                        return DelimiterCompare(s1ch0, s2ch0);
                    }
                    str1 = str1.substr(1);
                    str2 = str2.substr(1);
                } else {
                    return 1;
                }
            } else if (IsDelimiter(s2ch0)) {
                return -1;
            } else {
                auto l1 = OtherCharactersLength(str1);
                auto l2 = OtherCharactersLength(str2);
                auto s1 = str1.substr(0, l1);
                str1 = str1.substr(l1);
                auto s2 = str2.substr(0, l2);
                str2 = str2.substr(l2);
                if (s1 != s2) {
                    return s1.compare(s2);
                }
            }
        }
    }
    if (str1.empty()) {
        if (str2.empty()) {
            return 0;
        }
        typeof(str2.at(0)) ch0 = str2.at(0);
        if (IsDelimiter(ch0)) {
            return -1;
        }
        return 1;
    }
    typeof(str1.at(0)) ch0 = str1.at(0);
    if (IsDelimiter(ch0)) {
        return 1;
    }
    return -1;
}

static_assert(versioncmp_ce("1", "1") == 0);
static_assert(versioncmp_ce("1.0.1a", "1.0.1") != 0);
static_assert(versioncmp_ce("1.0.1a", "1.0.1") < 0);
static_assert(versioncmp_ce("1.0.1", "1.0.1a") > 0);
static_assert(versioncmp_ce("1.0.1-r1", "1.0.1-r2") != 0);

constexpr bool consistent_vcompare(const std::string &first, const std::string &second) {
    if (versioncmp_ce(first, second) >= 0) {
        return false;
    }
    return versioncmp_ce(second, first) > 0;
}

static_assert(consistent_vcompare("1", "2"));
static_assert(consistent_vcompare("0.0.1", "0.0.2"));
static_assert(consistent_vcompare("0.0.1,1", "0.0.2"));
static_assert(consistent_vcompare("0.0.1", "0.0.1,1"));
static_assert(consistent_vcompare("1.0", "1.0-r1"));
static_assert(consistent_vcompare("1.0", "1.0.1a"));
static_assert(consistent_vcompare("1.0", "1.0.1b"));
static_assert(consistent_vcompare("1.0", "1.0.1"));
static_assert(consistent_vcompare("1.0", "1.0.1-r1"));
static_assert(consistent_vcompare("1.0", "1.1"));
static_assert(consistent_vcompare("1.0", "2.0"));
static_assert(consistent_vcompare("1.0-r1", "1.0.1a"));
static_assert(consistent_vcompare("1.0-r1", "1.0.1b"));
static_assert(consistent_vcompare("1.0-r1", "1.0.1"));
static_assert(consistent_vcompare("1.0-r1", "1.0.1-r1"));
static_assert(consistent_vcompare("1.0-r1", "1.1"));
static_assert(consistent_vcompare("1.0-r1", "2.0"));
static_assert(consistent_vcompare("1.0.1a", "1.0.1b"));
static_assert(consistent_vcompare("1.0.1a", "1.0.1"));
static_assert(consistent_vcompare("1.0.1a", "1.0.1-r1"));
static_assert(consistent_vcompare("1.0.1a", "1.0.1-r2"));
static_assert(consistent_vcompare("1.0.1a", "1.1"));
static_assert(consistent_vcompare("1.0.1a", "2.0"));
static_assert(consistent_vcompare("1.0.1b", "1.0.1"));
static_assert(consistent_vcompare("1.0.1b", "1.0.1-r1"));
static_assert(consistent_vcompare("1.0.1b", "1.0.1-r2"));
static_assert(consistent_vcompare("1.0.1b", "1.1"));
static_assert(consistent_vcompare("1.0.1b", "2.0"));
static_assert(consistent_vcompare("1.0.1", "1.0.1-r1"));
static_assert(consistent_vcompare("1.0.1", "1.0.1-r2"));
static_assert(consistent_vcompare("1.0.1", "1.1"));
static_assert(consistent_vcompare("1.0.1", "2.0"));
static_assert(consistent_vcompare("1.0.1-r1", "1.0.1-r2"));
static_assert(consistent_vcompare("1.0.1-r1", "1.1"));
static_assert(consistent_vcompare("1.0.1-r1", "2.0"));
static_assert(consistent_vcompare("1.0.1-r2", "1.1"));
static_assert(consistent_vcompare("1.0.1-r2", "2.0"));
static_assert(consistent_vcompare("1.1", "2.0"));

int versioncmp(const std::string &a, const std::string &b) {
    return versioncmp_ce(a, b);
}
