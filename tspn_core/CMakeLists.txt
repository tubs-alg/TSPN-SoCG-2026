add_library(
  tspn_core
  node.cpp
  common.cpp
  soc.cpp
  relaxed_solution.cpp
  strategies/branching_strategy.cpp
  strategies/root_node_strategy.cpp
  strategies/order_filtering.cpp
  utils/geometry.cpp
  utils/drawer.cpp
  details/cgal_conversion.cpp
  details/cgal_operations.cpp
  details/pre_simplify.cpp
  details/convex_decomposition.cpp
  details/convex_partition.cpp
  details/lazy_trajectory.cpp
)
target_include_directories(tspn_core PUBLIC ${CMAKE_SOURCE_DIR})
set_target_properties(tspn_core PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(tspn_core PRIVATE gurobi::gurobi)
target_link_libraries(tspn_core PRIVATE ${CGAL_TARGET})
target_link_libraries(tspn_core PRIVATE ${Boost_LIBRARIES})
