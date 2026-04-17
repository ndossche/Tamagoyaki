# TableGen generation for HerbieMLIR dialect

include(${PROJECT_SOURCE_DIR}/cmake/TableGenHelpers.cmake)

# ============================================================================
# HerbieMLIR Dialect
# ============================================================================
add_dialect_tablegen(
  NAME HerbieMLIR
  NAMESPACE herbie
  TD_FILE HerbieMLIRDialect.td
  COMPONENTS Ops Attrs Enums Dialect Passes OpInterfaces
)
