add_executable(
  gfxstream-generic-apigen
  ApiGen.cpp EntryPoint.cpp main.cpp Parser.cpp strUtils.cpp TypeFactory.cpp)
target_include_directories(gfxstream-generic-apigen PRIVATE ../..)
