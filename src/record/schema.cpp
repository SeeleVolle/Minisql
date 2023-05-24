#include "record/schema.h"

/**
 * TODO: Student Implement
 */
uint32_t Schema::SerializeTo(char *buf) const {
  uint32_t offset = 0;
  //magic_number
  MACH_WRITE_UINT32(buf, SCHEMA_MAGIC_NUM);
  offset += sizeof(uint32_t);
  //column_count
  MACH_WRITE_UINT32(buf + offset, this->GetColumnCount());
  offset += sizeof(uint32_t);
  //column
  for (uint32_t i = 0; i < this->GetColumnCount(); i++) {
    offset += this->columns_[i]->SerializeTo(buf + offset);
  }
  //is_manage
  MACH_WRITE_TO(bool, buf + offset, this->is_manage_);
  return offset;
}

uint32_t Schema::GetSerializedSize() const {
  uint32_t size = 0;
  size = size + sizeof(bool) + 2 * sizeof(uint32_t);
  for(uint32_t i = 0; i < this->GetColumnCount(); i++){
    size += this->columns_[i]->GetSerializedSize();
  }
  return size;
}

uint32_t Schema::DeserializeFrom(char *buf, Schema *&schema) {
  if(schema != nullptr){
    LOG(WARNING) << "Pointer to column is not null in column deserialize." << std::endl;
  }
  uint32_t offset = 0;
  //Check magic_number
  uint32_t magic_number = 0;
  magic_number = MACH_READ_UINT32(buf + offset);
  ASSERT(magic_number == SCHEMA_MAGIC_NUM, "magic_number != SCHEMA_MAGIC_NUM in schema deserialize");
  offset += sizeof(uint32_t);
  //column_count
  uint32_t column_count = MACH_READ_UINT32(buf + offset);
  offset += sizeof(uint32_t);
  //column
  std::vector<Column *> columns;
  Column *schema_column = nullptr;
  for(uint32_t i = 0; i < column_count; i++){
    offset += Column::DeserializeFrom(buf + offset, schema_column);
    columns.push_back(schema_column);
  }
  //is_manage
  bool is_manage = MACH_READ_FROM(bool, buf + offset);
  offset += sizeof(bool);
  //Allocate a new Schema
  schema = new Schema(columns, is_manage);
  return offset;
}