# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "MinSizeRel")
  file(REMOVE_RECURSE
  "CMakeFiles\\appCASM_emulator_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\appCASM_emulator_autogen.dir\\ParseCache.txt"
  "appCASM_emulator_autogen"
  )
endif()
