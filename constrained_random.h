#ifndef CONSTRAINED_RANDOM_H
#define CONSTRAINED_RANDOM_H

#include "core.h"
#include <unordered_map>
#include <vector>

struct TestResult {
    int passes  = 0;
    int fails   = 0;
    int skipped = 0;
    int seed;
};

void constrainedRandomTest(Core* cores[], int numCores, int numOps, int seed, TestResult& result);

void runConstrainedRandom(Core* cores[], bus* b, int numCores, int numSeeds, int opsPerSeed);
#endif