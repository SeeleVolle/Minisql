#include "record/types.h"

#include "common/macros.h"
#include "record/field.h"

inline int CompareStrings(const char *str1, int len1, const char *str2, int len2) {
  assert(str1 != nullptr);
  assert(len1 >= 0);
  assert(str2 != nullptr);
  assert(len2 >= 0);
  int ret = memcmp(str1, str2, static_cast<size_t>(std::min(len1, len2)));
  if (ret == 0 && len1 != len2) {
    ret = len1 - len2;
  }
  return ret;
}

// ==============================Type=============================

Type *Type::type_singletons_[] = {new Type(TypeId::kTypeInvalid), new TypeInt(), new TypeFloat(), new TypeChar()};

uint32_t Type::SerializeTo(const Field &field, char *buf) const {
  ASSERT(buf != nullptr, "buf is null in Type::SerializeTo");
  ASSERT(type_id_ == field.type_id_, "type_id_ is not equal to field.type_id_ in Type::SerializeTo");
  ASSERT(field.IsNull() == false, "field is null in Type::SerializeTo");
// type_id_ is not in the TypeId, assert false
  if(type_id_ != TypeId::kTypeChar && type_id_ != TypeId::kTypeInt && type_id_ != TypeId::kTypeFloat) {
    ASSERT(false, "type_id_ is not in the TypeId, assert false");
  }
  uint32_t offset = 0;
  offset = this->GetInstance(type_id_)->SerializeTo(field, buf);
  return offset;
}

//DeserializeFrom the data in this type
uint32_t Type::DeserializeFrom(char *storage, Field **field, bool is_null) const {
  ASSERT(storage != nullptr, "storage is null in Type::DeserializeFrom");
  ASSERT(field != nullptr, "field is null in Type::DeserializeFrom");
  uint32_t offset = 0;
  if(type_id_ != TypeId::kTypeChar && type_id_ != TypeId::kTypeInt && type_id_ != TypeId::kTypeFloat) {
    ASSERT(false, "type_id_ is not in the TypeId, assert false");
  }
  offset = this->GetInstance(type_id_)->DeserializeFrom(storage, field, is_null);
  return offset;
}

uint32_t Type::GetSerializedSize(const Field &field, bool is_null) const {
  ASSERT(field.is_null_ == false, "field is null in Type::GetSerializedSize");
  return this->GetInstance(type_id_)->GetSerializedSize(field, is_null);
}

const char *Type::GetData(const Field &val) const {
  ASSERT(val.type_id_ == type_id_, "val.type_id_ is not equal to type_id_ in Type::GetData");
  switch(type_id_){
    case TypeId::kTypeInt:{
      int32_t int_val = val.value_.integer_;
      //memcpy ok ?
      std::string str_int = std::to_string(int_val);
      return str_int.c_str();
    }
    case TypeId::kTypeFloat:{
      float float_val = val.value_.float_;
      std::string str_float = std::to_string(float_val);
      return str_float.c_str();
    }
    case TypeId::kTypeChar:{
      return val.value_.chars_;
    }
    default:
      ASSERT(false, "type_id_ is not in the TypeId, assert false");
  }
}

uint32_t Type::GetLength(const Field &val) const {
  ASSERT(val.type_id_ == type_id_, "val.type_id_ is not equal to type_id_ in Type::GetData");
  return this->GetInstance(val.type_id_)->GetLength(val);
}

CmpBool Type::CompareEquals(const Field &left, const Field &right) const {
  return this->GetInstance(type_id_)->CompareEquals(left, right);
}

CmpBool Type::CompareNotEquals(const Field &left, const Field &right) const {
  return this->GetInstance(type_id_)->CompareNotEquals(left, right);
}

CmpBool Type::CompareLessThan(const Field &left, const Field &right) const {
  return this->GetInstance(type_id_)->CompareLessThan(left, right);
}

CmpBool Type::CompareLessThanEquals(const Field &left, const Field &right) const {
  return this->GetInstance(type_id_)->CompareLessThanEquals(left, right);
}

CmpBool Type::CompareGreaterThan(const Field &left, const Field &right) const {
  return this->GetInstance(type_id_)->CompareGreaterThan(left, right);
}

CmpBool Type::CompareGreaterThanEquals(const Field &left, const Field &right) const {
  return this->GetInstance(type_id_)->CompareGreaterThanEquals(left, right);
}

// ==============================TypeInt=================================

uint32_t TypeInt::SerializeTo(const Field &field, char *buf) const {
  if (!field.IsNull()) {
    MACH_WRITE_TO(int32_t, buf, field.value_.integer_);
    return GetTypeSize(type_id_);
  }
  return 0;
}

uint32_t TypeInt::DeserializeFrom(char *storage, Field **field, bool is_null) const {
  if (is_null) {
    *field = new Field(TypeId::kTypeInt);
    return 0;
  }
  int32_t val = MACH_READ_FROM(int32_t, storage);
  *field = new Field(TypeId::kTypeInt, val);
  return GetTypeSize(type_id_);
}

uint32_t TypeInt::GetSerializedSize(const Field &field, bool is_null) const {
  if (is_null) {
    return 0;
  }
  return GetTypeSize(type_id_);
}

CmpBool TypeInt::CompareEquals(const Field &left, const Field &right) const {
  ASSERT(left.CheckComparable(right), "Not comparable.");
  if (left.IsNull() || right.IsNull()) {
    return CmpBool::kNull;
  }
  return GetCmpBool(left.value_.integer_ == right.value_.integer_);
}

CmpBool TypeInt::CompareNotEquals(const Field &left, const Field &right) const {
  ASSERT(left.CheckComparable(right), "Not comparable.");
  if (left.IsNull() || right.IsNull()) {
    return CmpBool::kNull;
  }
  return GetCmpBool(left.value_.integer_ != right.value_.integer_);
}

CmpBool TypeInt::CompareLessThan(const Field &left, const Field &right) const {
  ASSERT(left.CheckComparable(right), "Not comparable.");
  if (left.IsNull() || right.IsNull()) {
    return CmpBool::kNull;
  }
  return GetCmpBool(left.value_.integer_ < right.value_.integer_);
}

CmpBool TypeInt::CompareLessThanEquals(const Field &left, const Field &right) const {
  ASSERT(left.CheckComparable(right), "Not comparable.");
  if (left.IsNull() || right.IsNull()) {
    return CmpBool::kNull;
  }
  return GetCmpBool(left.value_.integer_ <= right.value_.integer_);
}

CmpBool TypeInt::CompareGreaterThan(const Field &left, const Field &right) const {
  ASSERT(left.CheckComparable(right), "Not comparable.");
  if (left.IsNull() || right.IsNull()) {
    return CmpBool::kNull;
  }
  return GetCmpBool(left.value_.integer_ > right.value_.integer_);
}

CmpBool TypeInt::CompareGreaterThanEquals(const Field &left, const Field &right) const {
  ASSERT(left.CheckComparable(right), "Not comparable.");
  if (left.IsNull() || right.IsNull()) {
    return CmpBool::kNull;
  }
  return GetCmpBool(left.value_.integer_ >= right.value_.integer_);
}

// ==============================TypeFloat=============================

uint32_t TypeFloat::SerializeTo(const Field &field, char *buf) const {
  if (!field.IsNull()) {
    MACH_WRITE_TO(float_t, buf, field.value_.float_);
    return GetTypeSize(type_id_);
  }
  return 0;
}

uint32_t TypeFloat::DeserializeFrom(char *storage, Field **field, bool is_null) const {
  if (is_null) {
    *field = new Field(TypeId::kTypeFloat);
    return 0;
  }
  float_t val = MACH_READ_FROM(float_t, storage);
  *field = new Field(TypeId::kTypeFloat, val);
  return GetTypeSize(type_id_);
}

uint32_t TypeFloat::GetSerializedSize(const Field &field, bool is_null) const {
  if (is_null) {
    return 0;
  }
  return GetTypeSize(type_id_);
}

CmpBool TypeFloat::CompareEquals(const Field &left, const Field &right) const {
  ASSERT(left.CheckComparable(right), "Not comparable.");
  if (left.IsNull() || right.IsNull()) {
    return CmpBool::kNull;
  }
  return GetCmpBool(left.value_.float_ == right.value_.float_);
}

CmpBool TypeFloat::CompareNotEquals(const Field &left, const Field &right) const {
  ASSERT(left.CheckComparable(right), "Not comparable.");
  if (left.IsNull() || right.IsNull()) {
    return CmpBool::kNull;
  }
  return GetCmpBool(left.value_.float_ != right.value_.float_);
}

CmpBool TypeFloat::CompareLessThan(const Field &left, const Field &right) const {
  ASSERT(left.CheckComparable(right), "Not comparable.");
  if (left.IsNull() || right.IsNull()) {
    return CmpBool::kNull;
  }
  return GetCmpBool(left.value_.float_ < right.value_.float_);
}

CmpBool TypeFloat::CompareLessThanEquals(const Field &left, const Field &right) const {
  ASSERT(left.CheckComparable(right), "Not comparable.");
  if (left.IsNull() || right.IsNull()) {
    return CmpBool::kNull;
  }
  return GetCmpBool(left.value_.float_ <= right.value_.float_);
}

CmpBool TypeFloat::CompareGreaterThan(const Field &left, const Field &right) const {
  ASSERT(left.CheckComparable(right), "Not comparable.");
  if (left.IsNull() || right.IsNull()) {
    return CmpBool::kNull;
  }
  return GetCmpBool(left.value_.float_ > right.value_.float_);
}

CmpBool TypeFloat::CompareGreaterThanEquals(const Field &left, const Field &right) const {
  ASSERT(left.CheckComparable(right), "Not comparable.");
  if (left.IsNull() || right.IsNull()) {
    return CmpBool::kNull;
  }
  return GetCmpBool(left.value_.float_ >= right.value_.float_);
}

// ==============================TypeChar=============================
uint32_t TypeChar::SerializeTo(const Field &field, char *buf) const {
  if (!field.IsNull()) {
    uint32_t len = GetLength(field);
    memcpy(buf, &len, sizeof(uint32_t));
    memcpy(buf + sizeof(uint32_t), field.value_.chars_, len);
    return len + sizeof(uint32_t);
  }
  return 0;
}

uint32_t TypeChar::DeserializeFrom(char *storage, Field **field, bool is_null) const {
  if (is_null) {
    *field = new Field(TypeId::kTypeChar);
    return 0;
  }
  uint32_t len = MACH_READ_UINT32(storage);
  *field = new Field(TypeId::kTypeChar, storage + sizeof(uint32_t), len, true);
  return len + sizeof(uint32_t);
}

uint32_t TypeChar::GetSerializedSize(const Field &field, bool is_null) const {
  if (is_null) {
    return 0;
  }
  uint32_t len = GetLength(field);
  return len + sizeof(uint32_t);
}

const char *TypeChar::GetData(const Field &val) const {
  return val.value_.chars_;
}

uint32_t TypeChar::GetLength(const Field &val) const {
  return val.len_;
}

CmpBool TypeChar::CompareEquals(const Field &left, const Field &right) const {
  ASSERT(left.CheckComparable(right), "Not comparable.");
  if (left.IsNull() || right.IsNull()) {
    return CmpBool::kNull;
  }
  return GetCmpBool(CompareStrings(left.GetData(), left.GetLength(), right.GetData(), right.GetLength()) == 0);
}

CmpBool TypeChar::CompareNotEquals(const Field &left, const Field &right) const {
  ASSERT(left.CheckComparable(right), "Not comparable.");
  if (left.IsNull() || right.IsNull()) {
    return CmpBool::kNull;
  }
  return GetCmpBool(CompareStrings(left.GetData(), left.GetLength(), right.GetData(), right.GetLength()) != 0);
}

CmpBool TypeChar::CompareLessThan(const Field &left, const Field &right) const {
  ASSERT(left.CheckComparable(right), "Not comparable.");
  if (left.IsNull() || right.IsNull()) {
    return CmpBool::kNull;
  }
  return GetCmpBool(CompareStrings(left.GetData(), left.GetLength(), right.GetData(), right.GetLength()) < 0);
}

CmpBool TypeChar::CompareLessThanEquals(const Field &left, const Field &right) const {
  ASSERT(left.CheckComparable(right), "Not comparable.");
  if (left.IsNull() || right.IsNull()) {
    return CmpBool::kNull;
  }
  return GetCmpBool(CompareStrings(left.GetData(), left.GetLength(), right.GetData(), right.GetLength()) <= 0);
}

CmpBool TypeChar::CompareGreaterThan(const Field &left, const Field &right) const {
  ASSERT(left.CheckComparable(right), "Not comparable.");
  if (left.IsNull() || right.IsNull()) {
    return CmpBool::kNull;
  }
  return GetCmpBool(CompareStrings(left.GetData(), left.GetLength(), right.GetData(), right.GetLength()) > 0);
}

CmpBool TypeChar::CompareGreaterThanEquals(const Field &left, const Field &right) const {
  ASSERT(left.CheckComparable(right), "Not comparable.");
  if (left.IsNull() || right.IsNull()) {
    return CmpBool::kNull;
  }
  return GetCmpBool(CompareStrings(left.GetData(), left.GetLength(), right.GetData(), right.GetLength()) >= 0);
}
