#include "storage/disk_manager.h"

#include <sys/stat.h>
#include <filesystem>
#include <stdexcept>

#include "glog/logging.h"
#include "page/bitmap_page.h"

const int FIRST_BITMAP_INDEX = 1;

DiskManager::DiskManager(const std::string &db_file) : file_name_(db_file) {
  std::scoped_lock<std::recursive_mutex> lock(db_io_latch_);
  db_io_.open(db_file, std::ios::binary | std::ios::in | std::ios::out);
  // directory or file does not exist
  if (!db_io_.is_open()) {
    db_io_.clear();
    // create a new file
    std::filesystem::path p = db_file;
    if(p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
    db_io_.open(db_file, std::ios::binary | std::ios::trunc | std::ios::out);
    db_io_.close();
    // reopen with original mode
    db_io_.open(db_file, std::ios::binary | std::ios::in | std::ios::out);
    if (!db_io_.is_open()) {
      throw std::exception();
    }
  }
  ReadPhysicalPage(META_PAGE_ID, meta_data_);
}

void DiskManager::Close() {
  std::scoped_lock<std::recursive_mutex> lock(db_io_latch_);
  if (!closed) {
    db_io_.close();
    closed = true;
  }
}

void DiskManager::ReadPage(page_id_t logical_page_id, char *page_data) {
  ASSERT(logical_page_id >= 0, "Invalid page id.");
  ReadPhysicalPage(MapPageId(logical_page_id), page_data);
}

void DiskManager::WritePage(page_id_t logical_page_id, const char *page_data) {
  ASSERT(logical_page_id >= 0, "Invalid page id.");
  WritePhysicalPage(MapPageId(logical_page_id), page_data);
}

/**
 * TODO: Student Implement
 */
page_id_t DiskManager::AllocatePage() {
  size_t bitmap_page_index = 0;
  char bitmap_data[PAGE_SIZE];
  memset(bitmap_data,0,PAGE_SIZE * sizeof(char));
  DiskFileMetaPage* meta_page = reinterpret_cast<DiskFileMetaPage*>(this->GetMetaData());
  while(1){
    //Read the bitmap_data for the bitmap_page, note 0 for the metapage
    //1...1001 1002...2002 2003...3003 3004...4004
    size_t log = FIRST_BITMAP_INDEX + bitmap_page_index*(BitmapPage<PAGE_SIZE>::GetMaxSupportedSize() + 1);
    ReadPhysicalPage(FIRST_BITMAP_INDEX + bitmap_page_index*(BitmapPage<PAGE_SIZE>::GetMaxSupportedSize() + 1) ,bitmap_data);
    BitmapPage<PAGE_SIZE> bitmap_page(bitmap_data);
    //We need to allocate page in another bitmap_page
    if(bitmap_page.IsFull()){
      bitmap_page_index++;
    }
    else{
      if(bitmap_page.IsEmpty()){
        meta_page->num_extents_++;
      }
      meta_page->extent_used_page_[bitmap_page_index]++;
      meta_page->num_allocated_pages_++;
      uint32_t index;
      //Allocate a page
      bitmap_page.AllocatePage(index);
      //Write the bitmap_page back
      WritePhysicalPage(FIRST_BITMAP_INDEX + bitmap_page_index*(BitmapPage<PAGE_SIZE>::GetMaxSupportedSize() + 1), reinterpret_cast<char *>(&bitmap_page));

      return index + BitmapPage<PAGE_SIZE>::GetMaxSupportedSize() * bitmap_page_index;
    }
  }
}

/**
 * TODO: Student Implement
 */
void DiskManager::DeAllocatePage(page_id_t logical_page_id) {
  char bitmap_data[PAGE_SIZE];
  memset(bitmap_data, 0, PAGE_SIZE);
  size_t page_index = logical_page_id % BitmapPage<PAGE_SIZE>::GetMaxSupportedSize();
  size_t bitmap_page_index = logical_page_id / BitmapPage<PAGE_SIZE>::GetMaxSupportedSize();
  ReadPhysicalPage(FIRST_BITMAP_INDEX + bitmap_page_index*(BitmapPage<PAGE_SIZE>::GetMaxSupportedSize() + 1),bitmap_data);
  BitmapPage<PAGE_SIZE> bitmap_page(bitmap_data);
  bitmap_page.DeAllocatePage(page_index);

  DiskFileMetaPage * meta_page = reinterpret_cast<DiskFileMetaPage *>(this->GetMetaData());
  //If the bitmap_page is empty after delete the page, the extent should decrease by 1
  if(bitmap_page.IsEmpty())
  {
    meta_page->num_extents_--;
  }
  meta_page->num_allocated_pages_--;
  meta_page->extent_used_page_[bitmap_page_index]--;
  WritePhysicalPage(FIRST_BITMAP_INDEX + bitmap_page_index*(BitmapPage<PAGE_SIZE>::GetMaxSupportedSize() + 1),reinterpret_cast<char *>(&bitmap_page));
}

/**
 * TODO: Student Implement
 */
bool DiskManager::IsPageFree(page_id_t logical_page_id) {
  char bitmap_data[PAGE_SIZE];
  size_t page_index = logical_page_id % BitmapPage<PAGE_SIZE>::GetMaxSupportedSize();
  size_t bitmap_page_index = logical_page_id / BitmapPage<PAGE_SIZE>::GetMaxSupportedSize();
  ReadPhysicalPage(FIRST_BITMAP_INDEX + bitmap_page_index*(BitmapPage<PAGE_SIZE>::GetMaxSupportedSize() + 1) ,bitmap_data);
  BitmapPage<PAGE_SIZE> bitmap_page(bitmap_data);
  if(bitmap_page.IsPageFree(page_index))
    return true;
  return false;
}

/**
 * TODO: Student Implement
 */
page_id_t DiskManager::MapPageId(page_id_t logical_page_id) {
  return logical_page_id + 2 + logical_page_id / BITMAP_SIZE;
}

int DiskManager::GetFileSize(const std::string &file_name) {
  struct stat stat_buf;
  int rc = stat(file_name.c_str(), &stat_buf);
  return rc == 0 ? stat_buf.st_size : -1;
}

void DiskManager::ReadPhysicalPage(page_id_t physical_page_id, char *page_data) {
  int offset = physical_page_id * PAGE_SIZE;
  // check if read beyond file length
  if (offset >= GetFileSize(file_name_)) {
#ifdef ENABLE_BPM_DEBUG
    LOG(INFO) << "Read less than a page" << std::endl;
#endif
    memset(page_data, 0, PAGE_SIZE);
  } else {
    // set read cursor to offset
    db_io_.seekp(offset);
    db_io_.read(page_data, PAGE_SIZE);
    // if file ends before reading PAGE_SIZE
    int read_count = db_io_.gcount();
    if (read_count < PAGE_SIZE) {
#ifdef ENABLE_BPM_DEBUG
      LOG(INFO) << "Read less than a page" << std::endl;
#endif
      memset(page_data + read_count, 0, PAGE_SIZE - read_count);
    }
  }
}

void DiskManager::WritePhysicalPage(page_id_t physical_page_id, const char *page_data) {
  size_t offset = static_cast<size_t>(physical_page_id) * PAGE_SIZE;
  // set write cursor to offset
  db_io_.seekp(offset);
  db_io_.write(page_data, PAGE_SIZE);
  // check for I/O error
  if (db_io_.bad()) {
    LOG(ERROR) << "I/O error while writing";
    return;
  }
  // needs to flush to keep disk file in sync
  db_io_.flush();
}