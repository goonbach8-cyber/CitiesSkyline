cmake_minimum_required(VERSION 3.20)
project(CitiesSkylineCpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

if(EMSCRIPTEN)
  set(PLATFORM "Web" CACHE STRING "" FORCE)
endif()

FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5
)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

add_executable(CitiesSkylineCpp
  src/main.cpp
  src/CameraController.cpp
  src/City.cpp
  src/BuildingRenderer.cpp
)

target_include_directories(CitiesSkylineCpp PRIVATE src)
target_link_libraries(CitiesSkylineCpp PRIVATE raylib)

if(EMSCRIPTEN)
  set_target_properties(CitiesSkylineCpp PROPERTIES
    SUFFIX ".html"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/web"
  )

  target_link_options(CitiesSkylineCpp PRIVATE
    "-sUSE_GLFW=3"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sEXPORTED_FUNCTIONS=[\"_main\",\"_CitySetTool\",\"_CitySetSpeed\",\"_CityMoney\",\"_CityPopulation\",\"_CityFilledJobs\",\"_CityTotalJobs\",\"_CityNetMonthly\",\"_CityPowerUsed\",\"_CityPowerCapacity\",\"_CityWaterUsed\",\"_CityWaterCapacity\",\"_CityHappiness\",\"_CityResidentialDemand\",\"_CityCommercialDemand\",\"_CityIndustrialDemand\",\"_CityLevel\",\"_CityScore\",\"_CityMilestoneTarget\",\"_CitySpeed\",\"_CitySelectedTool\",\"_CityVehicleCount\",\"_CityDay\",\"_CityMonth\",\"_CityYear\"]"
    "-sEXPORTED_RUNTIME_METHODS=[\"ccall\"]"
    "--shell-file" "${CMAKE_SOURCE_DIR}/web/shell.html"
  )
endif()
