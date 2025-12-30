// MutableScopedHashTable.h

#ifndef MUTABLE_SCOPED_HASH_TABLE_H
#define MUTABLE_SCOPED_HASH_TABLE_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseMapInfo.h"
#include "llvm/Support/AllocatorBase.h"
#include <cassert>

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
         AllocatorTy &Allocator);

  /// Remove this entry from the scope's linked list
  void unlinkFromScope();

  template <typename AllocatorTy> void Destroy(AllocatorTy &Allocator);
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

  MutableScopedHashTable(const MutableScopedHashTable &) = delete;
  MutableScopedHashTable &operator=(const MutableScopedHashTable &) = delete;

  ~MutableScopedHashTable() {
    assert(!CurScope && TopLevelMap.empty() && "Scope imbalance!");
  }

  using AllocTy::getAllocator;

  size_type count(const K &Key) const { return TopLevelMap.count(Key); }

  V lookup(const K &Key) const;

  void insert(const K &Key, const V &Val);

  /// Erase a key from the table. Returns true if found and erased.
  bool erase(const K &Key);

  ScopeTy *getCurScope() { return CurScope; }
  const ScopeTy *getCurScope() const { return CurScope; }
};

} // namespace mlir::tamatch

#endif // MUTABLE_SCOPED_HASH_TABLE_H
