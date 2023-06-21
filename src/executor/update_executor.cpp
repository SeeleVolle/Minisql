//
// Created by njz on 2023/1/30.
//

#include "executor/executors/update_executor.h"

UpdateExecutor::UpdateExecutor(ExecuteContext *exec_ctx, const UpdatePlanNode *plan,
                               std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

/**
* TODO: Student Implement
*/
void UpdateExecutor::Init() {
  cnt_ = 0;
  std::string table_name = plan_->GetTableName();
  child_executor_->Init();
  CatalogManager *catalog_mgr = GetExecutorContext()->GetCatalog();
  TableInfo *table_info = nullptr;
  catalog_mgr->GetTable(table_name, table_info);
  std::vector<IndexInfo *> indexes_;
  catalog_mgr->GetTableIndexes(table_name, indexes_);
  Row *row = new Row(), *old_row = new Row();
  RowId *rid = new RowId();
  while(child_executor_->Next(row, rid)){
    int insert_flag = true;
    //Get the old_row from the table_heap
    old_row->SetRowId(*rid);
    table_info->GetTableHeap()->GetTuple(old_row, nullptr);
    Row full_row(GenerateUpdatedTuple(*old_row));
    //Check the value whether is unique
    for(auto indexinfo_ : indexes_){
      std::vector<Column *> columns = indexinfo_->GetIndexKeySchema()->GetColumns();
      ASSERT(columns.size() == 1, "InsertExecutor only support single column index");
      if(columns[0]->IsUnique() == false)
        continue;
      BPlusTreeIndex * b_index_ = reinterpret_cast<BPlusTreeIndex *>(indexinfo_->GetIndex());
      std::vector<RowId> results_;
      std::vector<Field> Fields;
      Fields.push_back(*(full_row.GetField(columns[0]->GetTableInd())));
      if(full_row.GetField(columns[0]->GetTableInd())->CompareEquals(*old_row->GetField(columns[0]->GetTableInd())) == true)
        continue;
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
      table_info->GetTableHeap()->UpdateTuple(full_row, *rid, nullptr);
      for(auto index_info_ : indexes_){
        std::vector<Column *> columns = index_info_->GetIndexKeySchema()->GetColumns();
        ASSERT(columns.size() == 1, "InsertExecutor only support single column index");
        std::vector<Field> Fields;
        Fields.push_back(*(old_row->GetField(columns[0]->GetTableInd())));
        Row index_old_row(Fields);
        index_old_row.SetRowId(*rid);
        index_info_->GetIndex()->RemoveEntry(index_old_row, *rid, nullptr);

        std::vector<Field> Fields2;
        Fields2.push_back(*(full_row.GetField(columns[0]->GetTableInd())));
        Row index_new_row(Fields2);
        index_new_row.SetRowId(full_row.GetRowId());
        index_info_->GetIndex()->InsertEntry(index_new_row, full_row.GetRowId(), nullptr);
      }
      cnt_++;
    }
  }
}

bool UpdateExecutor::Next([[maybe_unused]] Row *row, RowId *rid) {
  if(cnt_ == 0){
    return false;
  }
  cnt_--;
  row = new Row();
  rid = new RowId();
  return true;
}

Row UpdateExecutor::GenerateUpdatedTuple(const Row &src_row) {
  Row full_row;
  full_row.GetFields().resize(src_row.GetFieldCount());
  for(int i = 0; i < src_row.GetFieldCount(); i++)
    plan_->update_attrs_.find(i) == plan_->update_attrs_.end() ? full_row.GetFields()[i] = new Field(*src_row.GetField(i)) : full_row.GetFields()[i] = new Field(plan_->update_attrs_.at(i)->Evaluate(&full_row));
  return full_row;
}