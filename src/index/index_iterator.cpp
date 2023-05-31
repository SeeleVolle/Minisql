#include "index/index_iterator.h"

#include "index/basic_comparator.h"
#include "index/generic_key.h"

IndexIterator::IndexIterator() = default;

IndexIterator::IndexIterator(page_id_t page_id, BufferPoolManager *bpm, int index)
    : current_page_id(page_id), item_index(index), buffer_pool_manager(bpm) {
  if(page_id != INVALID_PAGE_ID)
    page = reinterpret_cast<LeafPage *>(buffer_pool_manager->FetchPage(current_page_id)->GetData());
  else
    page = nullptr;
}

IndexIterator::~IndexIterator() {
  if (current_page_id != INVALID_PAGE_ID)
    buffer_pool_manager->UnpinPage(current_page_id, false);
}

std::pair<GenericKey *, RowId> IndexIterator::operator*() {
  return std::pair<GenericKey *, RowId>(page->KeyAt(item_index), page->ValueAt(item_index));
}

IndexIterator &IndexIterator::operator++() {
  if (item_index < page->GetSize() - 1){
    item_index++;
  }
  else{
    page_id_t next_page_id = page->GetNextPageId(),
              old_page_id = current_page_id;
    if(next_page_id == INVALID_PAGE_ID){
      current_page_id = INVALID_PAGE_ID;
      page = nullptr;
      item_index = 0;
    }
    else{
      current_page_id = next_page_id;
      page = reinterpret_cast<LeafPage *>(buffer_pool_manager->FetchPage(current_page_id)->GetData());
    }
    buffer_pool_manager->UnpinPage(old_page_id, true);
  }
  return *this;
}

bool IndexIterator::operator==(const IndexIterator &itr) const {
  return current_page_id == itr.current_page_id && item_index == itr.item_index;
}

bool IndexIterator::operator!=(const IndexIterator &itr) const {
  return !(*this == itr);
}