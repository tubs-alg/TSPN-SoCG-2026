# This is the root file for CMake, which is used to define the native part of
# the project. It is completely independent of the Python-part and can actually
# be used just to build a C++-library.
cmake_minimum_required(VERSION 3.23)
option(CXX "enable C++ compilation" ON)

if(CXX)
  enable_language(CXX)
endif()
project(tspn CXX) # Feel free to add more project information

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POSITION_INDEPENDENT_CODE ON) # The code needs to be compiled as PIC
                                        # to build the shared lib for python.
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
include(./cmake/CCache.cmake) # Magic to speed up compiling by caching.

# ~~~
# CPM Dependencies ~~~~~~~~~~~~~~~~
# CPM allows us to include some simple dependencies without much fuss.
# Great default, for everything more complicated, uses conan.
# Why not always use conan? CPM works without any interaction (conan needs
# to be called externally). CPM also has no problems with install PyBind11
# (which conan has at the time).
# If the dependency has complicated dependencies on its own or is slow to
# compile, better use conan.

# ~~~
# Conan dependencies ~~~~~~~~~~~~~~~~~~~~~
# We are using the `cmake_find_package` generator to create `FindXXX`-files
# in the `cmake`-folder. This is only necessary, if the packages are not
# already installed. This approach is not without its problems:
# PRO: Simple option that is compatible with scikit-build. Can work without conan.
# CON: Difficult if you want to use changing conan settings.

# run `conan install .. --build=missing` in `.conan`.
# list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/.conan")
# Also need to be defined in `./conanfile.txt`.
find_package(fmt REQUIRED)
find_package(doctest REQUIRED)
find_package(nlohmann_json REQUIRED CONFIG)
find_package(CGAL REQUIRED CONFIG)
find_package(gurobi REQUIRED CONFIG)
find_package(Boost REQUIRED CONFIG)
include_directories(${GUROBI_INCLUDE_DIRS})
#find_package(Boost REQUIRED COMPONENTS thread system)

# Probe for the correct CGAL target name (CGAL::CGAL vs cgal::cgal)
if(TARGET CGAL::CGAL)
    set(CGAL_TARGET CGAL::CGAL)
    message(STATUS "Using CGAL target: CGAL::CGAL")
elseif(TARGET cgal::cgal)
    set(CGAL_TARGET cgal::cgal)
    message(STATUS "Using CGAL target: cgal::cgal")
else()
    message(FATAL_ERROR "Neither CGAL::CGAL nor cgal::cgal target found")
endif()

# ~~~
# System dependencies ~~~~~~~~~~~~~~~~~~~~~~
# These files have to be installed on the system

# ~~~
# C++-Library ~~~~~~~~~~~~~~~~~~~~~~~~~
# This part defines the actual C++-library that may be used as a simple
# C++-library. The Python-interface to this library is defined separately.
add_subdirectory(src)

# ~~~
# TESTS OF THE PUBLIC INTERFACE ~~~~~~~~~~~~~~
# A good programmer will add at least some unit tests.
# The tests-folder can also be used for Python-tests, without interference.
if(NOT SKBUILD)
  # only import if exists. The production code may be shipped without.
  add_subdirectory(tests) # defined in a separate CMakeLists.txt
endif()

# ~~~
# Python Interface ~~~~~~~~~~~~~~~~~~~~
# The Python-bindings are compiled as separate target to allow the independent
# usage of the C++-library.
add_subdirectory(pysrc) # defined in a separate CMakeLists.txt