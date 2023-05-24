#ifndef MINISQL_TABLE_ITERATOR_H
#define MINISQL_TABLE_ITERATOR_H

#include "common/rowid.h"
#include "record/row.h"
#include "transaction/transaction.h"

class TableHeap;

class TableIterator {
public:
  // you may define your own constructor based on your member variables
  explicit TableIterator();

  TableIterator(const TableIterator &other);

  TableIterator(TableHeap *table_heap, Row * row, Transaction *txn);

  virtual ~TableIterator();

  inline bool operator==(const TableIterator &itr) const;

  inline bool operator!=(const TableIterator &itr) const;

  const Row &operator*();

  Row *operator->();

  TableIterator &operator=(const TableIterator &itr) noexcept;

  TableIterator &operator++();

  TableIterator operator++(int);

private:
  // add your own private member variables here
  TableHeap *table_heap; //The table heap pointer
  Row *row; //Traverse the each row in the table
  Transaction *txn; //Used for the ++ operation
};

#endif  // MINISQL_TABLE_ITERATOR_H
