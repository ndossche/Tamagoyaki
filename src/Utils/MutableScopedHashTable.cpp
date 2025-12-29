#include "Utils/MutableScopedHashTable.h"
#include "vendor/mlir/SimpleOperationInfo.h"
#include "llvm/Support/RecyclingAllocator.h"

namespace mlir::tamatch {

// MutableScopedHashTableVal implementation

template <typename K, typename V>
template <typename AllocatorTy>
MutableScopedHashTableVal<K, V> *MutableScopedHashTableVal<K, V>::Create(
    MutableScopedHashTableVal *&headPtr, MutableScopedHashTableVal *nextForKey,
    const K &key, const V &val, AllocatorTy &Allocator) {
  MutableScopedHashTableVal *New =
      Allocator.template Allocate<MutableScopedHashTableVal>();
  new (New) MutableScopedHashTableVal(key, val);

  // Insert at head of scope list, maintaining back-pointer
  New->NextInScope = headPtr;
  New->PrevInScopePtr = &headPtr;
  if (headPtr)
    headPtr->PrevInScopePtr = &New->NextInScope;
  headPtr = New;

  New->NextForKey = nextForKey;
  return New;
}

template <typename K, typename V>
void MutableScopedHashTableVal<K, V>::unlinkFromScope() {
  *PrevInScopePtr = NextInScope;
  if (NextInScope)
    NextInScope->PrevInScopePtr = PrevInScopePtr;
}

template <typename K, typename V>
template <typename AllocatorTy>
void MutableScopedHashTableVal<K, V>::Destroy(AllocatorTy &Allocator) {
  this->~MutableScopedHashTableVal();
  Allocator.Deallocate(this);
}

// MutableScopedHashTable implementation

template <typename K, typename V, typename KInfo, typename AllocatorTy>
V MutableScopedHashTable<K, V, KInfo, AllocatorTy>::lookup(const K &Key) const {
  auto I = TopLevelMap.find(Key);
  if (I != TopLevelMap.end())
    return I->second->getValue();
  return V();
}

template <typename K, typename V, typename KInfo, typename AllocatorTy>
void MutableScopedHashTable<K, V, KInfo, AllocatorTy>::insert(const K &Key,
                                                              const V &Val) {
  assert(CurScope && "No scope active!");
  ValTy *&KeyEntry = TopLevelMap[Key];
  KeyEntry = ValTy::Create(CurScope->getLastValInScopeRef(), KeyEntry, Key, Val,
                           getAllocator());
}

template <typename K, typename V, typename KInfo, typename AllocatorTy>
bool MutableScopedHashTable<K, V, KInfo, AllocatorTy>::erase(const K &Key) {
  auto it = TopLevelMap.find(Key);
  if (it == TopLevelMap.end())
    return false;

  ValTy *Entry = it->second;

  // Update TopLevelMap: point to next shadowed value or remove entirely
  if (Entry->getNextForKey()) {
    it->second = Entry->getNextForKey();
  } else {
    TopLevelMap.erase(it);
  }

  // Remove from scope's linked list
  Entry->unlinkFromScope();

  // Deallocate
  Entry->Destroy(getAllocator());
  return true;
}

// MutableScopedHashTableScope implementation

template <typename K, typename V, typename KInfo, typename Allocator>
MutableScopedHashTableScope<K, V, KInfo, Allocator>::
    MutableScopedHashTableScope(
        MutableScopedHashTable<K, V, KInfo, Allocator> &ht)
    : HT(ht) {
  PrevScope = HT.CurScope;
  HT.CurScope = this;
  LastValInScope = nullptr;
}

template <typename K, typename V, typename KInfo, typename Allocator>
MutableScopedHashTableScope<K, V, KInfo,
                            Allocator>::~MutableScopedHashTableScope() {
  assert(HT.CurScope == this && "Scope imbalance!");
  HT.CurScope = PrevScope;

  while (MutableScopedHashTableVal<K, V> *ThisEntry = LastValInScope) {
    if (!ThisEntry->getNextForKey()) {
      assert(HT.TopLevelMap[ThisEntry->getKey()] == ThisEntry &&
             "Scope imbalance!");
      HT.TopLevelMap.erase(ThisEntry->getKey());
    } else {
      MutableScopedHashTableVal<K, V> *&KeyEntry =
          HT.TopLevelMap[ThisEntry->getKey()];
      assert(KeyEntry == ThisEntry && "Scope imbalance!");
      KeyEntry = ThisEntry->getNextForKey();
    }

    LastValInScope = ThisEntry->getNextInScope();
    ThisEntry->Destroy(HT.getAllocator());
  }
}

// Explicit instantiation
using AllocatorTy = llvm::RecyclingAllocator<
    llvm::BumpPtrAllocator,
    MutableScopedHashTableVal<Operation *, Operation *>>;

template class MutableScopedHashTable<Operation *, Operation *,
                                      SimpleOperationInfo, AllocatorTy>;
template class MutableScopedHashTableScope<Operation *, Operation *,
                                           SimpleOperationInfo, AllocatorTy>;

} // namespace mlir::tamatch
