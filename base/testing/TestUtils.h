#ifndef BASE_TESTING_TESTUTILS_H_
#define BASE_TESTING_TESTUTILS_H_

#include <gmock/gmock.h>

#include <regex>
#include <string>

MATCHER_P(MatchesStdRegex, regStr, std::string("contains regular expression: ") + regStr) {
    std::regex reg(regStr);
    return std::regex_match(arg, reg);
}

#endif  // BASE_TESTING_TESTUTILS_H_
