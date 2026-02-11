#ifndef EQUIVALENCE_UTILS_H
#define EQUIVALENCE_UTILS_H

#include "EquivalenceDialect.h"

#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Support/LogicalResult.h"
#include "llvm/ADT/StringRef.h"
#include <cstdint>

namespace mlir::equivalence {

/// Transform a function by wrapping its body in a GraphOp.
/// If insertSingleElementEqs is true, all values are wrapped in ClassOps.
/// Returns success if the transformation was successful.
LogicalResult insertGraphInFunction(func::FuncOp funcOp,
                                    bool insertSingleElementEqs);

/// Run greedy cost-based selection on a single GraphOp.
/// Assigns min_cost_index attributes to ClassOps based on minimum-cost
/// operands.
void selectGreedy(GraphOp graphOp, int64_t defaultCost,
                  llvm::StringRef costAttributeName = "equivalence.cost");

/// Clear all selection state from a GraphOp: remove min_cost_index from
/// ClassOps and the cost attribute from all other operations.
void clearSelection(GraphOp graphOp,
                    llvm::StringRef costAttributeName = "equivalence.cost");

/// Extract selected nodes from equivalence classes in a GraphOp.
/// For each ClassOp with min_cost_index set, replaces its result with the
/// selected operand, erases non-selected operand ops, and removes the cost
/// attribute. ClassOps with no uses are erased entirely. ClassOps without
/// min_cost_index are left untouched.
void extractFromGraph(GraphOp graphOp);

/// Inline a GraphOp by splicing its body into the parent block, replacing
/// graph results with yielded values, and erasing the GraphOp.
void inlineGraphOp(GraphOp graphOp);

} // namespace mlir::equivalence

#endif // EQUIVALENCE_UTILS_H
