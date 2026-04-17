# TableGen generation for Rover passes

include(${PROJECT_SOURCE_DIR}/cmake/TableGenHelpers.cmake)

# ============================================================================
# Rover Passes
# ============================================================================
add_dialect_tablegen(
  NAME Rover
  NAMESPACE rover
  TD_FILE RoverDialect.td
  COMPONENTS Passes
)
