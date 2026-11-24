// Core tests: each area file in tests/core/ holds its TEST()s; main.cpp runs them all (or the ones named on the
// command line), each with a new, empty data folder ($JOHN_HOME, `home`).
#pragma once
#undef NDEBUG  // the checks are asserts: keep them in Release builds
#include "john.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <random>
#include <regex>
#include <set>
#include <stdexcept>
#include <tuple>
#include <vector>
#include <miniz.h>
#ifdef _WIN32
#define setenv(k, v, overwrite) _putenv_s(k, v)
#define unsetenv(k) _putenv_s(k, "")
#endif

using namespace john;

struct Test { const char* name; void (*run)(); };
std::vector<Test>& tests();  // every TEST(), in the order the files register them
#define TEST(name)                                                                       \
  static void test_##name();                                                             \
  static const bool registered_##name = (tests().push_back({#name, test_##name}), true); \
  static void test_##name()

extern fs::path home;  // the running test's data folder ($JOHN_HOME); it doesn't exist until something creates it

// The jig several tests share (abc_import checks how it imports): roll, grace cut, tuplet, rest, tie, voltas, key
inline const char* const JIG = "X:1\nT:Test Jig\nM:6/8\nL:1/8\nK:D\n|:~A3 {g}AFA|d2e f2d|(3efg a2 z|1 d3- d2:|2 d3 d2B||\n";

inline const Event& nth(const Song& s, int n) {  // n-th note/rest (skipping bars)
  for (auto& e : s.events) if (e.kind != Kind::Bar && n-- == 0) return e;
  assert(false);
  return s.events[0];
}
