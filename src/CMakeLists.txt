add_library(
  tspn
  node.cpp
  branching_strategies/branching_strategy.cpp
  relaxed_solution.cpp
  utils/geometry.cpp
  utils/drawer.cpp
  details/cgal_conversion.cpp
  details/cgal_operations.cpp
  details/pre_simplify.cpp
  details/convex_decomposition.cpp
  details/convex_partition.cpp
  details/lazy_trajectory.cpp
  strategies/order_filtering.cpp
  root_node_strategies/triple_node_strategies.cpp
)
target_sources(
  tspn
  PUBLIC ../include/tspn/types.h
         ../include/tspn/common.h
         ../include/tspn/soc.h
  PRIVATE ./soc.cpp
          ./common.cpp
)
target_include_directories(tspn PUBLIC ../include)
#target_compile_options(
#  tspn PRIVATE "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall>")
set_target_properties(tspn PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(tspn PRIVATE gurobi::gurobi)
target_link_libraries(tspn PRIVATE ${CGAL_TARGET})
target_link_libraries(tspn PRIVATE ${Boost_LIBRARIES})
