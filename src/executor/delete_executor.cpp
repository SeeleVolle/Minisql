//
// Created by njz on 2023/1/29.
//

#include "executor/executors/delete_executor.h"

/**
* TODO: Student Implement
*/

DeleteExecutor::DeleteExecutor(ExecuteContext *exec_ctx, const DeletePlanNode *plan,
                               std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void DeleteExecutor::Init() {


  cnt_ = 0;
  child_executor_->Init();
  std::string table_name_ = plan_->GetTableName();
  CatalogManager * catalog_mgr = GetExecutorContext()->GetCatalog();
  TableInfo * table_info = nullptr;
  catalog_mgr->GetTable(table_name_, table_info);
  std::vector<IndexInfo *>indexes_;
  catalog_mgr->GetTableIndexes(table_name_, indexes_);
  Row * row = new Row();
  RowId * rid = new RowId();
  while(child_executor_->Next(row, rid)){
    table_info->GetTableHeap()->ApplyDelete(*rid, nullptr);
    for(auto indexinfo_ : indexes_){
      std::vector<Column *> columns = indexinfo_->GetIndexKeySchema()->GetColumns();
      ASSERT(columns.size() == 1, "InsertExecutor only support single column index");
      BPlusTreeIndex * b_index_ = reinterpret_cast<BPlusTreeIndex *>(indexinfo_->GetIndex());
      std::vector<Field> Fields;
      Fields.push_back(*(row->GetField(columns[0]->GetTableInd())));
      Row index_row(Fields);
      index_row.SetRowId(*rid);
      b_index_->RemoveEntry(index_row, *rid, nullptr);
    }
    cnt_++;
  }
}

bool DeleteExecutor::Next([[maybe_unused]] Row *row, RowId *rid) {
  if(cnt_ == 0){
    return false;
  }
  cnt_--;
  row = new Row();
  rid = new RowId();
  return true;
}