# TableGen generation for Datapath dialect

include(${PROJECT_SOURCE_DIR}/cmake/TableGenHelpers.cmake)

# ============================================================================
# Datapath Dialect
# ============================================================================
add_dialect_tablegen(
  NAME Datapath
  NAMESPACE datapath
  TD_FILE DatapathDialect.td
  COMPONENTS Ops Attrs Dialect Passes
)
