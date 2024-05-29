#pragma once

#include <random>

class RandomEngine {
  public:
    RandomEngine(int min, int max) : distribution(min, max) {
      static std::random_device rd;
      rng.seed(rd());
    }

    int32_t randInt() {
      return distribution(rng);
    }

  private:
    std::default_random_engine rng;
    std::uniform_int_distribution<int> distribution;
};
