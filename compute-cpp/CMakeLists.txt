cmake_minimum_required(VERSION 3.15)
project(ComputeService)

find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Proto file generation
set(PROTO_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../protos")
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_DIR})

# We expect the docker container to have protoc and grpc_cpp_plugin
find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin)

get_filename_component(sim_proto "${PROTO_SRC_DIR}/simulation.proto" ABSOLUTE)

add_custom_command(
  OUTPUT "${GENERATED_DIR}/simulation.pb.cc"
         "${GENERATED_DIR}/simulation.pb.h"
         "${GENERATED_DIR}/simulation.grpc.pb.cc"
         "${GENERATED_DIR}/simulation.grpc.pb.h"
  COMMAND ${Protobuf_PROTOC_EXECUTABLE}
  ARGS --grpc_out="${GENERATED_DIR}"
       --cpp_out="${GENERATED_DIR}"
       -I "${PROTO_SRC_DIR}"
       --plugin=protoc-gen-grpc="${GRPC_CPP_PLUGIN}"
       "${sim_proto}"
  DEPENDS "${sim_proto}"
)

add_library(simulation_grpc
  ${GENERATED_DIR}/simulation.pb.cc
  ${GENERATED_DIR}/simulation.grpc.pb.cc
)
target_link_libraries(simulation_grpc
  ${Protobuf_LIBRARIES}
  gRPC::grpc++
)
target_include_directories(simulation_grpc PUBLIC ${GENERATED_DIR})

add_executable(compute_service
  src/main.cpp
  src/heat_equation.cpp
  src/nbody.cpp
)

target_link_libraries(compute_service
  simulation_grpc
  ${Protobuf_LIBRARIES}
  gRPC::grpc++
)

target_include_directories(compute_service PRIVATE src)
