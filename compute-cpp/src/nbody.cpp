#include "nbody.h"
#include <cmath>
#include <random>

NBodyOutput simulate_nbody(const NBodyInput& input) {
    int n = input.num_bodies;
    std::vector<Body> bodies(n);
    std::vector<double> all_positions;
    all_positions.reserve(n * input.time_steps * 2);

    // Initialize bodies randomly
    std::mt19937 gen(42);
    std::uniform_real_distribution<> pos_dist(-100.0, 100.0);
    std::uniform_real_distribution<> vel_dist(-1.0, 1.0);
    std::uniform_real_distribution<> mass_dist(1.0, 10.0);

    for (int i = 0; i < n; ++i) {
        bodies[i].x = pos_dist(gen);
        bodies[i].y = pos_dist(gen);
        bodies[i].vx = vel_dist(gen);
        bodies[i].vy = vel_dist(gen);
        bodies[i].mass = mass_dist(gen);
    }

    for (int t = 0; t < input.time_steps; ++t) {
        // Record positions
        for (const auto& b : bodies) {
            all_positions.push_back(b.x);
            all_positions.push_back(b.y);
        }

        // Compute forces
        std::vector<double> fx(n, 0.0);
        std::vector<double> fy(n, 0.0);

        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                double dx = bodies[j].x - bodies[i].x;
                double dy = bodies[j].y - bodies[i].y;
                double dist_sq = dx * dx + dy * dy + 1e-9; // Softening parameter
                double dist = std::sqrt(dist_sq);
                double f = input.g_constant * bodies[i].mass * bodies[j].mass / dist_sq;

                double f_x = f * dx / dist;
                double f_y = f * dy / dist;

                fx[i] += f_x;
                fy[i] += f_y;
                fx[j] -= f_x;
                fy[j] -= f_y;
            }
        }

        // Update positions and velocities
        for (int i = 0; i < n; ++i) {
            bodies[i].vx += (fx[i] / bodies[i].mass) * input.delta_t;
            bodies[i].vy += (fy[i] / bodies[i].mass) * input.delta_t;
            bodies[i].x += bodies[i].vx * input.delta_t;
            bodies[i].y += bodies[i].vy * input.delta_t;
        }
    }

    return {input.time_steps, n, bodies, all_positions};
}
