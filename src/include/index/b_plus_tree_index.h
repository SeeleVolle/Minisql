#ifndef MINISQL_B_PLUS_TREE_INDEX_H
#define MINISQL_B_PLUS_TREE_INDEX_H

#include "index/b_plus_tree.h"
#include "index/generic_key.h"
#include "index/index.h"

class BPlusTreeIndex : public Index {
 public:
  BPlusTreeIndex(index_id_t index_id, IndexSchema *key_schema, size_t key_size, BufferPoolManager *buffer_pool_manager);

  KeyManager GetKeyManager(){
    return processor_;
  }

  dberr_t InsertEntry(const Row &key, RowId row_id, Transaction *txn) override;

  dberr_t RemoveEntry(const Row &key, RowId row_id, Transaction *txn) override;

  dberr_t ScanKey(const Row &key, std::vector<RowId> &result, Transaction *txn, string compare_operator = "=") override;

  dberr_t Destroy() override;

  IndexIterator GetBeginIterator();

  IndexIterator GetBeginIterator(GenericKey *key);

  IndexIterator GetEndIterator();

  bool IsEmpty(){
    return container_.IsEmpty();
  }

//  bool SetBPlusTree(BPlusTree *tree)
//  {
//      if(tree == nullptr)
//          return false;
//      container_ = *tree;
//      return true;
//  }

 protected:
  // comparator for key
  KeyManager processor_;
  // container
  BPlusTree container_;
};

#endif  // MINISQL_B_PLUS_TREE_INDEX_H
