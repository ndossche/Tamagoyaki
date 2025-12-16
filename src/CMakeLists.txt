add_mlir_dialect_library(
  MLIRPotato
  PotatoDialect.cpp

  # to silence LLVMProcessSources.cmake:116
  PARTIAL_SOURCES_INTENDED

  ADDITIONAL_HEADER_DIRS
  ${PROJECT_SOURCE_DIR}/include

  DEPENDS
  MLIRPotatoIncGen
  copy_generated_td_to_src_include

  LINK_LIBS PUBLIC
  MLIRIR
  MLIRInferTypeOpInterface
  MLIRFuncDialect
  MLIRRegisterAllDialects
  MLIRRegisterAllPasses
)

add_mlir_public_c_api_library(PotatoCAPI
  PotatoCAPI.cpp
  # to silence LLVMProcessSources.cmake:116
  PARTIAL_SOURCES_INTENDED

  LINK_LIBS PUBLIC
  MLIRPotato
)

get_property(dialect_libs GLOBAL PROPERTY MLIR_DIALECT_LIBS)
get_property(conversion_libs GLOBAL PROPERTY MLIR_CONVERSION_LIBS)
set(LIBS ${dialect_libs} ${conversion_libs} MLIROptLib MLIRPotato)

add_llvm_executable(potato-opt potato-opt.cpp)
llvm_update_compile_flags(potato-opt)
target_link_libraries(potato-opt PRIVATE ${LIBS})
add_dependencies(potato-opt MLIRPotatoIncGen)

mlir_check_all_link_libraries(potato-opt)
