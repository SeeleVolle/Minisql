#include "storage/table_heap.h"

/**
 * TODO: Student Implement
 */
bool TableHeap::InsertTuple(Row &row, Transaction *txn) {
  //There is no page in the page chaintable
  if(this->GetFirstPageId() == INVALID_PAGE_ID){
    page_id_t new_page_id;
    TablePage *newpage = reinterpret_cast<TablePage *>(buffer_pool_manager_->NewPage(new_page_id));
    newpage->Init(new_page_id, INVALID_PAGE_ID, log_manager_, txn);
    newpage->InsertTuple(row, schema_, txn, lock_manager_, log_manager_);
    buffer_pool_manager_->UnpinPage(newpage->GetTablePageId(), true);
    return true;
  }
  //There is at least one page in the page chain table
  ASSERT(this->GetFirstPageId() != INVALID_PAGE_ID, "Fetch Error in the InsertTuple");
  TablePage *page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(this->GetFirstPageId()));
  while(!page->InsertTuple(row, schema_,txn, lock_manager_, log_manager_)){
    //If the page is not enough for the table insert, then move to another page
    //It the next page is not exist, need to create a new page, maintain the double linked list
     if(page->GetNextPageId() == INVALID_PAGE_ID){
       page_id_t new_page_id;
       TablePage *newpage = reinterpret_cast<TablePage *>(buffer_pool_manager_->NewPage(new_page_id));
       newpage->Init(new_page_id, page->GetTablePageId(), log_manager_, txn);
       page->SetNextPageId(new_page_id);
     }
    buffer_pool_manager_->UnpinPage(page->GetTablePageId(), true);
    page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(page->GetNextPageId()));
  }
  buffer_pool_manager_->UnpinPage(page->GetTablePageId(), true);
  return true;
}


bool TableHeap::MarkDelete(const RowId &rid, Transaction *txn) {
  // Find the page which contains the tuple.
  auto page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(rid.GetPageId()));
  // If the page could not be found, then abort the transaction.
  if (page == nullptr) {
    return false;
  }
  // Otherwise, mark the tuple as deleted.
  page->WLatch();
  page->MarkDelete(rid, txn, lock_manager_, log_manager_);
  page->WUnlatch();
  buffer_pool_manager_->UnpinPage(page->GetTablePageId(), true);
  return true;
}

/**
 * TODO: Student Implement
 */
int TableHeap::UpdateTuple(Row &row, const RowId &rid, Transaction *txn) {
  //1. Find the page which contains the tuple
  ASSERT(rid.GetPageId() != INVALID_PAGE_ID, "The page id is invalid in UpdateTuple");
  TablePage * page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(rid.GetPageId()));
  if(page == nullptr){
//    LOG(ERROR) << "The page could not be found in the UpdateTable" << std::endl;
    return false;
  }
  Row *old_row = new Row(rid);
  page->GetTuple(old_row, schema_, txn, lock_manager_);

  std::string log;
  //2. Update the tuple from the page, if there is not enough space, allocate a new page
  row.SetRowId(rid);
  if(page->UpdateTuple(row, old_row, schema_, txn, lock_manager_, log_manager_, log) == false)
  {
    buffer_pool_manager_->UnpinPage(page->GetPageId(), true);
    if(log == "Not enough space to update, need to delete and insert for update."){
//      page->MarkDelete(rid, txn, lock_manager_, log_manager_);
//      page->ApplyDelete(rid, txn, log_manager_);
//      if (page->InsertTuple(row, schema_, txn, lock_manager_, log_manager_) == false){
//        ASSERT(false, "InsertTuple failed in UpdateTuple");
//      }
//      else{
//        buffer_pool_manager_->UnpinPage(page->GetTablePageId(), true);
//        return true;
//      }
      if(page->GetNextPageId() != INVALID_PAGE_ID)
        page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(page->GetNextPageId()));
      else{
        page_id_t new_page_id;
        TablePage *newpage = reinterpret_cast<TablePage *>(buffer_pool_manager_->NewPage(new_page_id));
        newpage->Init(new_page_id, page->GetTablePageId(), log_manager_, txn);
        newpage->InsertTuple(row, schema_, txn, lock_manager_, log_manager_);
        page->SetNextPageId(new_page_id);
        page->ApplyDelete(rid, txn, log_manager_);
        buffer_pool_manager_->UnpinPage(page->GetTablePageId(), true);
        buffer_pool_manager_->UnpinPage(new_page_id, true);
        return 2;
      }
     }
    else{
      buffer_pool_manager_->UnpinPage(page->GetPageId(), true);
      return false;
    }
  }
//  std::string log;
//  if (page->UpdateTuple(row, old_row, schema_, txn, lock_manager_, log_manager_, log) == false) {
//    buffer_pool_manager_->UnpinPage(page->GetTablePageId(), true);
//    delete old_row;
//    return false;
//  }
  buffer_pool_manager_->UnpinPage(page->GetPageId(), true);
  delete old_row;
  return true;
}

/**
 * TODO: Student Implement
 */
void TableHeap::ApplyDelete(const RowId &rid, Transaction *txn) {
  // Step1: Find the page which contains the tuple.
  auto page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(rid.GetPageId()));
  ASSERT(page != nullptr, "The page could not be found in the ApplyDelete");
  // Step2: Delete the tuple from the page.
  page->ApplyDelete(rid, txn, log_manager_);
  buffer_pool_manager_->UnpinPage(page->GetTablePageId(), true);
}

void TableHeap::RollbackDelete(const RowId &rid, Transaction *txn) {
  // Find the page which contains the tuple.
  auto page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(rid.GetPageId()));
  assert(page != nullptr);
  // Rollback to delete.
  page->WLatch();
  page->RollbackDelete(rid, txn, log_manager_);
  page->WUnlatch();
  buffer_pool_manager_->UnpinPage(page->GetTablePageId(), true);
}

/**
 * TODO: Student Implement
 */
bool TableHeap::GetTuple(Row *row, Transaction *txn) {
  ASSERT(row->GetRowId().GetPageId() != INVALID_PAGE_ID, "The page id is invalid in GetTuple");
  TablePage *page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(row->GetRowId().GetPageId()));
  if(page == nullptr){
    LOG(ERROR) << "The page could not be found in the GetTuple" << std::endl;
    return false;
  }
  page->GetTuple(row, schema_, txn, lock_manager_);
  return true;
}

void TableHeap::DeleteTable(page_id_t page_id) {
  if (page_id != INVALID_PAGE_ID) {
    auto temp_table_page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(page_id));  // 删除table_heap
    if (temp_table_page->GetNextPageId() != INVALID_PAGE_ID)
      DeleteTable(temp_table_page->GetNextPageId());
    buffer_pool_manager_->UnpinPage(page_id, false);
    buffer_pool_manager_->DeletePage(page_id);
  } else {
    DeleteTable(first_page_id_);
  }
}

/**
 * TODO: Student Implement
 *
 */
TableIterator TableHeap::Begin(Transaction *txn) {
  page_id_t first_page_id = this->GetFirstPageId();
  TablePage *first_page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(first_page_id));
  RowId *row_id = new RowId();
  //No data in the table_heap
  if(first_page->GetFirstTupleRid(row_id) == false)
    return TableIterator(this, nullptr, nullptr);
  Row *first_row = new Row(*row_id);
  first_page->GetTuple(first_row, schema_, txn, lock_manager_);
  return TableIterator(this, first_row, txn);
}

/**
 * TODO: Student Implement
 */
TableIterator TableHeap::End() {
//  page_id_t page_id = this->GetFirstPageId();
//  if(page_id == INVALID_PAGE_ID){
//    return TableIterator(this, nullptr, nullptr);
//  }
//  TablePage *page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(page_id));
//  while(page->GetNextPageId() != INVALID_PAGE_ID)
//  {
//    page_id = page->GetNextPageId();
//    page = reinterpret_cast<TablePage *>(buffer_pool_manager_->FetchPage(page_id));
//  }
//  RowId *row_id, *next_row_id;
//  page->GetFirstTupleRid(row_id);
//  while(page->GetNextTupleRid(*row_id, next_row_id)){
//    row_id = next_row_id;
//  }
//  return TableIterator(this, row_id);
    return TableIterator(this, nullptr, nullptr);
}
