#include "storage/table_iterator.h"

#include "common/macros.h"
#include "storage/table_heap.h"

/**
 * TODO: Student Implement
 */
TableIterator::TableIterator() {
  this->table_heap = NULL;
  this->row = NULL;
  this->txn = NULL;
}

TableIterator::TableIterator(const TableIterator &other)
{
  this->table_heap = other.table_heap;
  this->row = other.row;
  this->txn = other.txn;
}

TableIterator::TableIterator(TableHeap *table_heap, Row * row, Transaction *txn) {
  this->table_heap = table_heap;
  this->row = row;
  this->txn = txn;
}

TableIterator::~TableIterator() {

}


const Row &TableIterator::operator*() {
  return *row;
}

Row *TableIterator::operator->() {
  return row;
}

TableIterator &TableIterator::operator=(const TableIterator &itr) noexcept {
  this->table_heap = itr.table_heap;
  this->row = itr.row;
  return *this;
}

// ++iter
TableIterator &TableIterator::operator++() {
  TablePage *now_page = reinterpret_cast<TablePage *>(this->table_heap->buffer_pool_manager_->FetchPage(this->row->GetRowId().GetPageId()));
  RowId *next_rowid = new RowId();
  if (now_page->GetNextTupleRid(row->GetRowId(), next_rowid))
  {
    this->row = new Row(*next_rowid);
    now_page->GetTuple(this->row, this->table_heap->schema_, this->txn, this->table_heap->lock_manager_);
  }
    //if it is the last row of the now_page, then we should find the next page
  else{
    page_id_t next_page_id = now_page->GetNextPageId();
    now_page = reinterpret_cast<TablePage *>(this->table_heap->buffer_pool_manager_->FetchPage(next_page_id));
    now_page->GetFirstTupleRid(next_rowid);
    this->row = new Row(*next_rowid);
    now_page->GetTuple(this->row, this->table_heap->schema_, this->txn, this->table_heap->lock_manager_);
  }
  return *this;
}

// iter++
TableIterator TableIterator::operator++(int) {
  TableIterator old = *this;
  TablePage *now_page = reinterpret_cast<TablePage *>(this->table_heap->buffer_pool_manager_->FetchPage(this->row->GetRowId().GetPageId()));
  RowId *next_rowid = new RowId();
  page_id_t next_page_id;

  if (now_page->GetNextTupleRid(row->GetRowId(), next_rowid))
  {
    this->row = new Row(*next_rowid);
    now_page->GetTuple(this->row, this->table_heap->schema_, this->txn, this->table_heap->lock_manager_);
  }
  //if it is the last row of the now_page, then we should find the next page
  else if((next_page_id = now_page->GetNextPageId()) != INVALID_PAGE_ID){
    now_page = reinterpret_cast<TablePage *>(this->table_heap->buffer_pool_manager_->FetchPage(next_page_id));
    now_page->GetFirstTupleRid(next_rowid);
    this->row = new Row(*next_rowid);
    now_page->GetTuple(this->row, this->table_heap->schema_, this->txn, this->table_heap->lock_manager_);
  }
  //If it is the last row of the last page, then we should find the end iterator
  else{
    this->row = nullptr;
    this->table_heap = nullptr;
  }
  return old;
}
