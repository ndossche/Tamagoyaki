# TableGen generation for Comb dialect

include(${PROJECT_SOURCE_DIR}/cmake/TableGenHelpers.cmake)

# ============================================================================
# Comb Dialect
# ============================================================================
add_dialect_tablegen(
  NAME Comb
  NAMESPACE comb
  TD_FILE CombDialect.td
  COMPONENTS Ops Attrs Dialect
)
