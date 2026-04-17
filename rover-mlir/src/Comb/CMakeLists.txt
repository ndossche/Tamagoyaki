# comb library

set(COMB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../include/Comb)
set(HW_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../include/)

# ============================================================================
# Comb Library
# ============================================================================
set(COMB_SOURCES
  CombDialect.cpp
)

add_mlir_library(Comb
  ${COMB_SOURCES}
  PARTIAL_SOURCES_INTENDED

  ADDITIONAL_HEADER_DIRS
  ${COMB_INCLUDE_DIR}
  # ${HW_INCLUDE_DIR}

  DEPENDS
  MLIRCombIncGen

  LINK_LIBS PUBLIC
  MLIRIR
  MLIRPass
  MLIRTransforms
  MLIRFuncDialect
  MLIRArithDialect
  MLIRAnalysis
)

target_include_directories(Comb PUBLIC ${COMB_INCLUDE_DIR})
target_include_directories(Comb PUBLIC ${CMAKE_BINARY_DIR}/rover-mlir/include/Comb)
target_include_directories(Comb PUBLIC ${HW_INCLUDE_DIR})
target_include_directories(Comb PUBLIC ${CMAKE_BINARY_DIR}/rover-mlir/include)
