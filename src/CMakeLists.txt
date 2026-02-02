add_mlir_dialect_library(
  MLIREquivalence
  EquivalenceDialect.cpp

  # to silence LLVMProcessSources.cmake:116
  PARTIAL_SOURCES_INTENDED

  ADDITIONAL_HEADER_DIRS
  ${PROJECT_SOURCE_DIR}/include

  DEPENDS
  MLIREquivalenceIncGen
  copy_equivalence_generated_td_to_src_include

  LINK_LIBS PUBLIC
  MLIRIR
  MLIRInferTypeOpInterface
  MLIRFuncDialect
  MLIRRegisterAllDialects
  MLIRRegisterAllPasses
)

add_mlir_dialect_library(
  MLIREmatch
  EmatchDialect.cpp
  Utils/MutableScopedHashTable.cpp
  Utils/ClassOpUnionFind.cpp
  Utils/HashConsPatternRewriter.cpp

  # to silence LLVMProcessSources.cmake:116
  PARTIAL_SOURCES_INTENDED

  ADDITIONAL_HEADER_DIRS
  ${PROJECT_SOURCE_DIR}/include

  DEPENDS
  MLIREmatchIncGen
  copy_ematch_generated_td_to_src_include

  LINK_LIBS PUBLIC
  MLIRIR
  MLIRRegisterAllDialects
)

add_mlir_public_c_api_library(EquivalenceCAPI
  EquivalenceCAPI.cpp
  # to silence LLVMProcessSources.cmake:116
  PARTIAL_SOURCES_INTENDED

  LINK_LIBS PUBLIC
  MLIREquivalence
)

get_property(dialect_libs GLOBAL PROPERTY MLIR_DIALECT_LIBS)
get_property(conversion_libs GLOBAL PROPERTY MLIR_CONVERSION_LIBS)
set(LIBS
    ${dialect_libs}
    ${conversion_libs}
    MLIROptLib
    MLIREquivalence
    MLIREmatch
)

add_llvm_executable(tamagoyaki-opt tamagoyaki-opt.cpp)
llvm_update_compile_flags(tamagoyaki-opt)
target_link_libraries(tamagoyaki-opt PRIVATE ${LIBS})
add_dependencies(tamagoyaki-opt MLIREquivalenceIncGen MLIREmatchIncGen)

mlir_check_all_link_libraries(tamagoyaki-opt)
