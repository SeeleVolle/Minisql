#include "page/b_plus_tree_internal_page.h"

#include "index/generic_key.h"

#define pairs_off (data_ + INTERNAL_PAGE_HEADER_SIZE)
#define pair_size (GetKeySize() + sizeof(page_id_t))
#define key_off 0
#define val_off GetKeySize()

/**
 * TODO: Student Implement
 */
/*****************************************************************************
 * HELPER METHODS AND UTILITIES
 *****************************************************************************/
/*
 * Init method after creating a new internal page
 * Including set page type, set current size, set page id, set parent id and set
 * max page size
 */
void InternalPage::Init(page_id_t page_id, page_id_t parent_id, int key_size, int max_size) {
  this->SetPageId(page_id);
  this->SetParentPageId(parent_id);
  this->SetKeySize(key_size);
  this->SetMaxSize(max_size);
  this->SetPageType(IndexPageType::INTERNAL_PAGE);
  this->SetSize(0);
}
/*
 * Helper method to get/set the key associated with input "index"(a.k.a
 * array offset)
 */
GenericKey *InternalPage::KeyAt(int index) {
  return reinterpret_cast<GenericKey *>(pairs_off + index * pair_size + key_off);
}

void InternalPage::SetKeyAt(int index, GenericKey *key) {
  memcpy(pairs_off + index * pair_size + key_off, key, GetKeySize());
}

page_id_t InternalPage::ValueAt(int index) const {
  return *reinterpret_cast<const page_id_t *>(pairs_off + index * pair_size + val_off);
}

void InternalPage::SetValueAt(int index, page_id_t value) {
  *reinterpret_cast<page_id_t *>(pairs_off + index * pair_size + val_off) = value;
}

int InternalPage::ValueIndex(const page_id_t &value) const {
  for (int i = 0; i < GetSize(); ++i) {
    if (ValueAt(i) == value)
      return i;
  }
  return -1;
}

void *InternalPage::PairPtrAt(int index) {
  return KeyAt(index);
}

void InternalPage::PairCopy(void *dest, void *src, int pair_num) {
  memcpy(dest, src, pair_num * (GetKeySize() + sizeof(page_id_t)));
}
/*****************************************************************************
 * LOOKUP
 *****************************************************************************/
/*
 * Find and return the child pointer(page_id) which points to the child page
 * that contains input "key"
 * Start the search from the second key(the first key should always be invalid)
 * 用了二分查找
 */
page_id_t InternalPage::Lookup(const GenericKey *key, const KeyManager &KM) {
  int left = 1, right = this->GetSize() - 1;
  while(left <= right) {
    int mid = (left + right) / 2;
    if (KM.CompareKeys(key, KeyAt(mid)) < 0) {
      right = mid - 1;
    }
    else if(KM.CompareKeys(key, KeyAt(mid)) > 0) {
      left = mid + 1;
    }
    else {
      return ValueAt(mid);
    }
  }
  return -1; // For not found
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/*
 * Populate new root page with old_value + new_key & new_value
 * When the insertion cause overflow from leaf page all the way up to the root
 * page, you should create a new root page and populate its elements.
 * NOTE: This method is only called within InsertIntoParent()(b_plus_tree.cpp)
 */
void InternalPage::PopulateNewRoot(const page_id_t &old_value, GenericKey *new_key, const page_id_t &new_value) {
  this->SetSize(2);
  this->SetValueAt(0, old_value);
  this->SetKeyAt(1, new_key);
  this->SetValueAt(1, new_value);
}

/*
 * Insert new_key & new_value pair right after the pair with its value ==
 * old_value
 * @return:  new size after insertion
 */
int InternalPage::InsertNodeAfter(const page_id_t &old_value, GenericKey *new_key, const page_id_t &new_value) {
  int index = ValueIndex(old_value);
  for(int i = GetSize(); i > index+1; i--) {
    this->SetValueAt(i , ValueAt(i-1));
    this->SetKeyAt(i, KeyAt(i-1));
  }
  this->SetValueAt(index + 1, new_value);
  this->SetKeyAt(index + 1, new_key);
  this->IncreaseSize(1);
  return this->GetSize();
}

/*****************************************************************************
 * SPLIT
 *****************************************************************************/
/*
 * Remove half of key & value pairs from this page to "recipient" page
 * buffer_pool_manager 是干嘛的？传给CopyNFrom()用于Fetch数据页
 */
void InternalPage::MoveHalfTo(InternalPage *recipient, BufferPoolManager *buffer_pool_manager) {
  int amount = this->GetSize() - this->GetMinSize();
  recipient->CopyNFrom(PairPtrAt(this->GetMinSize()), amount, buffer_pool_manager);
  this->IncreaseSize(-1 * amount);
}

/* Copy entries into me, starting from {items} and copy {size} entries.
 * Since it is an internal page, for all entries (pages) moved, their parents page now changes to me.
 * So I need to 'adopt' them by changing their parent page id, which needs to be persisted with BufferPoolManger
 *
 */
void InternalPage::CopyNFrom(void *src, int size, BufferPoolManager *buffer_pool_manager) {
  int old_size = this->GetSize();
  this->PairCopy(pairs_off + old_size * pair_size, src, size);
  this->IncreaseSize(size);
  for(int i = old_size; i < GetSize(); i++){
    auto *child = reinterpret_cast<BPlusTreePage *>(buffer_pool_manager->FetchPage(ValueAt(i))->GetData());
    ASSERT(child != nullptr, "child is nullptr in the CopyNfrom function\n");
    child->SetParentPageId(this->GetPageId());
    buffer_pool_manager->UnpinPage(child->GetPageId(), true);
  }
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/*
 * Remove the key & value pair in internal page according to input index(a.k.a
 * array offset)
 * NOTE: store key&value pair continuously after deletion
 */
void InternalPage::Remove(int index) {
  for(int i = index; i < GetSize()-1; i++){
    this->SetKeyAt(i, KeyAt(i + 1));
    this->SetValueAt(i, ValueAt(i + 1));
  }
  this->IncreaseSize(-1);
}

/*
 * Remove the only key & value pair in internal page and return the value
 * NOTE: only call this method within AdjustRoot()(in b_plus_tree.cpp)
 */
page_id_t InternalPage::RemoveAndReturnOnlyChild() {
  page_id_t only_value = ValueAt(0);
  this->IncreaseSize(-1);
  return only_value;
}

/*****************************************************************************
 * MERGE
 *****************************************************************************/
/*
 * Remove all key & value pairs from this page to "recipient" page.
 * The middle_key is the separation key you should get from the parent. You need
 * to make sure the middle key is added to the recipient to maintain the invariant.
 * You also need to use BufferPoolManager to persist changes to the parent page id for those
 * pages that are moved to the recipient
 */
//What is the meaning of the middle_key, if it is from the root, then it must be in the child
void InternalPage::MoveAllTo(InternalPage *recipient, GenericKey *middle_key, BufferPoolManager *buffer_pool_manager) {
  recipient->CopyNFrom(PairPtrAt(0), GetSize(), buffer_pool_manager);
//  recipient->SetKeyAt(0, middle_key);
//  for(int i = 0; i < GetSize(); i++){
//    auto *child = reinterpret_cast<BPlusTreePage *>(buffer_pool_manager->FetchPage(ValueAt(i))->GetData());
//    ASSERT(child != nullptr, "child is nullptr in the InternalPage::MoveAllTo function\n");
//    child->SetParentPageId(recipient->GetPageId());
//    buffer_pool_manager->UnpinPage(child->GetPageId(), true);
//  }
  this->SetSize(0);
}

/*****************************************************************************
 * REDISTRIBUTE
 *****************************************************************************/
/*
 * Remove the first key & value pair from this page to tail of "recipient" page.
 *
 * The middle_key is the separation key you should get from the parent. You need
 * to make sure the middle key is added to the recipient to maintain the invariant.
 * You also need to use BufferPoolManager to persist changes to the parent page id for those
 * pages that are moved to the recipient
 */
void InternalPage::MoveFirstToEndOf(InternalPage *recipient, GenericKey *middle_key,
                                    BufferPoolManager *buffer_pool_manager) {
  recipient->CopyLastFrom(KeyAt(0), ValueAt(0), buffer_pool_manager);
  for(int i = 0; i < GetSize()-1; i++){
    this->SetKeyAt(i, KeyAt(i+1));
    this->SetValueAt(i, ValueAt(i+1));
  }
  this->IncreaseSize(-1);
}

/* Append an entry at the end.
 * Since it is an internal page, the moved entry(page)'s parent needs to be updated.
 * So I need to 'adopt' it by changing its parent page id, which needs to be persisted with BufferPoolManger
 */
void InternalPage::CopyLastFrom(GenericKey *key, const page_id_t value, BufferPoolManager *buffer_pool_manager) {
  this->SetKeyAt(GetSize(), key);
  this->SetValueAt(GetSize(), value);
  this->IncreaseSize(1);
  auto *child = reinterpret_cast<BPlusTreePage *>(buffer_pool_manager->FetchPage(ValueAt(GetSize()-1))->GetData());
  child->SetParentPageId(this->GetPageId());
  buffer_pool_manager->UnpinPage(child->GetPageId(), true);

}

/*
 * Remove the last key & value pair from this page to head of "recipient" page.
 * You need to handle the original dummy key properly, e.g. updating recipient’s array to position the middle_key at the
 * right place.
 * You also need to use BufferPoolManager to persist changes to the parent page id for those pages that are
 * moved to the recipient
 */
void InternalPage::MoveLastToFrontOf(InternalPage *recipient, GenericKey *middle_key,
                                     BufferPoolManager *buffer_pool_manager) {
  recipient->CopyFirstFrom(KeyAt(GetSize()-1), ValueAt(GetSize()-1), buffer_pool_manager);
  this->IncreaseSize(-1);
}

/* Append an entry at the beginning.
 * Since it is an internal page, the moved entry(page)'s parent needs to be updated.
 * So I need to 'adopt' it by changing its parent page id, which needs to be persisted with BufferPoolManger
 */
void InternalPage::CopyFirstFrom(GenericKey *key, const page_id_t value, BufferPoolManager *buffer_pool_manager) {
  for(int i=this->GetSize(); i >= 1; i--){
    this->SetKeyAt(i, KeyAt(i-1));
    this->SetValueAt(i, ValueAt(i-1));
  }
  this->SetKeyAt(0, key);
  this->SetValueAt(0, value);
  this->IncreaseSize(1);
  auto *child = reinterpret_cast<BPlusTreePage *>(buffer_pool_manager->FetchPage(ValueAt(0))->GetData());
  child->SetParentPageId(this->GetPageId());
  buffer_pool_manager->UnpinPage(child->GetPageId(), true);
}