#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include "simulation.grpc.pb.h"

#include "heat_equation.h"
#include "nbody.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using simulation::SimulationService;
using simulation::HeatParams;
using simulation::HeatResult;
using simulation::NBodyParams;
using simulation::NBodyResult;
using simulation::BodyState;

class SimulationServiceImpl final : public SimulationService::Service {
    Status SolveHeatEquation(ServerContext* context, const HeatParams* request,
                  HeatResult* reply) override {
        HeatInput input;
        input.width = request->width();
        input.height = request->height();
        input.diffusion_rate = request->diffusion_rate();
        input.time_steps = request->time_steps();
        input.delta_t = request->delta_t();
        input.delta_x = request->delta_x();

        HeatOutput output = solve_heat_equation(input);

        reply->set_width(output.width);
        reply->set_height(output.height);
        for (double val : output.data) {
            reply->add_data(val);
        }

        return Status::OK;
    }

    Status SimulateNBody(ServerContext* context, const NBodyParams* request,
                         NBodyResult* reply) override {
        NBodyInput input;
        input.num_bodies = request->num_bodies();
        input.time_steps = request->time_steps();
        input.delta_t = request->delta_t();
        input.g_constant = request->g_constant();

        NBodyOutput output = simulate_nbody(input);

        reply->set_steps(output.steps);
        reply->set_num_bodies(output.num_bodies);

        for (const auto& b : output.final_state) {
            auto* bs = reply->add_final_state();
            bs->set_x(b.x);
            bs->set_y(b.y);
            bs->set_mass(b.mass);
            bs->set_vx(b.vx);
            bs->set_vy(b.vy);
        }

        for (double val : output.all_positions) {
            reply->add_all_positions(val);
        }

        return Status::OK;
    }
};

void RunServer() {
    std::string server_address("0.0.0.0:50051");
    SimulationServiceImpl service;

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();
}

int main(int argc, char** argv) {
    RunServer();
    return 0;
}
