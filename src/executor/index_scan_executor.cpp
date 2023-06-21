#include "executor/executors/index_scan_executor.h"
#include "algorithm"
#include "stack"
#include "planner/expressions/constant_value_expression.h"

/**
* TODO: Student Implement
*/
IndexScanExecutor::IndexScanExecutor(ExecuteContext *exec_ctx, const IndexScanPlanNode *plan)
    : AbstractExecutor(exec_ctx), plan_(plan) {}

bool Mycompare(const RowId a, const RowId b){
  return a == b;
}

void IndexScanExecutor::Init() {
  this->index_ = 0;
  std::string table_name = plan_->GetTableName();
  TableInfo * tableinfo_ = nullptr;

  GetExecutorContext()->GetCatalog()->GetTable(table_name, tableinfo_);
  if(tableinfo_ == nullptr)
    return;
  std::vector<RowId> old_rid, new_rid, result_rid, temp_rid;
  std::unordered_map<uint32_t, Field> column_fields;
  std::unordered_map<uint32_t, std::string> operators;
  //Scan all the predicates
  std::stack<AbstractExpressionRef> plan_stack;
  plan_stack.push(plan_->GetPredicate());
  while(!plan_stack.empty())
  {
    AbstractExpressionRef f = plan_stack.top();
    plan_stack.pop();
    if(f->GetType() == ExpressionType::ComparisonExpression)
    {
      //The last comparison
      if(plan_stack.empty())
      {
        std::shared_ptr<ColumnValueExpression> column_ = std::dynamic_pointer_cast<ColumnValueExpression>(f->GetChildAt(0));
        std::shared_ptr<ConstantValueExpression> constant_ = std::dynamic_pointer_cast<ConstantValueExpression>(f->GetChildAt(1));
        column_fields.emplace(std::pair<uint32_t, Field>(column_->GetColIdx(), constant_->val_));
        operators.emplace(std::pair<uint32_t, std::string>(column_->GetColIdx() ,std::dynamic_pointer_cast<ComparisonExpression>(f)->GetComparisonType()));
      }
      else{
        std::shared_ptr<ConstantValueExpression> constant_ = std::dynamic_pointer_cast<ConstantValueExpression>(plan_stack.top());
        plan_stack.pop();
        std::shared_ptr<ColumnValueExpression> column_ = std::dynamic_pointer_cast<ColumnValueExpression>(f->GetChildAt(1));
        column_fields.emplace(std::pair<uint32_t, Field>(column_->GetColIdx(), constant_->val_));
        operators.emplace(std::pair<uint32_t, std::string>(column_->GetColIdx() ,std::dynamic_pointer_cast<ComparisonExpression>(f)->GetComparisonType()));
      }
    }
    else
    {

    }
    for(auto child : f->GetChildren())
      plan_stack.push(child);
  }

  for(auto indexinfo_ : plan_->indexes_)
  {
    std::vector<Field> fields;
    std::vector<Column *> index_columns = indexinfo_->GetIndexKeySchema()->GetColumns();
    ASSERT(index_columns.size() == 1, "IndexScanExecutor only support single column index");
    for(auto column : index_columns)
      if (column_fields.find(column->GetTableInd()) != column_fields.end())
        fields.push_back(column_fields.at(column->GetTableInd()));
    if(fields.empty())
      continue;
    BPlusTreeIndex * b_index_ = reinterpret_cast<BPlusTreeIndex *>(indexinfo_->GetIndex());
    Row *index_row = new Row(fields);
    b_index_->ScanKey(*index_row, new_rid, nullptr, operators.at(index_columns[0]->GetTableInd()));
//    else if(operators.at(column->GetTableInd() == "<>"))
//    {
//      b_index_->ScanKey(*index_row, temp_rid, nullptr);
//      for(IndexIterator iter = b_index_->GetBeginIterator(); iter != b_index_->End(); iter++)
//      {
//        if(std::find(temp_rid.begin(), temp_rid.end(), (*iter).second) == temp_rid.end())
//          new_rid.emplace((*iter).second);
//      }
//    }
//    else if (operators.at(column->GetTableInd() == "<"))
//    {
//      b_index_->ScanKey(*index_row, temp_rid, nullptr);
//      for(IndexIterator iter = b_index_->GetBeginIterator(); iter != b_index_->End(); iter++)
//      {
//        if(b_index_->(*iter).first)
//          new_rid.emplace((*iter).second);
//      }
//    }
//    else if (operators.at(column->GetTableInd() == "<="))
//    {
//      b_index_->ScanKey(*index_row, temp_rid, nullptr);
//      for(IndexIterator iter = b_index_->GetBeginIterator(); iter != b_index_->End(); iter++)
//      {
//        if()
//          new_rid.emplace((*iter).second);
//      }
//    }
//    else if (operators.at(column->GetTableInd() == ">"))
//    {
//      b_index_->ScanKey(*index_row, temp_rid, nullptr);
//      for(IndexIterator iter = b_index_->GetBeginIterator(); iter != b_index_->End(); iter++)
//      {
//        if()
//          new_rid.emplace((*iter).second);
//      }
//    }
//    else if (operators.at(column->GetTableInd() == ">="))
//    {
//      b_index_->ScanKey(*index_row, temp_rid, nullptr);
//      for(IndexIterator iter = b_index_->GetBeginIterator(); iter != b_index_->End(); iter++)
//      {
//        if()
//          new_rid.emplace((*iter).second);
//      }
//    }
    if(old_rid.empty() == true)
    {
      old_rid = new_rid;
      result_rid = new_rid;
    }
    else
      set_intersection(old_rid.begin(), old_rid.end(), new_rid.begin(), new_rid.end(), result_rid.begin(), Mycompare);
    old_rid = result_rid;
    new_rid.clear();
    result_rid.clear();
    temp_rid.clear();
  }
  if(plan_->need_filter_ == false){
    for(auto rid : old_rid){
      Row *row = new Row(rid);
      tableinfo_->GetTableHeap()->GetTuple(row, nullptr);
      Row index_row;
      row->GetKeyFromRow(tableinfo_->GetSchema(), plan_->OutputSchema(), index_row);
      index_row.SetRowId(row->GetRowId());
      result_set.push_back(index_row);
    }
  }
  else{
    for(auto rid : old_rid){
      Row *row = new Row(rid);
      tableinfo_->GetTableHeap()->GetTuple(row, nullptr);
      if(plan_->GetPredicate()->Evaluate(row).CompareEquals(Field(kTypeInt, 1)) == true){
        Row index_row;
        row->GetKeyFromRow(tableinfo_->GetSchema(), plan_->OutputSchema(), index_row);
        index_row.SetRowId(row->GetRowId());
        result_set.push_back(index_row);
      }
    }
  }
}

bool IndexScanExecutor::Next(Row *row, RowId *rid) {
  if(index_ >= result_set.size()){
    return false;
  }
  *row = result_set[index_];
  *rid = result_set[index_].GetRowId();
  index_++;
  return true;
}
