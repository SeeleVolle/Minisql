#include "record/row.h"

/**
 * TODO: Student Implement
 */
uint32_t Row::SerializeTo(char *buf, Schema *schema) const {
  ASSERT(schema != nullptr, "Invalid schema before serialize.");
  ASSERT(schema->GetColumnCount() == fields_.size(), "Fields size do not match schema's column size.\n");
  uint32_t offset = 0;
  size_t field_count = this->GetFieldCount();
  //If the field count is 0, means the row is empty
  if(field_count == 0){
    return 0;
  }
  //Write the field count to the buffer, the MATCH_WRITE_TO just write a type value
  MACH_WRITE_TO(size_t, buf, field_count);
  offset = offset + sizeof(size_t);
  //Create a bitmap page to mark the null fields
  char *bitmap = new char(field_count / 8 + 1);
  memset(bitmap, 0, sizeof(char)*(field_count / 8 + 1));
  for(int i = 0; i < field_count; i++){
    //If the field is null, mark the bitmap as 1
    if(fields_[i]->IsNull()){
      bitmap[i / 8] |= (1 << (i % 8));
    }
  }
  //Write the bitmap into the buf
  for(int i=0; i < field_count / 8 + 1; i++){
    MACH_WRITE_TO(char, buf + offset, bitmap[i]);
    offset = offset + sizeof(char);
  }
  //Write the fields into the buf
  for(int i=0; i < field_count; i++){
    offset = offset + fields_[i]->SerializeTo(buf + offset);
  }
  ASSERT(offset == this->GetSerializedSize(schema), "Unexpected serialize size in Row::Serialize.");
  return offset;
}

uint32_t Row::DeserializeFrom(char *buf, Schema *schema) {
  ASSERT(schema != nullptr, "Invalid schema before serialize.");
  ASSERT(fields_.empty(), "Non empty field in row.");
  uint32_t offset = 0;
  //Read the field count from the buf
  size_t field_count = MACH_READ_FROM(size_t, buf);
  offset = offset + sizeof(size_t);
  char *bitmap = new char(field_count / 8 + 1);
  //Read the bitmap from the buf
  for(int i = 0; i < field_count / 8 + 1; i++) {
    bitmap[i] = MACH_READ_FROM(char, buf + offset);
    offset = offset + sizeof(char);
  }
  //Read the fields from the buf
  if(!fields_.empty())
    this->fields_.clear();
  for(int i=0; i<field_count; i++) {
    Field *new_field;
    offset += Field::DeserializeFrom(buf + offset, schema->GetColumn(i)->GetType(), &new_field, bitmap[i / 8] & (1<< (i % 8)));
    this->fields_.push_back(new_field);
  }
  ASSERT(offset == this->GetSerializedSize(schema), "Unexpected deserialize size in Row::Deserialize.\n");
  return offset;
}

uint32_t Row::GetSerializedSize(Schema *schema) const {
  ASSERT(schema != nullptr, "Invalid schema before serialize.");
  ASSERT(schema->GetColumnCount() == fields_.size(), "Fields size do not match schema's column size.");
  uint32_t size = 0;
  //If there is not any field in a row, return 0
  if(fields_.empty()){
    return 0;
  }
  for(int i = 0; i < this->GetFieldCount(); i++){
    if(fields_[i]->IsNull() == false){
      size += fields_[i]->GetSerializedSize();
    }
  }
  size += sizeof(size_t) + sizeof(char) * (this->GetFieldCount() / 8 + 1);
  return size;
}

void Row::GetKeyFromRow(const Schema *schema, const Schema *key_schema, Row &key_row) {
  auto columns = key_schema->GetColumns();
  std::vector<Field> fields;
  uint32_t idx;
  for (auto column : columns) {
    schema->GetColumnIndex(column->GetName(), idx);
    fields.emplace_back(*this->GetField(idx));
  }
  key_row = Row(fields);
}
