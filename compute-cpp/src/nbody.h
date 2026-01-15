#pragma once
#include <vector>

struct NBodyInput {
    int num_bodies;
    int time_steps;
    double delta_t;
    double g_constant;
};

struct Body {
    double x, y;
    double vx, vy;
    double mass;
};

struct NBodyOutput {
    int steps;
    int num_bodies;
    std::vector<Body> final_state;
    std::vector<double> all_positions;
};

NBodyOutput simulate_nbody(const NBodyInput& input);
