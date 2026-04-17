# HW library

set(HW_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../include/HW/)

# ============================================================================
# HW Library
# ============================================================================
set(HW_SOURCES
  HWDialect.cpp
)

add_mlir_library(HWDialect
  ${HW_SOURCES}
  PARTIAL_SOURCES_INTENDED

  ADDITIONAL_HEADER_DIRS
  ${HW_INCLUDE_DIR}

  DEPENDS
  MLIRHWIncGen

  LINK_LIBS PUBLIC
  MLIRIR
  MLIRPass
  MLIRTransforms
  MLIRFuncDialect
  MLIRArithDialect
  MLIRAnalysis
)

target_include_directories(HWDialect PUBLIC ${HW_INCLUDE_DIR})
target_include_directories(HWDialect PUBLIC ${CMAKE_BINARY_DIR}/rover-mlir/include/HW)
