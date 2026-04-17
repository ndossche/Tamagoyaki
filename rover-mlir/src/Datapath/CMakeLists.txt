# datapath library

set(COMB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../include/Comb)
set(DATAPATH_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../include/Datapath/)

# ============================================================================
# Datapath Library
# ============================================================================
set(DATAPATH_SOURCES
  DatapathDialect.cpp
)

add_mlir_library(DatapathDialect
  ${DATAPATH_SOURCES}
  PARTIAL_SOURCES_INTENDED

  ADDITIONAL_HEADER_DIRS
  ${DATAPATH_INCLUDE_DIR}
  # ${HW_INCLUDE_DIR}

  DEPENDS
  MLIRDatapathIncGen

  LINK_LIBS PUBLIC
  MLIRIR
  MLIRPass
  MLIRTransforms
  MLIRFuncDialect
  MLIRArithDialect
  MLIRAnalysis
)

target_include_directories(DatapathDialect PUBLIC ${DATAPATH_INCLUDE_DIR})
target_include_directories(DatapathDialect PUBLIC ${CMAKE_BINARY_DIR}/rover-mlir/include/Datapath)
target_include_directories(DatapathDialect PUBLIC ${COMB_INCLUDE_DIR})
target_include_directories(DatapathDialect PUBLIC ${CMAKE_BINARY_DIR}/rover-mlir/include/Comb)

