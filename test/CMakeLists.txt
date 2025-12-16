llvm_canonicalize_cmake_booleans(MLIR_ENABLE_BINDINGS_PYTHON)

# this configures the adjacent lit.site.cfg.py.in
set(POTATO_BINARY_DIR ${PROJECT_BINARY_DIR})
set(POTATO_SOURCE_DIR ${PROJECT_SOURCE_DIR})
configure_lit_site_cfg(
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.py.in
  ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg.py MAIN_CONFIG
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.cfg.py)

set(POTATO_TEST_DEPENDS
    FileCheck
    count
    not
    potato-opt)

if(MLIR_ENABLE_BINDINGS_PYTHON)
    list(APPEND POTATO_TEST_DEPENDS PotatoPythonModules)
endif()

add_lit_testsuite(
  check-potato "Running the potato regression tests"
  ${CMAKE_CURRENT_BINARY_DIR} DEPENDS ${POTATO_TEST_DEPENDS})
set_target_properties(check-potato PROPERTIES FOLDER "Tests")

add_lit_testsuites(POTATO ${CMAKE_CURRENT_SOURCE_DIR} DEPENDS
                   ${POTATO_TEST_DEPENDS})
