#include "heat_equation.h"
#include <vector>
#include <algorithm>

HeatOutput solve_heat_equation(const HeatInput& input) {
    int w = input.width;
    int h = input.height;
    std::vector<double> u(w * h, 0.0);
    std::vector<double> u_new(w * h, 0.0);

    // Initialize with a heat source in the center
    int center_x = w / 2;
    int center_y = h / 2;
    int radius = std::min(w, h) / 10;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int dx = x - center_x;
            int dy = y - center_y;
            if (dx*dx + dy*dy < radius*radius) {
                u[y * w + x] = 100.0;
            }
        }
    }

    double r = input.diffusion_rate * input.delta_t / (input.delta_x * input.delta_x);

    for (int t = 0; t < input.time_steps; ++t) {
        for (int y = 1; y < h - 1; ++y) {
            for (int x = 1; x < w - 1; ++x) {
                int idx = y * w + x;
                u_new[idx] = u[idx] + r * (
                    u[y * w + (x + 1)] + u[y * w + (x - 1)] +
                    u[(y + 1) * w + x] + u[(y - 1) * w + x] -
                    4 * u[idx]
                );
            }
        }
        u = u_new;
    }

    return {w, h, u};
}
