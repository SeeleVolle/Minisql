#include "executor/execute_engine.h"

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

#include <chrono>

#include "parser/syntax_tree_printer.h"
#include "common/result_writer.h"
#include "executor/executors/delete_executor.h"
#include "executor/executors/index_scan_executor.h"
#include "executor/executors/insert_executor.h"
#include "executor/executors/seq_scan_executor.h"
#include "executor/executors/update_executor.h"
#include "executor/executors/values_executor.h"
#include "glog/logging.h"
#include "planner/planner.h"
#include "utils/utils.h"
extern "C" {
int yyparse(void);
#include "parser/minisql_lex.h"
#include "parser/parser.h"
}

ExecuteEngine::ExecuteEngine() {
  char path[] = "./databases";
  DIR *dir;
  if((dir = opendir(path)) == nullptr) {
    mkdir("./databases", 0777);
    dir = opendir(path);
  }
  /** After you finish the code for the CatalogManager section,
   *  you can uncomment the commented code.   **/
//  struct dirent *stdir;
//  while((stdir = readdir(dir)) != nullptr) {
//    if( strcmp( stdir->d_name , "." ) == 0 ||
//        strcmp( stdir->d_name , "..") == 0 ||
//        stdir->d_name[0] == '.')
//      continue;
//    dbs_[stdir->d_name] = new DBStorageEngine(stdir->d_name, false);
//  }
  closedir(dir);
}

std::unique_ptr<AbstractExecutor> ExecuteEngine::CreateExecutor(ExecuteContext *exec_ctx,
                                                                const AbstractPlanNodeRef &plan) {
  switch (plan->GetType()) {
    // Create a new sequential scan executor
    case PlanType::SeqScan: {
      return std::make_unique<SeqScanExecutor>(exec_ctx, dynamic_cast<const SeqScanPlanNode *>(plan.get()));
    }
    // Create a new index scan executor
    case PlanType::IndexScan: {
      return std::make_unique<IndexScanExecutor>(exec_ctx, dynamic_cast<const IndexScanPlanNode *>(plan.get()));
    }
    // Create a new update executor
    case PlanType::Update: {
      auto update_plan = dynamic_cast<const UpdatePlanNode *>(plan.get());
      auto child_executor = CreateExecutor(exec_ctx, update_plan->GetChildPlan());
      return std::make_unique<UpdateExecutor>(exec_ctx, update_plan, std::move(child_executor));
    }
      // Create a new delete executor
    case PlanType::Delete: {
      auto delete_plan = dynamic_cast<const DeletePlanNode *>(plan.get());
      auto child_executor = CreateExecutor(exec_ctx, delete_plan->GetChildPlan());
      return std::make_unique<DeleteExecutor>(exec_ctx, delete_plan, std::move(child_executor));
    }
    case PlanType::Insert: {
      auto insert_plan = dynamic_cast<const InsertPlanNode *>(plan.get());
      auto child_executor = CreateExecutor(exec_ctx, insert_plan->GetChildPlan());
      return std::make_unique<InsertExecutor>(exec_ctx, insert_plan, std::move(child_executor));
    }
    case PlanType::Values: {
      return std::make_unique<ValuesExecutor>(exec_ctx, dynamic_cast<const ValuesPlanNode *>(plan.get()));
    }
    default:
      throw std::logic_error("Unsupported plan type.");
  }
}

dberr_t ExecuteEngine::ExecutePlan(const AbstractPlanNodeRef &plan, std::vector<Row> *result_set, Transaction *txn,
                                   ExecuteContext *exec_ctx) {
  // Construct the executor for the abstract plan node
  auto executor = CreateExecutor(exec_ctx, plan);

  try {
    executor->Init();
    RowId rid{};
    Row row{};
    while (executor->Next(&row, &rid)) {
      if (result_set != nullptr) {
        result_set->push_back(row);
      }
    }
  } catch (const exception &ex) {
    std::cout << "Error Encountered in Executor Execution: " << ex.what() << std::endl;
    if (result_set != nullptr) {
      result_set->clear();
    }
    return DB_FAILED;
  }
  return DB_SUCCESS;
}

dberr_t ExecuteEngine::Execute(pSyntaxNode ast) {
  if (ast == nullptr) {
    return DB_FAILED;
  }
  auto start_time = std::chrono::steady_clock::now();
  unique_ptr<ExecuteContext> context(nullptr);
  if(!current_db_.empty())
    context = dbs_[current_db_]->MakeExecuteContext(nullptr);
  switch (ast->type_) {
    case kNodeCreateDB:
      return ExecuteCreateDatabase(ast, context.get());
    case kNodeDropDB:
      return ExecuteDropDatabase(ast, context.get());
    case kNodeShowDB:
      return ExecuteShowDatabases(ast, context.get());
    case kNodeUseDB:
      return ExecuteUseDatabase(ast, context.get());
    case kNodeShowTables:
      return ExecuteShowTables(ast, context.get());
    case kNodeCreateTable:
      return ExecuteCreateTable(ast, context.get());
    case kNodeDropTable:
      return ExecuteDropTable(ast, context.get());
    case kNodeShowIndexes:
      return ExecuteShowIndexes(ast, context.get());
    case kNodeCreateIndex:
      return ExecuteCreateIndex(ast, context.get());
    case kNodeDropIndex:
      return ExecuteDropIndex(ast, context.get());
    case kNodeTrxBegin:
      return ExecuteTrxBegin(ast, context.get());
    case kNodeTrxCommit:
      return ExecuteTrxCommit(ast, context.get());
    case kNodeTrxRollback:
      return ExecuteTrxRollback(ast, context.get());
    case kNodeExecFile:
      return ExecuteExecfile(ast, context.get());
    case kNodeQuit:
      return ExecuteQuit(ast, context.get());
    default:
      break;
  }
  if(current_db_ == ""){
    cout<<"No database selected. Please select it first."<<endl;
    return DB_FAILED;
  }
  // Plan the query.
  Planner planner(context.get());
  std::vector<Row> result_set{};
  try {
    planner.PlanQuery(ast);
    // Execute the query.
    ExecutePlan(planner.plan_, &result_set, nullptr, context.get());
  } catch (const exception &ex) {
    std::cout << "Error Encountered in Planner: " << ex.what() << std::endl;
    return DB_FAILED;
  }
  auto stop_time = std::chrono::steady_clock::now();
  double duration_time =
      double((std::chrono::duration_cast<std::chrono::milliseconds>(stop_time - start_time)).count());

//  std::time_t starttime = chrono::system_clock::to_time_t(start_time);
//  std::time_t stoptime = chrono::system_clock::to_time_t(stop_time);
//  cout<<"stop time"<<starttime <<endl<<"start time"<<stoptime<<endl;
//  cout<<"duration time: "<<duration_time<<endl;
  // Return the result set as string.
  std::stringstream ss;
  ResultWriter writer(ss);

  if (planner.plan_->GetType() == PlanType::SeqScan || planner.plan_->GetType() == PlanType::IndexScan) {
    auto schema = planner.plan_->OutputSchema();
    auto num_of_columns = schema->GetColumnCount();
    if (!result_set.empty()) {
      // find the max width for each column
      vector<int> data_width(num_of_columns, 0);
      for (const auto &row : result_set) {
        for (uint32_t i = 0; i < num_of_columns; i++) {
          data_width[i] = max(data_width[i], int(row.GetField(i)->toString().size()));
        }
      }
      int k = 0;
      for (const auto &column : schema->GetColumns()) {
        data_width[k] = max(data_width[k], int(column->GetName().length()));
        k++;
      }
      // Generate header for the result set.
      writer.Divider(data_width);
      k = 0;
      writer.BeginRow();
      for (const auto &column : schema->GetColumns()) {
        writer.WriteHeaderCell(column->GetName(), data_width[k++]);
      }
      writer.EndRow();
      writer.Divider(data_width);

      // Transforming result set into strings.
      for (const auto &row : result_set) {
        writer.BeginRow();
        for (uint32_t i = 0; i < schema->GetColumnCount(); i++) {
          writer.WriteCell(row.GetField(i)->toString(), data_width[i]);
        }
        writer.EndRow();
      }
      writer.Divider(data_width);
    }
    writer.EndInformation(result_set.size(), duration_time, true);
  } else {
    writer.EndInformation(result_set.size(), duration_time, false);
  }
  std::cout << writer.stream_.rdbuf();
  return DB_SUCCESS;
}

void ExecuteEngine::ExecuteInformation(dberr_t result) {
  switch (result) {
    case DB_ALREADY_EXIST:
      cout << "Database already exists." << endl;
      break;
    case DB_NOT_EXIST:
      cout << "Database not exists." << endl;
      break;
    case DB_TABLE_ALREADY_EXIST:
      cout << "Table already exists." << endl;
      break;
    case DB_TABLE_NOT_EXIST:
      cout << "Table not exists." << endl;
      break;
    case DB_INDEX_ALREADY_EXIST:
      cout << "Index already exists." << endl;
      break;
    case DB_INDEX_NOT_FOUND:
      cout << "Index not exists." << endl;
      break;
    case DB_COLUMN_NAME_NOT_EXIST:
      cout << "Column not exists." << endl;
      break;
    case DB_KEY_NOT_FOUND:
      cout << "Key not exists." << endl;
      break;
    case DB_QUIT:
      cout << "Bye." << endl;
      break;
    default:
      break;
  }
}
/**
 * TODO: Student Implement
 */
dberr_t ExecuteEngine::ExecuteCreateDatabase(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteCreateDatabase" << std::endl;
#endif
  clock_t start, end;
  start = clock();
  std::string db_name = ast->child_->val_;
  std::string file = "DataBaseStorage.txt";

  ifstream in = ifstream(file, ios::in);
  std::vector<std::string> db_names;
  std::string line;
  while(getline(in, line))
    db_names.push_back(line);
  in.close();

  if(std::find(db_names.begin(), db_names.end(), db_name) != db_names.end()){
    LOG(ERROR)<< "Database already exists." << std::endl;
    return DB_ALREADY_EXIST;
  }
  DBStorageEngine * new_db = new DBStorageEngine(db_name, true);
  dbs_.insert(std::pair<std::string, DBStorageEngine *>(db_name, new_db));
  //Write the database information into the disk
  ofstream out = ofstream(file, ios::app);
  out << db_name << endl;
  out.close();
  end = clock();
  cout << "query OK("<<setprecision(3)<<double(end-start)/CLOCKS_PER_SEC << "sec)"<< endl;
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t ExecuteEngine::ExecuteDropDatabase(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteDropDatabase" << std::endl;
#endif
  clock_t start, end;
  start = clock();

  std::string file = "DataBaseStorage.txt";
  std::string db_name = ast->child_->val_;
  ifstream in = ifstream(file, ios::in);
  std::vector<std::string> db_names;
  std::string line;
  while(getline(in, line))
      db_names.push_back(line);
  in.close();
  //Flush the information about the database
  if(std::find(db_names.begin(), db_names.end(),db_name) == db_names.end()){
    LOG(ERROR)<< "Database not exists." << std::endl;
    return DB_NOT_EXIST;
  }
  if(dbs_.find(db_name) != dbs_.end()){
    delete dbs_[db_name];
    dbs_.erase(db_name);
  }
  if(current_db_ == db_name)
    current_db_ = "";

  ofstream out = ofstream(file, ios::out);
  for(auto name : db_names)
    if(name != db_name)
      out << name << endl;
  end = clock();
  cout<<"query OK("<<setprecision(3)<<double(end-start)/CLOCKS_PER_SEC <<"sec)"<< endl;
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t ExecuteEngine::ExecuteShowDatabases(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteShowDatabases" << std::endl;
#endif
  clock_t start, end;
  start = clock();
  ResultWriter *writer = new ResultWriter(std::cout, false);
  std::string file = "DataBaseStorage.txt";
  ifstream in = ifstream(file, ios::in);
  std::vector<std::string> db_names;
  std::string line;
  while(getline(in, line))
    db_names.push_back(line);

  //synatx print
  cout<<endl;
  cout<<"+--------------------+"<<endl;
  cout<<"| Database           |"<<endl;
  cout<<"+--------------------+"<<endl;
  if(db_names.empty() == true){
    cout<<"|                    |"<<endl;
    cout<<"+--------------------+"<<endl;
    end = clock();
    cout<<"0 rows in set ("<<setprecision(3) <<double(end-start)/CLOCKS_PER_SEC <<"sec)"<< endl;
    return DB_SUCCESS;
  }
  else{
    size_t max_length = 18;
    for(auto db : db_names)
      if(db.length() > max_length)
        max_length = db.length()+10;
    for(auto db : db_names){
      writer->BeginRow(); writer->WriteCell(db, max_length); writer->EndRow();
    }
    cout<<"+--------------------+"<<endl;
  }
  end = clock();
  LOG(INFO)<<dbs_.size()<<" rows in set ("<<setprecision(3)<<double(end-start)/CLOCKS_PER_SEC  <<"sec)"<< endl;
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t ExecuteEngine::ExecuteUseDatabase(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteUseDatabase" << std::endl;
#endif
  clock_t start, end;
  start = clock();
  std::string db_name = ast->child_->val_;
  if(dbs_.find(db_name) != dbs_.end()){
    current_db_ = db_name;
    end = clock();
    cout<<"Database changed"<<endl;
    return DB_SUCCESS;
  }
  std::string file = "DataBaseStorage.txt";
  ifstream in = ifstream(file, ios::in);
  std::vector<std::string> db_names;
  std::string line;
  while(getline(in, line))
    db_names.push_back(line);
  if(std::find(db_names.begin(), db_names.end(), db_name) == db_names.end()){
    cout<< "Database "<<db_name <<" not exists." << std::endl;
    return DB_NOT_EXIST;
  }
  DBStorageEngine * new_db = new DBStorageEngine(db_name, false);
  dbs_.insert(std::pair<std::string, DBStorageEngine *>(db_name, new_db));
  current_db_ = db_name;
  end = clock();
  cout<<"Database changed("<<setprecision(3)<<(double)(end-start)/CLOCKS_PER_SEC<<"sec)"<<endl;
}

/**
 * TODO: Student Implement
 */
dberr_t ExecuteEngine::ExecuteShowTables(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteShowTables" << std::endl;
#endif
  clock_t start, end;
  start = clock();
  ResultWriter *writer = new ResultWriter(std::cout, false);
  if(current_db_ == ""){
    end = clock();
    cout<<"No database selected. Please select it first.("<<setprecision(3)<<(double)(end-start)/CLOCKS_PER_SEC<<"sec)"<<endl;
    return DB_FAILED;
  }
  std::vector<TableInfo *>tables;
  dbs_[current_db_]->catalog_mgr_->GetTables(tables);
  //synatx print
  std::string title = "Tables_in_"+current_db_;
  size_t max_length = title.length() + 5;
  for(auto table : tables)
    if(table->GetTableName().length() > max_length)
      max_length = table->GetTableName().length()+5;
  writer->Divider_line(max_length);
  writer->BeginRow(); writer->WriteCell(current_db_, max_length); writer->EndRow();
  writer->Divider_line(max_length);
  if(tables.empty() == true){
    writer->BeginRow(); writer->WriteCell(" ", max_length); writer->EndRow();
    writer->Divider_line(max_length);
    end = clock();
    cout<<"0 rows in set ("<<setprecision(3)<<(double)(end-start)/CLOCKS_PER_SEC<<"sec)"<<endl;
    return DB_SUCCESS;
  }
  for(auto table : tables){
    writer->BeginRow(); writer->WriteCell(table->GetTableName(), max_length); writer->EndRow();
  }
  writer->Divider_line(max_length);
  end = clock();
  writer->EndInformation(tables.size(), double(end-start)/CLOCKS_PER_SEC * 1000, true);
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t ExecuteEngine::ExecuteCreateTable(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteCreateTable" << std::endl;
#endif
  //0. Check Whether the table already exists
  clock_t start, end;
  start = clock();
  if (current_db_ == "")
  {
    end = clock();
    cout<<"No database selected. Please select it first.("<<setprecision(3)<<(double)(end-start)/CLOCKS_PER_SEC<<"sec)"<<endl;
    return DB_FAILED;
  }
  std::string table_name = ast->child_->val_;
  std::vector<TableInfo *>tables;
  dbs_[current_db_]->catalog_mgr_->GetTables(tables);
  //Check whether the table is already existing
  for(auto table : tables){
    if(table->GetTableName() == table_name){
      end = clock();
      cout<<"Table "<<table_name<<" already exists.("<<setprecision(3)<<(double)(end-start)/CLOCKS_PER_SEC<<"sec)"<<endl;
      return DB_TABLE_ALREADY_EXIST;
    }
  }
  //Get all the information before creating table
  pSyntaxNode Column_first = ast->child_->next_->child_, Column_end = Column_first;
  std::vector<std::string> primary_keys;
  //1. Get the primary key information if exists
  while(Column_end->next_ != nullptr)
    Column_end = Column_end->next_;
  if( Column_end->val_ != NULL && string(Column_end->val_) == "primary keys"){
    pSyntaxNode primary_key = Column_end->child_;
    while(primary_key != nullptr){
      primary_keys.push_back(primary_key->val_);
      primary_key = primary_key->next_;
    }
  }
  //2. Get the column information
  std::vector<Column *> columns;
  pSyntaxNode Column_attribute = Column_first->child_;
  std::string column_name, column_type;
  int column_index = 0;

  while(Column_first != nullptr){
    if(Column_first->val_ != NULL && string(Column_first->val_) == "primary keys")
      break;
    bool unique_flag = false;
    bool nullable_flag = true;
    Column_attribute = Column_first->child_;
    if(Column_first->val_ != NULL && string(Column_first->val_) == "unique")
      unique_flag = true;
    for(auto key: primary_keys)
      if(Column_attribute->val_ == key)
        unique_flag = true;
    //If the column is in the primary key, then it can't be null
    column_name = Column_attribute->val_;
    if(std::find(primary_keys.begin(), primary_keys.end(), column_name) != primary_keys.end())
      nullable_flag = false;
    //Check whether the name is repeated
    for(auto column : columns){
      if(column->GetName() == column_name){
        end = clock();
        cout<<"Column "<<column_name<<" repeated.("<<setprecision(3)<<(double)(end-start)/CLOCKS_PER_SEC<<"sec)"<<endl;
        return DB_FAILED;
      }
    }
    Column_attribute = Column_attribute->next_;
    column_type = Column_attribute->val_;
    if(column_type == "int" )
    {
      Column * new_column = new Column(column_name, kTypeInt, column_index, nullable_flag, unique_flag);
      columns.push_back(new_column);
    }
    else if(column_type == "char"){
      std::string length = Column_attribute->child_->val_;
      uint32_t length_int = stoi(length);
      if(length.find(".") != length.npos || length.find("-") != length.npos){
        end = clock();
        cout<<"Invalid length of type char.("<<setprecision(3)<<(double)(end-start)/CLOCKS_PER_SEC<<"sec)"<<endl;
        return DB_FAILED;
      }
      Column * new_column = new Column(column_name, kTypeChar, length_int, column_index, nullable_flag, unique_flag);
      columns.push_back(new_column);
    }
    else if(column_type == "float"){
      Column * new_column = new Column(column_name, kTypeFloat, column_index, nullable_flag, unique_flag);
      columns.push_back(new_column);
    }
    column_index++;
    Column_first = Column_first->next_;
  }
  //Create table
  auto schema = std::make_shared<Schema>(columns);
  Transaction txn;
  TableInfo * table_info = nullptr;
  dbs_[current_db_]->catalog_mgr_->CreateTable(table_name, schema.get(), &txn, table_info);
  //Scan for the primary_keys and unique
  for(auto column : columns){
    if(column->IsUnique()){
      IndexInfo * index_info = nullptr;
      std::vector<std::string> index_keys;
      index_keys.clear();
      index_keys.push_back(column->GetName());
      dbs_[current_db_]->catalog_mgr_->CreateIndex(table_name, "index_"+column->GetName(), index_keys, &txn, index_info, "bptree");
    }
  }
//  if(primary_keys.size() != 0)
//  {
//    std::string pri_index_name = "index_";
//    for(auto key : primary_keys)
//      pri_index_name += key;
//    IndexInfo * index_info = nullptr;
//    dbs_[current_db_]->catalog_mgr_->CreateIndex(table_name, pri_index_name, primary_keys, &txn, index_info, "bptree");
//
//  }
  end = clock();
  cout<<"Query OK, 0 rows affected ("<<setprecision(3)<<(double)(end-start)/CLOCKS_PER_SEC<<"sec)"<<endl;
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t ExecuteEngine::ExecuteDropTable(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteDropTable" << std::endl;
#endif
  clock_t start, end;
  start = clock();
  if (current_db_ == "")
  {
    end = clock();
    cout<<"No database selected. Please select it first.("<<setprecision(3)<<(double)(end-start)/CLOCKS_PER_SEC<<"sec)"<<endl;
    return DB_FAILED;
  }
  std::string table_name = ast->child_->val_;
  std::vector<TableInfo *>tables;
  dbs_[current_db_]->catalog_mgr_->GetTables(tables);
  //Check whether the table exists in current db
  bool exist_flag = false;
  for(auto table : tables){
    if(table->GetTableName() == table_name)
    {
      exist_flag = true;
      break;
    }
  }
  if(exist_flag == false){
    end = clock();
    cout<<"table "<<table_name<<"not exists("<<setprecision(3)<<(double)(end-start)/CLOCKS_PER_SEC<<"sec)"<<endl;
    return DB_FAILED;
  }
  dbs_[current_db_]->catalog_mgr_->DropTable(table_name);
  end = clock();
  cout<<"Query OK. (" <<setprecision(3)<<(double)(end-start)/CLOCKS_PER_SEC<<"sec)"<< endl;
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t ExecuteEngine::ExecuteShowIndexes(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteShowIndexes" << std::endl;
#endif
  clock_t start, end;
  start = clock();
  if (current_db_ == "")
  {
    end = clock();
    cout<<"No database selected. Please select it first.("<<setprecision(3)<<(double)(end-start)/CLOCKS_PER_SEC<<"sec)"<<endl;
    return DB_FAILED;
  }
  ResultWriter *writer = new ResultWriter(std::cout, false);
  if(current_db_ == ""){
    end = clock();
    cout<<"No database selected. Please select it first.("<<setprecision(3)<<(double)(end-start)/CLOCKS_PER_SEC<<"sec)"<<endl;
    return DB_FAILED;
  }
  std::vector<TableInfo *>tables;
  dbs_[current_db_]->catalog_mgr_->GetTables(tables);
  if(tables.empty() == true){
    end = clock();
    cout<<"No table in the "<<current_db_<<".("<<setprecision(3)<<(double)(end-start)/CLOCKS_PER_SEC<<"sec)"<< endl;
    return DB_FAILED;
  }
  std::vector<IndexInfo *>indexes;
  for(auto table : tables){
    std::string title = "Indexes_in_"+table->GetTableName();
    size_t max_length = title.length() + 5;
    std::vector<IndexInfo *>indexes;
    dbs_[current_db_]->catalog_mgr_->GetTableIndexes(table->GetTableName(), indexes);
    if(indexes.empty() == true){
      writer->Divider_line(max_length);
      writer->BeginRow(); writer->WriteCell(title, max_length); writer->EndRow();
      writer->Divider_line(max_length);
      writer->BeginRow(); writer->WriteCell(" ", max_length); writer->EndRow();
      writer->Divider_line(max_length);
      cout<<endl;
      continue;
    }
    for(auto index : indexes)
      if (index->GetIndexName().length() > max_length)
        max_length = index->GetIndexName().length() + 10;
    writer->Divider_line(max_length);
    writer->BeginRow(); writer->WriteCell(title, max_length); writer->EndRow();
    writer->Divider_line(max_length);
    for(auto index : indexes){
      writer->BeginRow(); writer->WriteCell(index->GetIndexName(), max_length); writer->EndRow();
    }
    writer->Divider_line(max_length);
    cout<<endl;
  }
  end = clock();
  cout<<"Query OK. (" <<setprecision(3)<<(double)(end-start)/CLOCKS_PER_SEC<<"sec)"<< endl;
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t ExecuteEngine::ExecuteCreateIndex(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteCreateIndex" << std::endl;
#endif
  clock_t start, end;
  start = clock();
  if (current_db_ == "")
  {
    end = clock();
    cout<<"No database selected. Please select it first.("<<setprecision(3)<<(double)(end-start)/CLOCKS_PER_SEC<<"sec)"<<endl;
    return DB_FAILED;
  }
  std::string index_name = ast->child_->val_;
  std::string table_name = ast->child_->next_->val_;
  //0. Check the validity of the table and index_name
  std::vector<TableInfo *> tables;
  TableInfo * table_info;
  TableHeap * table_heap;
  dbs_[current_db_]->catalog_mgr_->GetTables(tables);
  bool table_exist_flag = false;
  for(auto table: tables)
    if(table->GetTableName() == table_name)
    {
      table_exist_flag = true;
      table_info = table;
      table_heap = table->GetTableHeap();
      break;
    }
  if(table_exist_flag == false){
    end = clock();
    cout<<"Table not exists."<<endl;
    return DB_FAILED;
  }
  std::vector<IndexInfo *> indexes;
  dbs_[current_db_]->catalog_mgr_->GetTableIndexes(table_name, indexes);
  for(auto index: indexes)
    if(index->GetIndexName() == index_name)
    {
      end = clock();
      cout<<"Index "<<index_name<<" already exists"<<" in "<<table_name<<endl;
      return DB_FAILED;
    }
  std::string index_type = "btree";
  pSyntaxNode index_node = ast->child_->next_->next_ , index_node_end= ast->child_;
  //1. Judge the index type existence
  while(index_node_end->next_ != nullptr)
    index_node_end = index_node_end->next_;
  if(index_node_end->val_ != NULL && string(index_node_end->val_) == "index type"){
    index_type = index_node_end->child_->val_;
    index_node_end = index_node_end->child_;
  }
  //2. Get the columnlist
  std::vector<std::string> column_names;
  pSyntaxNode column_node = index_node->child_;
  while(column_node != nullptr){
    column_names.push_back(column_node->val_);
    column_node = column_node->next_;
  }
  Transaction txn;
  IndexInfo *index_info = nullptr;
  dbs_[current_db_]->catalog_mgr_->CreateIndex(table_name, index_name, column_names, &txn, index_info, index_type);
  for(TableIterator iter = table_info->GetTableHeap()->Begin(nullptr); iter != table_info->GetTableHeap()->End(); iter++){
    Row row = *iter;
    std::vector<Column*> columns = index_info->GetIndexKeySchema()->GetColumns();
    ASSERT(columns.size() == 1, "InsertExecutor only support single column index");
    std::vector<Field> Fields;
    Fields.push_back(*(row.GetField(columns[0]->GetTableInd())));
    Row index_row(Fields);
    RowId rid = row.GetRowId();
    index_row.SetRowId(rid);

    index_info->GetIndex()->InsertEntry(index_row, rid, nullptr);
  }

  end = clock();
  cout<<"Query OK, 0 rows affected ("<<setprecision(3)<<(double)(end-start)/CLOCKS_PER_SEC<<"sec)"<<endl;
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t ExecuteEngine::ExecuteDropIndex(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteDropIndex" << std::endl;
#endif
  clock_t start, end;
  start = clock();
  if(current_db_ == ""){
    end = clock();
    cout<<"No database selected. Please select it first."<<endl;
    return DB_FAILED;
  }
  std::string index_name = ast->child_->val_;
  std::vector<TableInfo *> tables;
  dbs_[current_db_]->catalog_mgr_->GetTables(tables);
  bool deleted_flag = false;
  for(auto table : tables){
    int status = dbs_[current_db_]->catalog_mgr_->DropIndex(table->GetTableName(), index_name);
    if(status == DB_SUCCESS)
      deleted_flag = true;
  }
  if(deleted_flag == false){
    end = clock();
    cout<<"index "<<index_name<<" not exists"<<setprecision(3)<<(double)(end-start)/CLOCKS_PER_SEC<<"sec)"<<endl;
    return DB_FAILED;
  }
  end = clock();
  cout<<"Query OK. (" <<setprecision(3)<<(double)(end-start)/CLOCKS_PER_SEC<<"sec)"<< endl;
  return DB_SUCCESS;
}


dberr_t ExecuteEngine::ExecuteTrxBegin(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteTrxBegin" << std::endl;
#endif
  return DB_FAILED;
}

dberr_t ExecuteEngine::ExecuteTrxCommit(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteTrxCommit" << std::endl;
#endif
  return DB_FAILED;
}

dberr_t ExecuteEngine::ExecuteTrxRollback(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteTrxRollback" << std::endl;
#endif
  return DB_FAILED;
}

/**
 * TODO: Student Implement
 */



dberr_t ExecuteEngine::ExecuteExecfile(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteExecfile" << std::endl;
#endif
//  auto start1 = std::chrono::steady_clock::now();
  struct timeval t1,t2;
  gettimeofday(&t1,NULL);
  std::string file_name = ast->child_->val_;
  ifstream in(file_name, ios::in);
  std::string line;
  while(!in.eof()){
    char cmd[1024];
    memset(cmd, 0, sizeof(cmd));
    //getline(in, line);
//    YY_BUFFER_STATE bp = yy_scan_string(line.c_str());
    while(in.peek() == '\n' || in.peek() == '\r')
      in.get();
    in.get(cmd, 1024, ';');
    in.get();
    int len = strlen(cmd);
//    cout<<cmd<<endl;
    cmd[len] = ';';
    cmd[len+1] = '\0';
//    cout<<cmd<<endl;
    YY_BUFFER_STATE bp = yy_scan_string(cmd);

    if (bp == nullptr) {
      LOG(ERROR) << "Failed to create yy buffer state." << std::endl;
      exit(1);
    }
    yy_switch_to_buffer(bp);
    // init parser module
    MinisqlParserInit();
    // parse
    yyparse();
    // parse result handle
    if (MinisqlParserGetError()) {
      // error
      printf("%s\n", MinisqlParserGetErrorMessage());
    } else {
      // Comment them out if you don't need to debug the syntax tree
    }
    this->Execute(MinisqlGetParserRootNode());
    // clean memory after parse
    MinisqlParserFinish();
    yy_delete_buffer(bp);
    yylex_destroy();
    // quit condition
  }
//  auto end1 =  std::chrono::steady_clock::now();
//  double duration_time =
//          double((std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1)).count());
  gettimeofday(&t2,NULL);
  double duration = (t2.tv_sec - t1.tv_sec) + (double)(t2.tv_usec - t1.tv_usec)/1000000.0;
//  cout <<" Query OK.(" << setprecision(3)<<duration_time<< "sec)" << endl;
  cout <<" Query OK.(" << setprecision(3)<<duration<< "sec)" << endl;
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t ExecuteEngine::ExecuteQuit(pSyntaxNode ast, ExecuteContext *context) {
#ifdef ENABLE_EXECUTE_DEBUG
  LOG(INFO) << "ExecuteQuit" << std::endl;
#endif
  for(auto db : dbs_){
    delete db.second;
  }
  dbs_.clear();
  return DB_QUIT;
}
