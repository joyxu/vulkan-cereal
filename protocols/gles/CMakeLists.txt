add_executable(
  gfxstream-gles-generator
  ApiGen.cpp EntryPoint.cpp main.cpp Parser.cpp strUtils.cpp TypeFactory.cpp)
target_include_directories(
  gfxstream-gles-generator
  PRIVATE
  ../..)
