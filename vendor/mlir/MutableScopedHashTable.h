// MutableScopedHashTable.h

#ifndef MUTABLE_SCOPED_HASH_TABLE_H
#define MUTABLE_SCOPED_HASH_TABLE_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/Support/AllocatorBase.h"
#include <cassert>
#include <new>

namespace mlir::tamatch {

template <typename K, typename V, typename KInfo = llvm::DenseMapInfo<K>,
          typename AllocatorTy = llvm::MallocAllocator>
class MutableScopedHashTable;

template <typename K, typename V> class MutableScopedHashTableVal {
  MutableScopedHashTableVal *NextInScope;
  MutableScopedHashTableVal **PrevInScopePtr; // For O(1) removal
  MutableScopedHashTableVal *NextForKey;
  K Key;
  V Val;

  MutableScopedHashTableVal(const K &key, const V &val) : Key(key), Val(val) {}

public:
  const K &getKey() const { return Key; }
  const V &getValue() const { return Val; }
  V &getValue() { return Val; }

  MutableScopedHashTableVal *getNextForKey() { return NextForKey; }
  const MutableScopedHashTableVal *getNextForKey() const { return NextForKey; }
  MutableScopedHashTableVal *getNextInScope() { return NextInScope; }

  template <typename AllocatorTy>
  static MutableScopedHashTableVal *
  Create(MutableScopedHashTableVal *&headPtr,
         MutableScopedHashTableVal *nextForKey, const K &key, const V &val,
         AllocatorTy &Allocator) {
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

  /// Remove this entry from the scope's linked list
  void unlinkFromScope() {
    *PrevInScopePtr = NextInScope;
    if (NextInScope)
      NextInScope->PrevInScopePtr = PrevInScopePtr;
  }

  template <typename AllocatorTy> void Destroy(AllocatorTy &Allocator) {
    this->~MutableScopedHashTableVal();
    Allocator.Deallocate(this);
  }
};

template <typename K, typename V, typename KInfo = llvm::DenseMapInfo<K>,
          typename AllocatorTy = llvm::MallocAllocator>
class MutableScopedHashTableScope {
  MutableScopedHashTable<K, V, KInfo, AllocatorTy> &HT;
  MutableScopedHashTableScope *PrevScope;
  MutableScopedHashTableVal<K, V> *LastValInScope;

public:
  MutableScopedHashTableScope(
      MutableScopedHashTable<K, V, KInfo, AllocatorTy> &HT);
  MutableScopedHashTableScope(MutableScopedHashTableScope &) = delete;
  MutableScopedHashTableScope &
  operator=(MutableScopedHashTableScope &) = delete;
  ~MutableScopedHashTableScope();

  MutableScopedHashTableScope *getParentScope() { return PrevScope; }
  const MutableScopedHashTableScope *getParentScope() const {
    return PrevScope;
  }

private:
  friend class MutableScopedHashTable<K, V, KInfo, AllocatorTy>;

  MutableScopedHashTableVal<K, V> *&getLastValInScopeRef() {
    return LastValInScope;
  }
};

template <typename K, typename V, typename KInfo, typename AllocatorTy>
class MutableScopedHashTable : llvm::detail::AllocatorHolder<AllocatorTy> {
  using AllocTy = llvm::detail::AllocatorHolder<AllocatorTy>;

public:
  using ScopeTy = MutableScopedHashTableScope<K, V, KInfo, AllocatorTy>;
  using size_type = unsigned;

private:
  friend class MutableScopedHashTableScope<K, V, KInfo, AllocatorTy>;

  using ValTy = MutableScopedHashTableVal<K, V>;

  llvm::DenseMap<K, ValTy *, KInfo> TopLevelMap;
  ScopeTy *CurScope = nullptr;

public:
  MutableScopedHashTable() = default;
  MutableScopedHashTable(AllocatorTy A) : AllocTy(A) {}
  MutableScopedHashTable(const MutableScopedHashTable &) = delete;
  MutableScopedHashTable &operator=(const MutableScopedHashTable &) = delete;

  ~MutableScopedHashTable() {
    assert(!CurScope && TopLevelMap.empty() && "Scope imbalance!");
  }

  using AllocTy::getAllocator;

  size_type count(const K &Key) const { return TopLevelMap.count(Key); }

  V lookup(const K &Key) const {
    auto I = TopLevelMap.find(Key);
    if (I != TopLevelMap.end())
      return I->second->getValue();
    return V();
  }

  void insert(const K &Key, const V &Val) {
    assert(CurScope && "No scope active!");
    ValTy *&KeyEntry = TopLevelMap[Key];
    KeyEntry = ValTy::Create(CurScope->getLastValInScopeRef(), KeyEntry, Key,
                             Val, getAllocator());
  }

  /// Erase a key from the table. Returns true if found and erased.
  bool erase(const K &Key) {
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

  ScopeTy *getCurScope() { return CurScope; }
  const ScopeTy *getCurScope() const { return CurScope; }
};

// Scope constructor
template <typename K, typename V, typename KInfo, typename Allocator>
MutableScopedHashTableScope<K, V, KInfo, Allocator>::
    MutableScopedHashTableScope(
        MutableScopedHashTable<K, V, KInfo, Allocator> &ht)
    : HT(ht) {
  PrevScope = HT.CurScope;
  HT.CurScope = this;
  LastValInScope = nullptr;
}

// Scope destructor
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

} // namespace mlir::tamatch

#endif // MUTABLE_SCOPED_HASH_TABLE_H
