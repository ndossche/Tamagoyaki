llvm_canonicalize_cmake_booleans(MLIR_ENABLE_BINDINGS_PYTHON)

# this configures the adjacent lit.site.cfg.py.in
set(TAMAGOYAKI_BINARY_DIR ${PROJECT_BINARY_DIR})
set(TAMAGOYAKI_SOURCE_DIR ${PROJECT_SOURCE_DIR})
set(TAMAGOYAKI_TEST_LIT_CFG ${CMAKE_CURRENT_SOURCE_DIR}/lit.cfg.py)
configure_lit_site_cfg(
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.py.in
  ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg.py MAIN_CONFIG
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.cfg.py)

set(TAMAGOYAKI_TEST_DEPENDS
    FileCheck
    count
    not
    tamagoyaki-opt)

add_lit_testsuite(
  check-tamagoyaki "Running the tamagoyaki regression tests"
  ${CMAKE_CURRENT_BINARY_DIR} DEPENDS ${TAMAGOYAKI_TEST_DEPENDS})
set_target_properties(check-tamagoyaki PROPERTIES FOLDER "Tests")

add_lit_testsuites(TAMAGOYAKI ${CMAKE_CURRENT_SOURCE_DIR} DEPENDS
                   ${TAMAGOYAKI_TEST_DEPENDS})
