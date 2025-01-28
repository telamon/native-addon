cmake_minimum_required(VERSION 3.25)

find_package(cmake-bare REQUIRED PATHS node_modules/cmake-bare)
find_package(cmake-napi REQUIRED PATHS node_modules/cmake-napi)

project(native_addon C)

add_bare_module(addon_bare)

target_sources(
  ${addon_bare}
  PRIVATE
    binding.c
)

add_napi_module(addon_napi)

target_sources(
  ${addon_napi}
  PRIVATE
    binding.c
)

resolve_node_module(bare-compat-napi compat)

target_include_directories(
  ${addon_napi}
  PRIVATE
    "${compat}/include"
)
