import grpc
from concurrent import futures
import random
import logging

# Import generated classes
import sys
import os

# Add generated directory to path
sys.path.append(os.path.join(os.path.dirname(__file__), '../generated'))

from protos import ai_pb2
from protos import ai_pb2_grpc
from protos import simulation_pb2

class OptimizerService(ai_pb2_grpc.OptimizerServiceServicer):
    def OptimizeHeatParams(self, request, context):
        print(f"Optimizing for Heat Goal: {request.target_property} = {request.desired_value}")

        # Simple heuristic optimization logic
        # For example, if target_property is "fast_diffusion", we want high diffusion rate

        width = 100
        height = 100
        time_steps = 100
        delta_t = 0.1
        delta_x = 1.0
        diffusion_rate = 1.0

        if request.target_property == "fast_diffusion":
            diffusion_rate = 5.0
            time_steps = 50
        elif request.target_property == "stable":
            diffusion_rate = 0.5
            delta_t = 0.05

        return simulation_pb2.HeatParams(
            width=width,
            height=height,
            diffusion_rate=diffusion_rate,
            time_steps=time_steps,
            delta_t=delta_t,
            delta_x=delta_x
        )

    def OptimizeNBodyParams(self, request, context):
        print(f"Optimizing for NBody Goal: {request.target_behavior}")

        num_bodies = request.body_count if request.body_count > 0 else 100
        time_steps = 200
        delta_t = 0.01
        g_constant = 1.0

        if request.target_behavior == "minimize_collisions":
             # Lower G or smaller time steps might help simulation stability
            g_constant = 0.5
            delta_t = 0.005
        elif request.target_behavior == "high_activity":
            g_constant = 5.0
            delta_t = 0.02

        return simulation_pb2.NBodyParams(
            num_bodies=num_bodies,
            time_steps=time_steps,
            delta_t=delta_t,
            g_constant=g_constant
        )

def serve():
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    ai_pb2_grpc.add_OptimizerServiceServicer_to_server(OptimizerService(), server)
    server.add_insecure_port('[::]:50052')
    print("AI Service listening on port 50052")
    server.start()
    server.wait_for_termination()

if __name__ == '__main__':
    logging.basicConfig()
    serve()
