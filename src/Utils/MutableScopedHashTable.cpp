// MutableScopedHashTable.cpp

#include "Utils/MutableScopedHashTable.h"
#include "mlir/IR/Operation.h"
#include "vendor/mlir/SimpleOperationInfo.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/RecyclingAllocator.h"

namespace mlir::ematch {

// Explicit instantiations
using AllocatorTy = llvm::RecyclingAllocator<
    llvm::BumpPtrAllocator,
    MutableScopedHashTableVal<Operation *, Operation *>>;

template class MutableScopedHashTableVal<Operation *, Operation *>;
template class MutableScopedHashTableScope<Operation *, Operation *,
                                           SimpleOperationInfo, AllocatorTy>;
template class MutableScopedHashTable<Operation *, Operation *,
                                      SimpleOperationInfo, AllocatorTy>;

} // namespace mlir::ematch
