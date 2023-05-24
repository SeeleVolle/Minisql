#include "buffer/buffer_pool_manager.h"

#include "glog/logging.h"
#include "page/bitmap_page.h"

static const char EMPTY_PAGE_DATA[PAGE_SIZE] = {0};

BufferPoolManager::BufferPoolManager(size_t pool_size, DiskManager *disk_manager)
    : pool_size_(pool_size), disk_manager_(disk_manager) {
  pages_ = new Page[pool_size_];
  replacer_ = new LRUReplacer(pool_size_);
  for (size_t i = 0; i < pool_size_; i++) {
    free_list_.emplace_back(i);
  }
}

BufferPoolManager::~BufferPoolManager() {
  for (auto page : page_table_) {
    FlushPage(page.first);
  }
  delete[] pages_;
  delete replacer_;
}

/**
 * TODO: Student Implement
 */
Page *BufferPoolManager::FetchPage(page_id_t page_id) {
  // 1.     Search the page table for the requested page (P).
  // 1.1    If P exists, pin it and return it immediately.
  // 1.2    If P does not exist, find a replacement page (R) from either the free list or the replacer.
  //        Note that pages are always found from the free list first.
  // 2.     If R is dirty, write it back to the disk.
  // 3.     Delete R from the page table and insert P.
  // 4.     Update P's metadata, read in the page content from disk, and then return a pointer to P.

  // 1.1    If P exists, pin it and return it immediately.
  if(page_table_.find(page_id) != page_table_.end()){
    pages_[page_table_[page_id]].pin_count_++;
    replacer_->Pin(page_table_[page_id]);
    return pages_+page_table_[page_id];
  }
  frame_id_t new_frame_id = -1;
  // 1.2    If P does not exist, find a replacement page (R) from either the free list or the replacer.
  //        Note that pages are always found from the free list first.
  if(!free_list_.empty()){
    new_frame_id = free_list_.front();
    free_list_.pop_front();
  }
  else if(replacer_->Victim(&new_frame_id) == false){
    return nullptr;
    LOG(ERROR) << "No available page in the buffer_pool" <<std::endl;
  }
  // 2.     If R is dirty, write it back to the disk.
  if(pages_[new_frame_id].IsDirty())
    FlushPage(pages_[new_frame_id].GetPageId());
  //// 3.     Delete R from the page table and insert P.
  page_table_.erase(pages_[new_frame_id].GetPageId());
  page_table_.insert(std::pair<page_id_t, frame_id_t>(page_id, new_frame_id));

  // 4.     Update P's metadata, read in the page content from disk, and then return a pointer to P.
  pages_[new_frame_id].ResetMemory();
  pages_[new_frame_id].page_id_ = page_id;
  pages_[new_frame_id].is_dirty_ = false;
  pages_[new_frame_id].pin_count_ = 1;
  replacer_->Pin(new_frame_id);
  //Update P's metadata, read in the page content from disk, and then return a pointer to P.
  disk_manager_->ReadPage(page_id, pages_[new_frame_id].GetData());

  return pages_+new_frame_id;
}

/**
 * TODO: Student Implement
 */
Page *BufferPoolManager::NewPage(page_id_t &page_id) {
  // 0.   Make sure you call AllocatePage!
  // 1.   If all the pages in the buffer pool are pinned, return nullptr.
  // 2.   Pick a victim page P from either the free list or the replacer. Always pick from the free list first.
  // 3.   Update P's metadata, zero out memory and add P to the page table.
  // 4.   Set the page ID output parameter. Return a pointer to P.

  // 1.   If all the pages in the buffer pool are pinned, return nullptr.
  if(CheckAllPinned())
    return nullptr;
  // 2.   Pick a victim page P from either the free list or the replacer. Always pick from the free list first.
  page_id = this->AllocatePage();
  frame_id_t new_frame_id = -1;
  if(!free_list_.empty()){
    new_frame_id = free_list_.front();
    free_list_.pop_front();
  }
  else if(replacer_->Victim(&new_frame_id) == false){
    return nullptr;
    LOG(ERROR) << "NO available page in the free_list and replacer" << std::endl;
  }
  // 3.   Update P's metadata, zero out memory and add P to the page table.
  pages_[new_frame_id].ResetMemory();
  pages_[new_frame_id].pin_count_ = 1; //The pin count for the new page is 1, for the creating processes
  pages_[new_frame_id].is_dirty_ = false;
  page_table_.insert(std::pair<page_id_t, frame_id_t>(page_id, new_frame_id));
  // 4.   Set the page ID output parameter. Return a pointer to P.
  pages_[new_frame_id].page_id_ = page_id;

  return pages_ + new_frame_id;
}

/**
 * TODO: Student Implement
 */
bool BufferPoolManager::DeletePage(page_id_t page_id) {
  // 0.   Make sure you call DeallocatePage!
  // 1.   Search the page table for the requested page (P).
  // 1.   If P does not exist, return true.
  // 2.   If P exists, but has a non-zero pin-count, return false. Someone is using the page.
  // 3.   Otherwise, P can be deleted. Remove P from the page table, reset its metadata and return it to the free list.

  //1.   Search the page table for the requested page (P). Return means that it has been deleted
  if(page_table_.find(page_id) == page_table_.end())
    return true;
  //2.   If P exists, but has a non-zero pin-count, return false. Someone is using the page.
  if(pages_[page_table_[page_id]].GetPinCount() != 0)
    return false;
  //3.   Otherwise, P can be deleted. Remove P from the page table, reset its metadata and return it to the free list.
  DeallocatePage(page_id);
  page_table_.erase(page_id);
  pages_[page_table_[page_id]].ResetMemory();
  pages_[page_table_[page_id]].page_id_ = INVALID_PAGE_ID;
  pages_[page_table_[page_id]].is_dirty_ = false;
  pages_[page_table_[page_id]].pin_count_ = 0;
  free_list_.push_back(page_table_[page_id]);
  return true;
}

/**
 * TODO: Student Implement
 */
bool BufferPoolManager::UnpinPage(page_id_t page_id, bool is_dirty) {
  //1. Search the page_table_ to check whether the page is in the buffer pool.
  if(page_table_.find(page_id) == page_table_.end())
    return false;
  //2. Check whether the page is pinned or not. If it is pinned, it can't be unpinned again.
  if(pages_[page_table_[page_id]].GetPinCount() == 0)
    return false;
  //3. If the page is pinned, unpin it and update the metadata.
  pages_[page_table_[page_id]].pin_count_--;
  pages_[page_table_[page_id]].is_dirty_ = is_dirty;
  //4. Check whether its pin number is 0 after unpinning.
  if(pages_[page_table_[page_id]].GetPinCount() == 0)
    replacer_->Unpin(page_table_[page_id]);
  return true;
}

/**
 * TODO: Student Implement
 */
bool BufferPoolManager::FlushPage(page_id_t page_id) {
  //1. Check whether the page is in the buffer pool.
  if(page_table_.find(page_id) == page_table_.end())
    return false;
  //2. Write the page into the disk
  disk_manager_->WritePage(page_id, pages_[page_table_[page_id]].GetData());
  return true;
}

page_id_t BufferPoolManager::AllocatePage() {
  int next_page_id = disk_manager_->AllocatePage();
  return next_page_id;
}

void BufferPoolManager::DeallocatePage(__attribute__((unused)) page_id_t page_id) {
  disk_manager_->DeAllocatePage(page_id);
}

bool BufferPoolManager::IsPageFree(page_id_t page_id) {
  return disk_manager_->IsPageFree(page_id);
}

// Only used for debug
bool BufferPoolManager::CheckAllUnpinned() {
  bool res = true;
  for (size_t i = 0; i < pool_size_; i++) {
    if (pages_[i].pin_count_ != 0) {
      res = false;
      LOG(ERROR) << "page " << pages_[i].page_id_ << " pin count:" << pages_[i].pin_count_ << endl;
    }
  }
  return res;
}

bool BufferPoolManager::CheckAllPinned() {
  bool res = true;
  for (size_t i =0; i < pool_size_; i++) {
    if(pages_[i].pin_count_ == 0){
      res = false;
      break;
      LOG(ERROR) << "page " << pages_[i].page_id_ << " is unpinned" <<endl;
    }
  }
  return res;
}