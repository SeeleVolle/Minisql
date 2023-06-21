//
// Created by njz on 2023/1/27.
//

#include "executor/executors/insert_executor.h"

InsertExecutor::InsertExecutor(ExecuteContext *exec_ctx, const InsertPlanNode *plan,
                               std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void InsertExecutor::Init() {
  cnt_ = 0;
  //table information
  std::string table_name_ = plan_->GetTableName();
  CatalogManager *catalog_mgr = GetExecutorContext()->GetCatalog();
  TableInfo *table_info_ = nullptr;
  catalog_mgr->GetTable(table_name_, table_info_);
  TableHeap * table_heap = table_info_->GetTableHeap();
  //Index information
  std::vector<IndexInfo *>indexes_;
  catalog_mgr->GetTableIndexes(table_name_, indexes_);
  //Get the rows we need to insert
  Row * row = new Row();
  RowId * rid = new RowId();
  child_executor_->Init();
  while(child_executor_->Next(row, rid) == true){
    int insert_flag = true;
    //Check the value whether is unique
    for(auto indexinfo_ : indexes_){
      std::vector<Column *> columns = indexinfo_->GetIndexKeySchema()->GetColumns();
      ASSERT(columns.size() == 1, "InsertExecutor only support single column index");
      if(columns[0]->IsUnique() == false)
        continue;
      BPlusTreeIndex * b_index_ = reinterpret_cast<BPlusTreeIndex *>(indexinfo_->GetIndex());
      //It means that the index without any data, we should insert the data directly
      if(b_index_->IsEmpty() == true)
        continue;
      std::vector<RowId> results_;
      std::vector<Field> Fields;
      Fields.push_back(*(row->GetField(columns[0]->GetTableInd())));
      Row index_row(Fields);
      index_row.SetRowId(*rid);
      b_index_->ScanKey(index_row, results_, nullptr);
      if(results_.empty() == false)
      {
        cout<< "Duplicate Entry "<<"for key "<<columns[0]->GetName()<< endl;
        insert_flag = false;
        break;
      }
    }
    if(insert_flag == false)
      continue;
    else{
      //Update the table_heap
      table_heap->InsertTuple(*row, nullptr);
      *rid = row->GetRowId();
      ASSERT(rid->Get() != INVALID_ROWID.Get(), "InsertExecutor: Invalid rowid");
      //Update the index
      for(auto indexinfo_ : indexes_){
        std::vector<Column*> columns = indexinfo_->GetIndexKeySchema()->GetColumns();
        ASSERT(columns.size() == 1, "InsertExecutor only support single column index");
        BPlusTreeIndex * b_index_ = reinterpret_cast<BPlusTreeIndex *>(indexinfo_->GetIndex());
        std::vector<Field> Fields;
        Fields.push_back(*(row->GetField(columns[0]->GetTableInd())));
        Row index_row(Fields);
        index_row.SetRowId(*rid);
        b_index_->InsertEntry(index_row, *rid, nullptr);
      }
      cnt_++;
    }
  }
}

bool InsertExecutor::Next([[maybe_unused]] Row *row, RowId *rid) {
  if (cnt_ == 0) {
    return false;
  }
  cnt_--;
//  row = new Row();
//  rid = new RowId();
  return true;
}