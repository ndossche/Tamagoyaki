include(AddMLIRPython)

add_compile_definitions("MLIR_PYTHON_PACKAGE_PREFIX=mmlir.")


################################################################################
# Sources
################################################################################

declare_mlir_python_sources(PotatoPythonSources)

set(LLVM_TARGET_DEFINITIONS Potato.td)
mlir_tablegen(_potato_ops_gen.py
  -gen-python-op-bindings -bind-dialect=potato
)
set(POTATO_OPS_GEN_TABLEGEN_OUTPUT ${TABLEGEN_OUTPUT})
add_public_tablegen_target(PotatoPythonSources.potato.tablegen)
# copy generated files from build dir to here
add_custom_command(
  OUTPUT
    ${CMAKE_CURRENT_SOURCE_DIR}/mmlir/dialects/_potato_ops_gen.py
  DEPENDS
    PotatoPythonSources.potato.tablegen
  COMMENT
    "copying generated py files from build to src include dir"
  COMMAND
    ${CMAKE_COMMAND} -E copy ${POTATO_OPS_GEN_TABLEGEN_OUTPUT} ${CMAKE_CURRENT_SOURCE_DIR}/mmlir/dialects
)

add_custom_target(copy_generated_py_to_src_include DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mmlir/dialects/_potato_ops_gen.py)

declare_mlir_python_sources(PotatoPythonSources.potato
  ADD_TO_PARENT PotatoPythonSources
  ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mmlir/dialects"
  SOURCES
    potato.py
)

declare_mlir_python_extension(PotatoPythonSources.Extension
  MODULE_NAME _potato
  ADD_TO_PARENT PotatoPythonSources
  SOURCES
    PotatoExtension.cpp
  PRIVATE_LINK_LIBS
    LLVMSupport
  EMBED_CAPI_LINK_LIBS
    PotatoCAPI
)

################################################################################
# Common CAPI
################################################################################

add_mlir_python_common_capi_library(PotatoPythonCAPI
  INSTALL_COMPONENT PotatoPythonModules
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
    PotatoPythonSources
    MLIRPythonExtension.RegisterEverything
    MLIRPythonSources.Core
)

################################################################################
# Instantiation of all Python modules
################################################################################

add_mlir_python_modules(PotatoPythonModules
  ROOT_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/mmlir"
  INSTALL_PREFIX python/mmlir
  DECLARED_SOURCES
    PotatoPythonSources
    MLIRPythonExtension.RegisterEverything
    MLIRPythonSources
  COMMON_CAPI_LINK_LIBS
    PotatoPythonCAPI
)
add_dependencies(PotatoPythonModules copy_generated_py_to_src_include)
