#include "catalog/catalog.h"

void CatalogMeta::SerializeTo(char *buf) const {
    ASSERT(GetSerializedSize() <= PAGE_SIZE, "Failed to serialize catalog metadata to disk.");
    uint32_t offset = 0;
    MACH_WRITE_UINT32(buf, CATALOG_METADATA_MAGIC_NUM);
    buf += 4;
    offset += 4;
    MACH_WRITE_UINT32(buf, table_meta_pages_.size());
    buf += 4;
    offset += 4;

    MACH_WRITE_UINT32(buf, index_meta_pages_.size());
    buf += 4;
    offset += 4;
  for (auto iter : table_meta_pages_) {
    MACH_WRITE_TO(table_id_t, buf, iter.first);
        buf += 4;
        offset += 4;
    MACH_WRITE_TO(page_id_t, buf, iter.second);
        buf += 4;
        offset += 4;
  }
    for (auto iter : index_meta_pages_) {
        MACH_WRITE_TO(index_id_t, buf, iter.first);
        buf += 4;
        offset += 4;

        MACH_WRITE_TO(page_id_t, buf, iter.second);
        buf += 4;
        offset += 4;
    }
  ASSERT(offset == GetSerializedSize(), "Wrong serialized size in CatalogMeta::SerializeTo.\n");
}

CatalogMeta *CatalogMeta::DeserializeFrom(char *buf) {
    // check valid
    uint32_t magic_num = MACH_READ_UINT32(buf);
    uint32_t offset = 0;
    buf += 4;
    offset += 4;
    ASSERT(magic_num == CATALOG_METADATA_MAGIC_NUM, "Failed to deserialize catalog metadata from disk.");
    // get table and index nums
    uint32_t table_nums = MACH_READ_UINT32(buf);
    buf += 4;
    offset += 4;
    uint32_t index_nums = MACH_READ_UINT32(buf);
    buf += 4;
    offset += 4;
    // create metadata and read value
    CatalogMeta *meta = new CatalogMeta();
    for (uint32_t i = 0; i < table_nums; i++) {
        auto table_id = MACH_READ_FROM(table_id_t, buf);
        buf += 4;
        offset += 4;
        auto table_heap_page_id = MACH_READ_FROM(page_id_t, buf);
        buf += 4;
        offset += 4;
        meta->table_meta_pages_.emplace(table_id, table_heap_page_id);
    }
    for (uint32_t i = 0; i < index_nums; i++) {
        auto index_id = MACH_READ_FROM(index_id_t, buf);
        buf += 4;
        offset += 4;
        auto index_page_id = MACH_READ_FROM(page_id_t, buf);
        buf += 4;
        offset += 4;
        meta->index_meta_pages_.emplace(index_id, index_page_id);
    }
    ASSERT(offset == meta->GetSerializedSize(), "Wrong serialized size in CatalogMeta::SerializeTo.\n");
    return meta;
}

/**
 * TODO: Student Implement
 */
uint32_t CatalogMeta::GetSerializedSize() const {
  return 3 * sizeof(uint32_t) + table_meta_pages_.size() * (sizeof(table_id_t) + sizeof(page_id_t)) +
         index_meta_pages_.size() * (sizeof(index_id_t) + sizeof(page_id_t));
}

CatalogMeta::CatalogMeta() {}

/**
 * TODO: Student Implement
 */
CatalogManager::CatalogManager(BufferPoolManager *buffer_pool_manager, LockManager *lock_manager,
                               LogManager *log_manager, bool init)
    : buffer_pool_manager_(buffer_pool_manager), lock_manager_(lock_manager), log_manager_(log_manager) {

  if(init){
    //Initialize the catalog_meta_
    catalog_meta_ = new CatalogMeta();
    next_table_id_ = next_index_id_ = 0;
  }
  else{
    //Get the catalog meta
    Page * catalog_page = buffer_pool_manager_->FetchPage(CATALOG_META_PAGE_ID);
    catalog_meta_ = CatalogMeta::DeserializeFrom(catalog_page->GetData());
    //Load the tables into the catalog manager
    for(auto iter : catalog_meta_->table_meta_pages_){
      ASSERT(LoadTable(iter.first, iter.second) == DB_SUCCESS, "Failed to load table into catalog manager");
    }
    //Load the indexs into the catalog manager
    for(auto iter : catalog_meta_->index_meta_pages_){
      ASSERT(LoadIndex(iter.first, iter.second) == DB_SUCCESS, "Failed to load index into catalog manager");
    }
    next_table_id_ = catalog_meta_->GetNextTableId();
    next_index_id_ = catalog_meta_->GetNextIndexId();
    buffer_pool_manager->UnpinPage(CATALOG_META_PAGE_ID, true);
  }
}

CatalogManager::~CatalogManager() {
  FlushCatalogMetaPage();
  delete catalog_meta_;
  for (auto iter : tables_) {
    delete iter.second;
  }
  for (auto iter : indexes_) {
    delete iter.second;
  }
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::CreateTable(const string &table_name, TableSchema *schema,
                                    Transaction *txn, TableInfo *&table_info) {
  //First Check the table whether is already exist
  if(table_names_.find(table_name) != table_names_.end()){
    return DB_TABLE_ALREADY_EXIST;
  }
  if (table_info == NULL)
    table_info = TableInfo::Create();
  //Create the table
  TableHeap * table_heap = TableHeap::Create(buffer_pool_manager_, Schema::DeepCopySchema(schema), txn, log_manager_ ,lock_manager_);
  TableMetadata * table_metadata = TableMetadata::Create(next_table_id_, table_name, table_heap->GetFirstPageId(), Schema::DeepCopySchema(schema));
  table_info->Init(table_metadata, table_heap);
  //Add the table into the catalog manager
  table_names_.emplace(table_name, next_table_id_);
  tables_.emplace(next_table_id_, table_info);
  //Add the table into the catalog meta
  catalog_meta_->AddTableMetaPage(next_table_id_, table_heap->GetFirstPageId());
  next_table_id_++;
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::GetTable(const string &table_name, TableInfo *&table_info) {
  //First Check the table whether exists
  if (table_names_.find(table_name) == table_names_.end()) {
    return DB_TABLE_NOT_EXIST;
  }
  //Get the table
  table_info = tables_[table_names_[table_name]];
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::GetTables(vector<TableInfo *> &tables) const {
  for(auto iter: tables_){
    tables.push_back(iter.second);
  }
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::CreateIndex(const std::string &table_name, const string &index_name,
                                    const std::vector<std::string> &index_keys, Transaction *txn,
                                    IndexInfo *&index_info, const string &index_type) {
  //First Check the table whether exists or not
  TableInfo * table_info = TableInfo::Create();
  if(table_names_.find(table_name) == table_names_.end()){
    return DB_TABLE_NOT_EXIST;
  }
  table_info = tables_[table_names_[table_name]];
  //Then check the index whether exists or not
  if(index_names_[table_name].find(index_name) != index_names_[table_name].end()){
    return DB_INDEX_ALREADY_EXIST;
  }
  index_info = IndexInfo::Create();
  //Create the index
  std::vector<uint32_t> key_map;
  for(int i=0; i < index_keys.size(); i++){
    uint32_t column_index;
    if(table_info->GetSchema()->GetColumnIndex(index_keys[i], column_index) == DB_COLUMN_NAME_NOT_EXIST)
      return DB_COLUMN_NAME_NOT_EXIST;
    key_map.push_back(column_index);
  }
  IndexMetadata * index_metadata = IndexMetadata::Create(next_index_id_, index_name, table_info->GetTableId(), key_map);
  index_info->Init(index_metadata, table_info, buffer_pool_manager_);
  //Add the index into the catalog manager
  index_names_[table_name].emplace(index_name, next_index_id_);
  indexes_.emplace(next_index_id_, index_info);
  //Add the index into the catalog meta
  catalog_meta_->AddIndexMetaPage(next_index_id_, INDEX_ROOTS_PAGE_ID);
  next_index_id_++;
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::GetIndex(const std::string &table_name, const std::string &index_name,
                                 IndexInfo *&index_info) const {
  //First Check the table whether exists or not
  if(table_names_.find(table_name) == table_names_.end()){
    return DB_TABLE_NOT_EXIST;
  }
  //Then check the index whether exists or not
  if(index_names_.find(index_name) != index_names_.end()){
    return DB_INDEX_ALREADY_EXIST;
  }
  index_info = indexes_.at(index_names_.at(table_name).at(index_name));
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::GetTableIndexes(const std::string &table_name, std::vector<IndexInfo *> &indexes) const {
  if(table_names_.find(table_name) == table_names_.end()){
    return DB_TABLE_NOT_EXIST;
  }
  for(auto iter : index_names_.at(table_name)){
    indexes.push_back(indexes_.at(iter.second));
  }
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::DropTable(const string &table_name) {
  //First Check the table whether exists or not
  if(table_names_.find(table_name) == table_names_.end()){
    return DB_TABLE_NOT_EXIST;
  }
  //Drop all the index on this table
  for(auto iter : index_names_[table_name]){
    IndexInfo * index_info = indexes_[iter.second];
    delete index_info;
    indexes_.erase(iter.second);
    catalog_meta_->index_meta_pages_.erase(iter.second);
  }
  index_names_.erase(table_name);
  next_index_id_ -= index_names_[table_name].size();

  //Drop the table
  TableInfo * table_info = tables_[table_names_[table_name]];
  delete table_info;
  tables_.erase(table_names_[table_name]);
  table_names_.erase(table_name);

  catalog_meta_->table_meta_pages_.erase(table_info->GetTableId());
  next_table_id_--;

  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::DropIndex(const string &table_name, const string &index_name) {
  //First Check the table whether exists or not
  if(table_names_.find(table_name) == table_names_.end()){
    return DB_TABLE_NOT_EXIST;
  }
  TableInfo * table_info = tables_[table_names_[table_name]];
  //Then check the index whether exists or not
  if(index_names_.find(index_name) == index_names_.end()){
    return DB_INDEX_NOT_FOUND;
  }
  //Drop the index
  IndexInfo * index_info = indexes_[index_names_[table_name][index_name]];
  delete index_info;
  indexes_.erase(index_names_[table_name][index_name]);
  index_names_[table_name].erase(index_name);
  catalog_meta_->index_meta_pages_.erase(index_names_[table_name][index_name]);
  next_index_id_--;

  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
 //Flush the catalog meta page into the disk
dberr_t CatalogManager::FlushCatalogMetaPage() const {
  Page * catalog_page = buffer_pool_manager_->FetchPage(CATALOG_META_PAGE_ID);
  if(catalog_page == nullptr){
    LOG(ERROR) << "Failed to fetch the catalog meta page in the FlushCatalogMetaPage" << endl;
    return DB_FAILED;
  }
  char * data = catalog_page->GetData();
  //Serialize the catalog meta data
  catalog_meta_->SerializeTo(data);
  data += catalog_meta_->GetSerializedSize();
  buffer_pool_manager_->UnpinPage(CATALOG_META_PAGE_ID, true);
//  buffer_pool_manager_->FlushPage(CATALOG_META_PAGE_ID);
  //Serialize the table meta data
  for(auto table_iter : catalog_meta_->table_meta_pages_){
    Page * table_page = buffer_pool_manager_->FetchPage(table_iter.second);
    data = table_page->GetData();
    tables_.at(table_iter.first)->GetTableMetadata()->SerializeTo(data);
    data += tables_.at(table_iter.first)->GetTableMetadata()->GetSerializedSize();
    buffer_pool_manager_->UnpinPage(table_iter.second, true);
  }
  //Serialize the index meta data
  for(auto index_iter : catalog_meta_->index_meta_pages_){
    Page * index_page = buffer_pool_manager_->FetchPage(index_iter.second);
    data = index_page->GetData();
    indexes_.at(index_iter.first)->GetIndexMetadata()->SerializeTo(data);
    data += indexes_.at(index_iter.first)->GetIndexMetadata()->GetSerializedSize();
    buffer_pool_manager_->UnpinPage(index_iter.second, true);
  }
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::LoadTable(const table_id_t table_id, const page_id_t page_id) {
  Page * table_page = buffer_pool_manager_->FetchPage(page_id);
  if(table_page == nullptr){
    LOG(ERROR) << "Failed to fetch table page in the LoadTable" << endl;
    return DB_FAILED;
  }
  TableMetadata * meta_data = nullptr;
  if(TableMetadata::DeserializeFrom(table_page->GetData(), meta_data) != meta_data->GetSerializedSize()){
    LOG(ERROR) << "The Deserialize outcome doesn't match the size of Serialize " << endl;
    return DB_FAILED;
  }
  if(meta_data->GetTableId() != table_id){
    LOG(ERROR) << "The table id doesn't match the table id in the meta data" << endl;
    return DB_FAILED;
  }
  TableHeap *table_heap = TableHeap::Create(buffer_pool_manager_, Schema::DeepCopySchema(meta_data->GetSchema()), nullptr, log_manager_ ,lock_manager_);
  TableInfo * table_info = TableInfo::Create();
  table_info->Init(meta_data, table_heap);
  table_names_.emplace(meta_data->GetTableName(), meta_data->GetTableId());
  std::cout<<"Table_name: "<<meta_data->GetTableName()<<" Table_id: "<<table_names_.at(meta_data->GetTableName())<<std::endl;
  tables_.emplace(meta_data->GetTableId(), table_info);
  std::cout<<"Table_id: "<<table_names_.at(meta_data->GetTableName())<<" Table_id: "<<tables_.at(table_names_.at(meta_data->GetTableName()))<<std::endl;
  buffer_pool_manager_->UnpinPage(page_id, true);
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::LoadIndex(const index_id_t index_id, const page_id_t page_id) {
  Page * index_page = buffer_pool_manager_->FetchPage(page_id);
  if(index_page == nullptr){
    LOG(ERROR) << "Failed to fetch index page in the LoadIndex" << endl;
    return DB_FAILED;
  }
  IndexMetadata * meta_data = nullptr;
  if(IndexMetadata::DeserializeFrom(index_page->GetData(), meta_data) != meta_data->GetSerializedSize()){
    LOG(ERROR) << "The Deserialize outcome doesn't match the size of Serialize " << endl;
    return DB_FAILED;
  }
  if(meta_data->GetIndexId() != index_id){
    LOG(ERROR) << "The index id doesn't match the index id in the meta data" << endl;
    return DB_FAILED;
  }
  TableInfo * table_info = tables_[meta_data->GetTableId()];
  IndexInfo * index_info = IndexInfo::Create();
  index_info->Init(meta_data, table_info, buffer_pool_manager_);
  //Insert the entry
  for(TableIterator table_iter = table_info->GetTableHeap()->Begin(nullptr); table_iter != table_info->GetTableHeap()->End(); table_iter++){
    std::vector<Field> fields;
    for(auto key_iter = meta_data->GetKeyMapping().begin(); key_iter != meta_data->GetKeyMapping().end(); key_iter++){
      fields.push_back(*(table_iter->GetField(*key_iter)));
    }
    Row row(fields);
    RowId rid(table_iter->GetRowId().GetPageId(), table_iter->GetRowId().GetSlotNum());
    index_info->GetIndex()->InsertEntry(row, rid, nullptr);
  }
  index_names_[table_info->GetTableName()].emplace(meta_data->GetIndexName(), meta_data->GetIndexId());
  cout<<"Index_name: "<<meta_data->GetIndexName()<<" Index_id: "<<index_names_[table_info->GetTableName()].at(meta_data->GetIndexName())<<endl;
  indexes_.emplace(meta_data->GetIndexId(), index_info);
//  IndexInfo * tset_info = indexes_[meta_data->GetIndexId()];
  buffer_pool_manager_->UnpinPage(page_id, true);
  return DB_SUCCESS;
}

/**
 * TODO: Student Implement
 */
dberr_t CatalogManager::GetTable(const table_id_t table_id, TableInfo *&table_info) {
  if(tables_.find(table_id) == tables_.end()){
    LOG(ERROR) << "The table id doesn't exist in the tables_" << endl;
    return DB_TABLE_NOT_EXIST;
  }
  table_info = tables_[table_id];
  return DB_SUCCESS;
}