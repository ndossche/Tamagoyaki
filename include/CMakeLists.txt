# Tama dialect
set(dialect Tamagoyaki)
set(dialect_namespace tama)
set(LLVM_TARGET_DEFINITIONS ${dialect}Dialect.td)

mlir_tablegen(${dialect}Ops.h.inc -gen-op-decls)
set(OPS_H_INC_TABLEGEN_OUTPUT ${TABLEGEN_OUTPUT})
mlir_tablegen(${dialect}Ops.cpp.inc -gen-op-defs)
set(OPS_CPP_INC_TABLEGEN_OUTPUT ${TABLEGEN_OUTPUT})
mlir_tablegen(${dialect}Types.h.inc -gen-typedef-decls
              -typedefs-dialect=${dialect_namespace})
set(TYPES_H_INC_TABLEGEN_OUTPUT ${TABLEGEN_OUTPUT})
mlir_tablegen(${dialect}Types.cpp.inc -gen-typedef-defs
              -typedefs-dialect=${dialect_namespace})
set(TYPES_CPP_INC_TABLEGEN_OUTPUT ${TABLEGEN_OUTPUT})
mlir_tablegen(${dialect}Dialect.h.inc -gen-dialect-decls
              -dialect=${dialect_namespace})
set(DIALECT_H_INC_TABLEGEN_OUTPUT ${TABLEGEN_OUTPUT})
mlir_tablegen(${dialect}Dialect.cpp.inc -gen-dialect-defs
              -dialect=${dialect_namespace})
set(DIALECT_CPP_INC_TABLEGEN_OUTPUT ${TABLEGEN_OUTPUT})
mlir_tablegen(TamagoyakiPasses.h.inc --gen-pass-decls)
set(PASS_H_INC_TABLEGEN_OUTPUT ${TABLEGEN_OUTPUT})

add_public_tablegen_target(MLIR${dialect}IncGen)

set(tama_generated_files
  ${CMAKE_CURRENT_SOURCE_DIR}/${dialect}Ops.h.inc
  ${CMAKE_CURRENT_SOURCE_DIR}/${dialect}Ops.cpp.inc
  ${CMAKE_CURRENT_SOURCE_DIR}/${dialect}Types.h.inc
  ${CMAKE_CURRENT_SOURCE_DIR}/${dialect}Types.cpp.inc
  ${CMAKE_CURRENT_SOURCE_DIR}/${dialect}Dialect.h.inc
  ${CMAKE_CURRENT_SOURCE_DIR}/${dialect}Dialect.cpp.inc
  ${CMAKE_CURRENT_SOURCE_DIR}/${dialect}TamagoyakiPasses.h.inc
)

# copy generated files from build dir to here
add_custom_command(
  OUTPUT
    ${tama_generated_files}
  DEPENDS
    MLIR${dialect}IncGen
  COMMENT
    "copying generated td files from build to src include dir"
  COMMAND
    ${CMAKE_COMMAND} -E copy ${OPS_H_INC_TABLEGEN_OUTPUT} ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND
    ${CMAKE_COMMAND} -E copy ${OPS_CPP_INC_TABLEGEN_OUTPUT} ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND
    ${CMAKE_COMMAND} -E copy ${TYPES_H_INC_TABLEGEN_OUTPUT} ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND
    ${CMAKE_COMMAND} -E copy ${TYPES_CPP_INC_TABLEGEN_OUTPUT} ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND
    ${CMAKE_COMMAND} -E copy ${DIALECT_H_INC_TABLEGEN_OUTPUT} ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND
    ${CMAKE_COMMAND} -E copy ${DIALECT_CPP_INC_TABLEGEN_OUTPUT} ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND
    ${CMAKE_COMMAND} -E copy ${PASS_H_INC_TABLEGEN_OUTPUT} ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_target(copy_tamagoyaki_generated_td_to_src_include DEPENDS ${tama_generated_files})

# Tamatch dialect
set(dialect Tamatch)
set(dialect_namespace tamatch)
set(LLVM_TARGET_DEFINITIONS ${dialect}Dialect.td)

mlir_tablegen(${dialect}Ops.h.inc -gen-op-decls)
set(OPS_H_INC_TABLEGEN_OUTPUT ${TABLEGEN_OUTPUT})
mlir_tablegen(${dialect}Ops.cpp.inc -gen-op-defs)
set(OPS_CPP_INC_TABLEGEN_OUTPUT ${TABLEGEN_OUTPUT})
mlir_tablegen(${dialect}Dialect.h.inc -gen-dialect-decls
              -dialect=${dialect_namespace})
set(DIALECT_H_INC_TABLEGEN_OUTPUT ${TABLEGEN_OUTPUT})
mlir_tablegen(${dialect}Dialect.cpp.inc -gen-dialect-defs
              -dialect=${dialect_namespace})
set(DIALECT_CPP_INC_TABLEGEN_OUTPUT ${TABLEGEN_OUTPUT})

add_public_tablegen_target(MLIR${dialect}IncGen)

set(tamatch_generated_files
  ${CMAKE_CURRENT_SOURCE_DIR}/${dialect}Ops.h.inc
  ${CMAKE_CURRENT_SOURCE_DIR}/${dialect}Ops.cpp.inc
  ${CMAKE_CURRENT_SOURCE_DIR}/${dialect}Dialect.h.inc
  ${CMAKE_CURRENT_SOURCE_DIR}/${dialect}Dialect.cpp.inc
)

# copy generated files from build dir to here
add_custom_command(
  OUTPUT
    ${tamatch_generated_files}
  DEPENDS
    MLIR${dialect}IncGen
  COMMENT
    "copying generated td files from build to src include dir"
  COMMAND
    ${CMAKE_COMMAND} -E copy ${OPS_H_INC_TABLEGEN_OUTPUT} ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND
    ${CMAKE_COMMAND} -E copy ${OPS_CPP_INC_TABLEGEN_OUTPUT} ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND
    ${CMAKE_COMMAND} -E copy ${DIALECT_H_INC_TABLEGEN_OUTPUT} ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND
    ${CMAKE_COMMAND} -E copy ${DIALECT_CPP_INC_TABLEGEN_OUTPUT} ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_target(copy_tamatch_generated_td_to_src_include DEPENDS ${tamatch_generated_files})
