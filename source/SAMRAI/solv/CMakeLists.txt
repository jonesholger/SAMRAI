set (solv_headers
  CartesianRobinBcHelper.h
  CellPoissonFACOps.h
  CellPoissonFACSolver.h
  CellPoissonHypreSolver.h
  CVODEAbstractFunctions.h
  CVODESolver.h
  FACOperatorStrategy.h
  FACPreconditioner.h
  GhostCellRobinBcCoefs.h
  KINSOL_SAMRAIContext.h
  KINSOLAbstractFunctions.h
  KINSOLSolver.h
  LocationIndexRobinBcCoefs.h
  NonlinearSolverStrategy.h
  nvector.h
  PETSc_SAMRAIVectorReal.h
  PETSc_SAMRAIVectorReal.C
  PETScAbstractVectorReal.h
  PETScAbstractVectorReal.C
  PoissonSpecifications.h
  RobinBcCoefStrategy.h
  SAMRAIVectorReal.h
  SAMRAIVectorReal.C
  SimpleCellRobinBcCoefs.h
  SNES_SAMRAIContext.h
  SNESAbstractFunctions.h
  solv_NVector.h
  Sundials_SAMRAIVector.h
  SundialsAbstractVector.h)

set_source_files_properties(
  PETScAbstractVectorReal.C
  PETSc_SAMRAIVectorReal.C
  SAMRAIVectorReal.C
  PROPERTIES HEADER_FILE_ONLY TRUE)

set (solv_sources
  CVODEAbstractFunctions.C
  CVODESolver.C
  CartesianRobinBcHelper.C
  CellPoissonFACOps.C
  CellPoissonFACSolver.C
  CellPoissonHypreSolver.C
  FACOperatorStrategy.C
  FACPreconditioner.C
  GhostCellRobinBcCoefs.C
  KINSOLAbstractFunctions.C
  KINSOLSolver.C
  KINSOL_SAMRAIContext.C
  LocationIndexRobinBcCoefs.C
  NonlinearSolverStrategy.C
  PoissonSpecifications.C
  RobinBcCoefStrategy.C
  SNESAbstractFunctions.C
  SNES_SAMRAIContext.C
  SimpleCellRobinBcCoefs.C
  SundialsAbstractVector.C
  Sundials_SAMRAIVector.C
  solv_NVector.C
  fortran/solv_cartesianrobinbchelper2d.f
  fortran/solv_cartesianrobinbchelper3d.f
  fortran/solv_cellpoissonfacops2d.f
  fortran/solv_cellpoissonfacops3d.f
  fortran/solv_cellpoissonhypresolver2d.f
  fortran/solv_cellpoissonhypresolver3d.f)

set (solv_depends_on
  SAMRAI_hier
  SAMRAI_math
  SAMRAI_tbox
  SAMRAI_xfer)

if (ENABLE_HYPRE)
  list (APPEND solv_depends_on HYPRE)
endif()

if (ENABLE_SUNDIALS)
  list (APPEND solv_depends_on SUNDIALS)
endif()

if (ENABLE_PETSC)
  list (APPEND solv_depends_on PETSc)
endif()

blt_add_library(
  NAME SAMRAI_solv
  SOURCES ${solv_sources}
  HEADERS ${solv_headers}
  DEPENDS_ON ${solv_depends_on})

set_target_properties(SAMRAI_solv PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories(
  SAMRAI_solv
  PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source>
  $<INSTALL_INTERFACE:include>)

install(TARGETS SAMRAI_solv
  EXPORT SAMRAITargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES ${solv_headers}
  DESTINATION include/SAMRAI/solv)
