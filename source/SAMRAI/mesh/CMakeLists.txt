set (mesh_headers
  BalanceBoxBreaker.h
  BalanceUtilities.h
  BergerRigoutsos.h
  BergerRigoutsosNode.h
  BoxGeneratorStrategy.h
  BoxInTransit.h
  BoxTransitSet.h
  CascadePartitioner.h
  CascadePartitionerTree.h
  ChopAndPackLoadBalancer.h
  GraphLoadBalancer.h
  GriddingAlgorithm.h
  GriddingAlgorithmConnectorWidthRequestor.h
  GriddingAlgorithmStrategy.h
  LoadBalanceStrategy.h
  MultiblockGriddingTagger.h
  PartitioningParams.h
  SpatialKey.h
  StandardTagAndInitialize.h
  StandardTagAndInitializeConnectorWidthRequestor.h
  StandardTagAndInitStrategy.h
  TagAndInitializeStrategy.h
  TileClustering.h
  TransitLoad.h
  TreeLoadBalancer.h
  VoucherTransitLoad.h)

set (mesh_sources
  BalanceBoxBreaker.C
  BalanceUtilities.C
  BergerRigoutsos.C
  BergerRigoutsosNode.C
  BoxGeneratorStrategy.C
  BoxTransitSet.C
  CascadePartitioner.C
  CascadePartitionerTree.C
  ChopAndPackLoadBalancer.C
  GraphLoadBalancer.C
  GriddingAlgorithm.C
  GriddingAlgorithmConnectorWidthRequestor.C
  GriddingAlgorithmStrategy.C
  LoadBalanceStrategy.C
  MultiblockGriddingTagger.C
  PartitioningParams.C
  SpatialKey.C
  StandardTagAndInitialize.C
  StandardTagAndInitializeConnectorWidthRequestor.C
  StandardTagAndInitStrategy.C
  TagAndInitializeStrategy.C
  TileClustering.C
  TransitLoad.C
  TreeLoadBalancer.C
  VoucherTransitLoad.C
  ${CMAKE_CURRENT_BINARY_DIR}/fortran/mesh_coarsentags1d.f
  ${CMAKE_CURRENT_BINARY_DIR}/fortran/mesh_coarsentags2d.f
  ${CMAKE_CURRENT_BINARY_DIR}/fortran/mesh_coarsentags3d.f)

process_m4(NAME fortran/mesh_coarsentags1d)
process_m4(NAME fortran/mesh_coarsentags2d)
process_m4(NAME fortran/mesh_coarsentags3d)

set (mesh_depends_on
  SAMRAI_hier
  SAMRAI_tbox
  SAMRAI_xfer
  boost)

if (ENABLE_CUDA)
  set (mesh_depends_on
    ${mesh_depends_on}
    cuda
    raja)
endif ()

blt_add_library(
  NAME SAMRAI_mesh
  SOURCES ${mesh_sources}
  HEADERS ${mesh_headers}
  HEADERS_OUTPUT_SUBDIR SAMRAI/mesh
  DEPENDS_ON ${mesh_depends_on})

install(TARGETS
  SAMRAI_mesh
  RUNTIME DESTINATION lib
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)

target_include_directories(
  SAMRAI_mesh
  PUBLIC ${PROJECT_SOURCE_DIR}/source)