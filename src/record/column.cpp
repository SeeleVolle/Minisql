#include "record/column.h"
#include "glog/logging.h"

Column::Column(std::string column_name, TypeId type, uint32_t index, bool nullable, bool unique)
    : name_(std::move(column_name)), type_(type), table_ind_(index), nullable_(nullable), unique_(unique) {
  ASSERT(type != TypeId::kTypeChar, "Wrong constructor for CHAR type.");
  switch (type) {
    case TypeId::kTypeInt:
      len_ = sizeof(int32_t);
      break;
    case TypeId::kTypeFloat:
      len_ = sizeof(float_t);
      break;
    default:
      ASSERT(false, "Unsupported column type.");
  }
}

Column::Column(std::string column_name, TypeId type, uint32_t length, uint32_t index, bool nullable, bool unique)
    : name_(std::move(column_name)),
      type_(type),
      len_(length),
      table_ind_(index),
      nullable_(nullable),
      unique_(unique) {
  ASSERT(type == TypeId::kTypeChar, "Wrong constructor for non-VARCHAR type.");
}

Column::Column(const Column *other)
    : name_(other->name_),
      type_(other->type_),
      len_(other->len_),
      table_ind_(other->table_ind_),
      nullable_(other->nullable_),
      unique_(other->unique_) {}

/**
* TODO: Student Implement
*/
uint32_t Column::SerializeTo(char *buf) const {
  uint32_t offset = 0;
  //Magic name
  MACH_WRITE_UINT32(buf + offset, COLUMN_MAGIC_NUM);
  offset += sizeof(uint32_t);
  //Name length
  MACH_WRITE_TO(size_t, buf + offset, name_.length());
  offset += sizeof(size_t);
  //Name content
  MACH_WRITE_STRING(buf+ offset, name_);
  offset += name_.length();
  //Type
  MACH_WRITE_TO(TypeId, buf + offset, type_);
  offset += sizeof(type_);
  //max_length
  MACH_WRITE_UINT32(buf + offset, len_);
  offset += sizeof(uint32_t);
  //colomn position
  MACH_WRITE_UINT32(buf + offset, table_ind_);
  offset += sizeof(uint32_t);
  //nullable flag
  MACH_WRITE_TO(bool, buf + offset, nullable_);
  offset += sizeof(bool);
  //unique flag
  MACH_WRITE_TO(bool, buf + offset, unique_);
  offset += sizeof(bool);
  ASSERT(offset == GetSerializedSize(), "Wrong serialized size in Column::SerializeTo.\n");
  return offset;
}

/**
 * TODO: Student Implement
 */
uint32_t Column::GetSerializedSize() const {
  return 3 * sizeof(uint32_t) + sizeof(size_t) + sizeof(type_) + name_.length() + 2 * sizeof(bool);
}

/**
 * TODO: Student Implement
 */
uint32_t Column::DeserializeFrom(char *buf, Column *&column) {
  if(column != nullptr) {
    LOG(WARNING) << "Column is not nullptr, delete it first.\n";
    column = nullptr;
  }
  uint32_t offset = 0;
  uint32_t magic_num =  MACH_READ_UINT32(buf + offset);
  offset += sizeof(uint32_t);
  ASSERT(COLUMN_MAGIC_NUM == magic_num, "Wrong magic number in the Column::DeserializeFrom.\n");
  //Name length
  size_t length = MACH_READ_FROM(size_t, buf+offset);
  offset += sizeof(size_t);
  //Name content
  char *content = new char[length+1];
  memset(content, 0, length+1);
  memcpy(content, buf+offset, length);
  std::string name(content);
  offset += sizeof(char) * length;
  //Type
  TypeId type = MACH_READ_FROM(TypeId, buf + offset);
  offset += sizeof(type);
  //max_length
  uint32_t max_length = MACH_READ_UINT32(buf + offset);
  offset += sizeof(uint32_t);
 //colomn position
  uint32_t position = MACH_READ_UINT32(buf + offset);
  offset += sizeof(uint32_t);
  //nullable flag
  bool nullable = MACH_READ_FROM(bool, buf + offset);
  offset += sizeof(bool);
  //unique flag
  bool unique = MACH_READ_FROM(bool, buf + offset);
  offset += sizeof(bool);
  //Allocate a new column
  if(type == TypeId::kTypeChar) {
    column = new Column(name, type, max_length, position, nullable, unique);
  } else {
    column = new Column(name, type, position, nullable, unique);
  }
  ASSERT(offset == column->GetSerializedSize(), "Wrong serialized size in Column::DeserializeFrom.\n");
  return offset;
}
