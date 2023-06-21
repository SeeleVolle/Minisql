//
// Created by njz on 2023/1/17.
//
#include "executor/executors/seq_scan_executor.h"
#include <stack>

/**
* TODO: Student Implement
*/
SeqScanExecutor::SeqScanExecutor(ExecuteContext *exec_ctx, const SeqScanPlanNode *plan)
    : AbstractExecutor(exec_ctx),
      plan_(plan){}

void SeqScanExecutor::Init() {
  this->index_ = 0;
  std::string table_name = plan_->GetTableName();
  CatalogManager * catalog_mgr = GetExecutorContext()->GetCatalog();
  TableInfo * table_info_ = nullptr;
  catalog_mgr->GetTable(table_name, table_info_);
  if(table_info_ == nullptr)
    return;
  std::vector<RowId> rids;
//  std::stack<AbstractExpressionRef> predicate;
//  predicate.push(plan_->GetPredicate());
//  while(!predicate.empty()){
//    AbstractExpressionRef e = predicate.top();
//    predicate.pop();
//    //Output the type of the e
//    for(int i = 0; i < e->GetChildren().size(); i++)
//      predicate.push(e->GetChildAt(i));
//  }
  //Wait for implementation
  if(plan_->GetPredicate().get() == nullptr){
    for(TableIterator iter = table_info_->GetTableHeap()->Begin(nullptr); iter != table_info_->GetTableHeap()->End(); iter++)
    {
      Row row = *iter;
      Row index_row;
      row.GetKeyFromRow(table_info_->GetSchema(), plan_->OutputSchema(), index_row);
      index_row.SetRowId(row.GetRowId());
      result_set_.push_back(index_row);
    }
  }

  //traverse the condition to judge whether the row is satisfied
  else{
    AbstractExpressionRef old_e;
    old_e = plan_->GetPredicate();
    if(old_e->GetType() == ExpressionType::ComparisonExpression){
      for(TableIterator iter = table_info_->GetTableHeap()->Begin(nullptr); iter != table_info_->GetTableHeap()->End(); iter++){
        Row row = *iter;
        Row index_row;
        if(old_e->Evaluate(&row).CompareEquals(Field(kTypeInt, 1)) == true){
          row.GetKeyFromRow(table_info_->GetSchema(), plan_->OutputSchema(), index_row);
          index_row.SetRowId(row.GetRowId());
          result_set_.push_back(index_row);
        }
      }
    }
    else{
        if(old_e->GetType() == ExpressionType::LogicExpression)
        {
          for(TableIterator iter = table_info_->GetTableHeap()->Begin(nullptr); iter != table_info_->GetTableHeap()->End(); iter++){
            Row row = *iter;
            Row index_row;
            if(old_e->Evaluate(&row).CompareEquals(Field(kTypeInt, 1)) == true){
              row.GetKeyFromRow(table_info_->GetSchema(), plan_->OutputSchema(), index_row);
              index_row.SetRowId(row.GetRowId());
              result_set_.push_back(index_row);
            }
          }
        }
      }
    }
}
bool SeqScanExecutor::Next(Row *row, RowId *rid) {
  if(index_ >= result_set_.size()){
    return false;
  }
  *row = result_set_[index_];
  *rid = result_set_[index_].GetRowId();
  index_++;
  return true;
}
