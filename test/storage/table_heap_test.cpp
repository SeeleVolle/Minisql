#include "storage/table_heap.h"

#include <unordered_map>
#include <vector>

#include "common/instance.h"
#include "gtest/gtest.h"
#include "record/field.h"
#include "record/schema.h"
#include "utils/utils.h"

static string db_file_name = "table_heap_test.db";
using Fields = std::vector<Field>;

TEST(TableHeapTest, TableHeapSampleTest) {
  // init testing instance
  auto disk_mgr_ = new DiskManager(db_file_name);
  auto bpm_ = new BufferPoolManager(DEFAULT_BUFFER_POOL_SIZE, disk_mgr_);
  const int row_nums = 10000;
  // create schema
  std::vector<Column *> columns = {new Column("id", TypeId::kTypeInt, 0, false, false),
                                   new Column("name", TypeId::kTypeChar, 64, 1, true, false),
                                   new Column("account", TypeId::kTypeFloat, 2, true, false)};
  auto schema = std::make_shared<Schema>(columns);
  // create rows
  std::unordered_map<int64_t, Fields *> row_values;
  uint32_t size = 0;
  TableHeap *table_heap = TableHeap::Create(bpm_, schema.get(), nullptr, nullptr, nullptr);
  for (int i = 0; i < row_nums; i++) {
    int32_t len = RandomUtils::RandomInt(0, 64);
    char *characters = new char[len];
    RandomUtils::RandomString(characters, len);
    Fields *fields =
        new Fields{Field(TypeId::kTypeInt, i), Field(TypeId::kTypeChar, const_cast<char *>(characters), len, true),
                   Field(TypeId::kTypeFloat, RandomUtils::RandomFloat(-999.f, 999.f))};
    Row row(*fields);
    ASSERT_TRUE(table_heap->InsertTuple(row, nullptr));
    if (row_values.find(row.GetRowId().Get()) != row_values.end()) {
      std::cout << row.GetRowId().Get() << std::endl;
      ASSERT_TRUE(false);
    } else {
      row_values.emplace(row.GetRowId().Get(), fields);
      size++;
    }
    delete[] characters;
  }

  ASSERT_EQ(row_nums, row_values.size());
  ASSERT_EQ(row_nums, size);
  for (auto row_kv : row_values) {
    size--;
    Row row(RowId(row_kv.first));
    table_heap->GetTuple(&row, nullptr);
    ASSERT_EQ(schema.get()->GetColumnCount(), row.GetFields().size());
    for (size_t j = 0; j < schema.get()->GetColumnCount(); j++) {
      ASSERT_EQ(CmpBool::kTrue, row.GetField(j)->CompareEquals(row_kv.second->at(j)));
    }
    // free spaces
    delete row_kv.second;
  }
  ASSERT_EQ(size, 0);
  //Test for tableIterator
  for(TableIterator iter = table_heap->Begin(nullptr); iter != table_heap->End(); iter++){
    Row row(*iter);
    ASSERT_EQ(schema.get()->GetColumnCount(), row.GetFields().size());
    for (size_t j = 0; j < schema.get()->GetColumnCount(); j++) {
      ASSERT_EQ(CmpBool::kTrue, row.GetField(j)->CompareEquals(row_values[row.GetRowId().Get()]->at(j)));
    }
  }
}

//TEST(TableHeapTest, myTableHeapSampleTest) {
//  // init testing instance
//  auto disk_mgr_ = new DiskManager(db_file_name);
//  auto bpm_ = new BufferPoolManager(DEFAULT_BUFFER_POOL_SIZE, disk_mgr_);
//  const int row_nums = 10000;
//  // create schema
//  std::vector<Column *> columns = {new Column("id", TypeId::kTypeInt, 0, false, false),
//                                   new Column("name", TypeId::kTypeChar, 64, 1, true, false),
//                                   new Column("account", TypeId::kTypeFloat, 2, true, false)};
//  auto schema = std::make_shared<Schema>(columns);
//  // create rows
//  std::unordered_map<int64_t, Fields *> row_values;
//  uint32_t size = 0;
//  TableHeap *table_heap = TableHeap::Create(bpm_, schema.get(), nullptr, nullptr, nullptr);
//  for (int i = 0; i < row_nums; i++) {
//    int32_t len = RandomUtils::RandomInt(0, 64);
//    char *characters = new char[len];
//    RandomUtils::RandomString(characters, len);
//    Fields *fields =
//            new Fields{Field(TypeId::kTypeInt, i), Field(TypeId::kTypeChar, const_cast<char *>(characters), len, true),
//                       Field(TypeId::kTypeFloat, RandomUtils::RandomFloat(-999.f, 999.f))};
//    Row row(*fields);
//    ASSERT_TRUE(table_heap->InsertTuple(row, nullptr));
//    //add test for update
//    Fields *myfields =new Fields {Field(TypeId::kTypeInt, 188),
//                                  Field(TypeId::kTypeChar, const_cast<char *>("minisql"), strlen("minisql"), false),
//                                  Field(TypeId::kTypeFloat, 19.99f)};
//    Row row2(*myfields);
//    int cnt = -1;
//    if( (cnt = table_heap->UpdateTuple(row2, row.GetRowId(), nullptr)) == 0)
//    {
//      cout<<i<<endl;
//      ASSERT(false, "update failed");
//    }
////    ASSERT_TRUE(table_heap->MarkDelete(row2.GetRowId(),nullptr));
////    table_heap->ApplyDelete(row2.GetRowId(),nullptr);
////    ASSERT_TRUE(table_heap->InsertTuple(row2, nullptr));
//    Row check_row(RowId(row2.GetRowId()));
//    table_heap->GetTuple(&check_row, nullptr);
//
//    for (size_t j = 0; j < schema.get()->GetColumnCount(); j++) {
//      if(CmpBool::kTrue != row2.GetField(j)->CompareEquals(*(check_row.GetField(j))))
//      {
//        cout<<i<<" "<<cnt<<endl;
//        cout<<row2.GetField(j)->type_id_<<" "<<check_row.GetField(j)->type_id_<<" "<<row.GetField(j)->type_id_<<endl;
//        cout<<row2.GetField(j)->is_null_<<" "<<check_row.GetField(j)->is_null_<<" "<<row.GetField(j)->is_null_<<endl;
//        cout<<row2.GetField(j)->value_.integer_<<" "<<check_row.GetField(j)->value_.integer_<<" "<<row.GetField(j)->value_.integer_<<endl;
//        exit(1);
//      }
//    }
//
//    if (row_values.find(row.GetRowId().Get()) != row_values.end()) {
//      std::cout << row.GetRowId().Get() << std::endl;
//      ASSERT_TRUE(false);
//    } else {
//      row_values.emplace(row.GetRowId().Get(), myfields);
//      size++;
//    }
//    delete[] characters;
//  }
//
//  ASSERT_EQ(row_nums, row_values.size());
//  ASSERT_EQ(row_nums, size);
//  for (auto row_kv : row_values) {
//    size--;
//    Row row(RowId(row_kv.first));
//    table_heap->GetTuple(&row, nullptr);
//    ASSERT_EQ(schema.get()->GetColumnCount(), row.GetFields().size());
//    for (size_t j = 0; j < schema.get()->GetColumnCount(); j++) {
//      ASSERT_EQ(CmpBool::kTrue, row.GetField(j)->CompareEquals(row_kv.second->at(j)));
//    }
//    // free spaces
//    delete row_kv.second;
//  }
//  ASSERT_EQ(size, 0);
//}
