include(AddMLIRPython)

add_compile_definitions("MLIR_PYTHON_PACKAGE_PREFIX=mmlir.")


################################################################################
# Sources
################################################################################

declare_mlir_python_sources(TamagoyakiPythonSources)

set(LLVM_TARGET_DEFINITIONS Tamagoyaki.td)
mlir_tablegen(_tamagoyaki_ops_gen.py
  -gen-python-op-bindings -bind-dialect=tamagoyaki
)
set(TAMAGOYAKI_OPS_GEN_TABLEGEN_OUTPUT ${TABLEGEN_OUTPUT})
add_public_tablegen_target(TamagoyakiPythonSources.tamagoyaki.tablegen)
# copy generated files from build dir to here
add_custom_command(
  OUTPUT
    ${CMAKE_CURRENT_SOURCE_DIR}/mmlir/dialects/_tamagoyaki_ops_gen.py
  DEPENDS
    TamagoyakiPythonSources.tamagoyaki.tablegen
  COMMENT
    "copying generated py files from build to src include dir"
  COMMAND
    ${CMAKE_COMMAND} -E copy ${TAMAGOYAKI_OPS_GEN_TABLEGEN_OUTPUT} ${CMAKE_CURRENT_SOURCE_DIR}/mmlir/dialects
)

add_custom_target(copy_generated_py_to_src_include DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mmlir/dialects/_tamagoyaki_ops_gen.py)

declare_mlir_python_sources(TamagoyakiPythonSources.tamagoyaki
  ADD_TO_PARENT TamagoyakiPythonSources
  ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mmlir/dialects"
  SOURCES
    tamagoyaki.py
)

declare_mlir_python_extension(TamagoyakiPythonSources.Extension
  MODULE_NAME _tamagoyaki
  ADD_TO_PARENT TamagoyakiPythonSources
  SOURCES
    TamagoyakiExtension.cpp
  PRIVATE_LINK_LIBS
    LLVMSupport
  EMBED_CAPI_LINK_LIBS
    TamagoyakiCAPI
)

################################################################################
# Common CAPI
################################################################################

add_mlir_python_common_capi_library(TamagoyakiPythonCAPI
  INSTALL_COMPONENT TamagoyakiPythonModules
  INSTALL_DESTINATION python/mmlir/_mlir_libs
  OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/mmlir/_mlir_libs
  # this is passed to mlir_python_setup_extension_rpath
  # it only matters if you distribute the bindings along with
  # the whole distro of mlir. cibuildwheel will correct this anyway.
  # also it's only for stuff like libmlir_utils etc.
  # RELATIVE_INSTALL_ROOT "../../.."
  DECLARED_HEADERS
    MLIRPythonCAPI.HeaderSources
  DECLARED_SOURCES
    TamagoyakiPythonSources
    MLIRPythonExtension.RegisterEverything
    MLIRPythonSources.Core
)

################################################################################
# Instantiation of all Python modules
################################################################################

add_mlir_python_modules(TamagoyakiPythonModules
  ROOT_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/mmlir"
  INSTALL_PREFIX python/mmlir
  DECLARED_SOURCES
    TamagoyakiPythonSources
    MLIRPythonExtension.RegisterEverything
    MLIRPythonSources
  COMMON_CAPI_LINK_LIBS
    TamagoyakiPythonCAPI
)
add_dependencies(TamagoyakiPythonModules copy_generated_py_to_src_include)
