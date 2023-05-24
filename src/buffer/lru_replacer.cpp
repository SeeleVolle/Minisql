#include "buffer/lru_replacer.h"

LRUReplacer::LRUReplacer(size_t num_pages){
  this->max_pages = num_pages;
  this->LRU_list.clear();
  this->LRU_map.clear();
}

LRUReplacer::~LRUReplacer() = default;

/**
 * TODO: Student Implement
 */
bool LRUReplacer::Victim(frame_id_t *frame_id) {
  if(LRU_list.size() == 0)
    return false;
  (*frame_id) = this->LRU_list.back();
  LRU_list.pop_back();
  LRU_map.erase((*frame_id));
  return true;
}

/**
 * TODO: Student Implement
 */
void LRUReplacer::Pin(frame_id_t frame_id) {
   if(this->LRU_map.find(frame_id) != this->LRU_map.end()){
      this->LRU_list.remove(frame_id);
      this->LRU_map.erase(frame_id);
    }
}

/**
 * TODO: Student Implement
 */
void LRUReplacer::Unpin(frame_id_t frame_id) {
  if(this->LRU_map.find(frame_id) == this->LRU_map.end()){
    this->LRU_list.push_front(frame_id);
    this->LRU_map.insert(frame_id);
  }
}

/**
 * TODO: Student Implement
 */
size_t LRUReplacer::Size() {
  return this->LRU_list.size();
}

size_t LRUReplacer::Get_max_Size() {
  return this->max_pages;
}