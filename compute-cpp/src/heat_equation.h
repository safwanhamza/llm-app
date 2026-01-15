#pragma once
#include <vector>

struct HeatInput {
    int width;
    int height;
    double diffusion_rate;
    int time_steps;
    double delta_t;
    double delta_x;
};

struct HeatOutput {
    int width;
    int height;
    std::vector<double> data;
};

HeatOutput solve_heat_equation(const HeatInput& input);
