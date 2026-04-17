# TableGen generation for HW dialect

include(${PROJECT_SOURCE_DIR}/cmake/TableGenHelpers.cmake)

# ============================================================================
# HW Dialect
# ============================================================================
add_dialect_tablegen(
  NAME HW
  NAMESPACE hw
  TD_FILE HWDialect.td
  COMPONENTS Ops Attrs Enums Dialect Passes
)
