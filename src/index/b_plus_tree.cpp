#include "index/b_plus_tree.h"

#include <string>

#include "glog/logging.h"
#include "index/basic_comparator.h"
#include "index/generic_key.h"
#include "page/index_roots_page.h"
#include "utils/tree_file_mgr.h"

#pragma clang diagnostic push
#pragma ide diagnostic ignored "UnreachableCode"
/**
 * TODO: Student Implement
 */

//For debug
static TreeFileManagers mgr("tree_2");

BPlusTree::BPlusTree(index_id_t index_id, BufferPoolManager *buffer_pool_manager, const KeyManager &KM,
                     int leaf_max_size, int internal_max_size)
    : index_id_(index_id),
      buffer_pool_manager_(buffer_pool_manager),
      processor_(KM),
      leaf_max_size_(leaf_max_size),
      internal_max_size_(internal_max_size) {
}

void BPlusTree::Destroy(page_id_t current_page_id) {
}

/*
 * Helper function to decide whether current b+tree is empty
 */
bool BPlusTree::IsEmpty() const {
  return root_page_id_ == INVALID_PAGE_ID;
}

/*****************************************************************************
 * SEARCH
 *****************************************************************************/
/*
 * Return the only value that associated with input key
 * This method is used for point query
 * @return : true means key exists
 */
bool BPlusTree::GetValue(const GenericKey *key, std::vector<RowId> &result, Transaction *transaction) {
  Page* page = FindLeafPage(key, INVALID_PAGE_ID, false);
  if(page == NULL){
    LOG(WARNING) <<"Failed to find leaf page in the BPLusTree::GetValue" << std::endl;
    return false;
  }
  LeafPage* leaf_page = reinterpret_cast<LeafPage *>(page->GetData());
  RowId rid;
  if(leaf_page->Lookup(key, rid, processor_)){
    result.push_back(rid);
//    LOG(WARNING) <<"The value exists in the leaf_page in the BPLusTree::GetValue" << std::endl;
    buffer_pool_manager_->UnpinPage(page->GetPageId(), true);
    return true;
  }
  buffer_pool_manager_->UnpinPage(page->GetPageId(), true);
  return false;
}
/******************************************************************************
 * INSERTION
 *****************************************************************************/
/*
 * Insert constant key & value pair into b+ tree
 * if current tree is empty, start new tree, update root page id and insert
 * entry, otherwise insert into leaf page.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */

bool BPlusTree::Insert(GenericKey *key, const RowId &value, Transaction *transaction) {
  std::vector<RowId> result;
  //If the B+ tree is empty
  if(IsEmpty()){
    StartNewTree(key, value);
    return true;
  }
  //The key is already in the B plus tree
  else if(GetValue(key, result, transaction) == true){
    return false;
  }
  return InsertIntoLeaf(key, value);
}

/*
 * Insert constant key & value pair into an empty tree
 * User needs to first ask for new page from buffer pool manager(NOTICE: throw
 * an "out of memory" exception if returned value is nullptr), then update b+
 * tree's root page id and insert entry directly into leaf page.
 */
void BPlusTree::StartNewTree(GenericKey *key, const RowId &value) {
  Page* page = buffer_pool_manager_->NewPage(root_page_id_);
  if(page == nullptr){
    throw std::bad_alloc();
  }
  UpdateRootPageId(false);
  LeafPage* leaf_page = reinterpret_cast<LeafPage *>(page->GetData());
  leaf_page->Init(root_page_id_, INVALID_PAGE_ID, processor_.GetKeySize(), leaf_max_size_);
  leaf_page->Insert(key, value, processor_);
  buffer_pool_manager_->UnpinPage(root_page_id_, true);
}

/*
 * Insert constant key & value pair into leaf page
 * User needs to first find the right leaf page as insertion target, then look
 * through leaf page to see whether insert key exist or not. If exist, return
 * immediately, otherwise insert entry. Remember to deal with split if necessary.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
//Cant't understand why
bool BPlusTree::InsertIntoLeaf(GenericKey *key, const RowId &value, Transaction *transaction) {
  Page *page = FindLeafPage(key, INVALID_PAGE_ID, false);
  LeafPage *leaf_page = reinterpret_cast<LeafPage *>(page->GetData());
  //Check if the key is already in the leaf page
  RowId rid;
  if(leaf_page->Lookup(key, rid, processor_)){
    return false;
  }
  //Check if the leaf_node is full
  else{
    if(leaf_page->GetMaxSize() == leaf_page->GetSize()+1){
      leaf_page->Insert(key, value, processor_);
      LeafPage *new_leaf_page = Split(leaf_page, transaction);
//      new_leaf_page->Insert(key, value, processor_);
//      leaf_page->SetNextPageId(new_leaf_page->GetPageId());
//      new_leaf_page->SetParentPageId(leaf_page->GetParentPageId());
      InsertIntoParent(leaf_page, new_leaf_page->KeyAt(0), new_leaf_page, transaction);
      buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), true);
      buffer_pool_manager_->UnpinPage(new_leaf_page->GetPageId(), true);
    }
    else{
      leaf_page->Insert(key, value, processor_);
      buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), true);
    }
  }

}

/*
 * Split input page and return newly created page.
 * Using template N to represent either internal page or leaf page.
 * User needs to first ask for new page from buffer pool manager(NOTICE: throw
 * an "out of memory" exception if returned value is nullptr), then move half
 * of key & value pairs from input page to newly created page
 */
BPlusTreeInternalPage *BPlusTree::Split(InternalPage *node, Transaction *transaction) {
  page_id_t page_id;
  Page * new_page = buffer_pool_manager_->NewPage(page_id);
  if(new_page == nullptr){
    throw std::bad_alloc();
  }
  InternalPage * new_internal = reinterpret_cast<InternalPage *>(new_page->GetData());
  new_internal->Init(page_id, node->GetParentPageId(), processor_.GetKeySize(), internal_max_size_);
//  TreeFileManagers mgr("tree_2");
//  this->PrintTree(mgr[0]);
  node->MoveHalfTo(new_internal, buffer_pool_manager_);
  new_internal->SetParentPageId(node->GetParentPageId());

  return new_internal;
}

BPlusTreeLeafPage *BPlusTree::Split(LeafPage *node, Transaction *transaction) {
  page_id_t page_id;
  Page * new_page = buffer_pool_manager_->NewPage(page_id);
  if(new_page == nullptr){
    throw std::bad_alloc();
  }
  LeafPage * new_leaf = reinterpret_cast<LeafPage *>(new_page->GetData());
  new_leaf->Init(page_id, node->GetParentPageId(), processor_.GetKeySize(), leaf_max_size_);
  node->MoveHalfTo(new_leaf);

  new_leaf->SetNextPageId(node->GetNextPageId());
  node->SetNextPageId(page_id);
  new_leaf->SetParentPageId(node->GetParentPageId());

  return new_leaf;
}

/*
 * Insert key & value pair into internal page after split
 * @param   old_node      input page from split() method
 * @param   key
 * @param   new_node      returned page from split() method
 * User needs to first find the parent page of old_node, parent node must be
 * adjusted to take info of new_node into account. Remember to deal with split
 * recursively if necessary.
 */
void BPlusTree::InsertIntoParent(BPlusTreePage *old_node, GenericKey *key, BPlusTreePage *new_node,
                                 Transaction *transaction) {
  //If the old_node doesn't have the parent page, then it is the root page
  if(old_node->IsRootPage()){
    page_id_t new_root_id;
    InternalPage * new_root_page = reinterpret_cast<InternalPage *>(buffer_pool_manager_->NewPage(new_root_id)->GetData());
    new_root_page->Init(new_root_id, INVALID_PAGE_ID, processor_.GetKeySize(), internal_max_size_);
    this->root_page_id_ = new_root_id;
    UpdateRootPageId(true);
    new_root_page->PopulateNewRoot(old_node->GetPageId(), key, new_node->GetPageId());
    old_node->SetParentPageId(new_root_id);
    new_node->SetParentPageId(new_root_id);
    buffer_pool_manager_->UnpinPage(new_root_id, true);
    return;
  }
  //For normal Internal nodes need split
  InternalPage * parent_page = reinterpret_cast<InternalPage *>(buffer_pool_manager_->FetchPage(old_node->GetParentPageId())->GetData());
  if(parent_page->GetMaxSize() == parent_page->GetSize()+1){
    parent_page->InsertNodeAfter(old_node->GetPageId(), key, new_node->GetPageId());
    new_node->SetParentPageId(old_node->GetParentPageId());

    InternalPage * new_parent_page = Split(parent_page, transaction);
//    new_parent_page->SetParentPageId(parent_page->GetParentPageId());
    //Recursively insert into parent
    InsertIntoParent(parent_page, new_parent_page->KeyAt(0), new_parent_page, transaction);
//    new_parent_page->SetParentPageId(parent_page->GetParentPageId());

    buffer_pool_manager_->UnpinPage(parent_page->GetPageId(), true);
    buffer_pool_manager_->UnpinPage(new_parent_page->GetPageId(), true);
  }
  //Need not to split
  else{
    parent_page->InsertNodeAfter(old_node->GetPageId(), key, new_node->GetPageId());
    new_node->SetParentPageId(old_node->GetParentPageId());
    buffer_pool_manager_->UnpinPage(parent_page->GetPageId(), true);
  }
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/*
 * Delete key & value pair associated with input key
 * If current tree is empty, return immediately.
 * If not, User needs to first find the right leaf page as deletion target, then
 * delete entry from leaf page. Remember to deal with redistribute or merge if
 * necessary.
 */
void BPlusTree::Remove(const GenericKey *key, Transaction *transaction) {
  //1. Check whether the current tree is empty
  if(this->IsEmpty() == true){
    return;
  }
  else{
    Page * page = FindLeafPage(key, INVALID_PAGE_ID, false);
    if(page == nullptr){
      LOG(ERROR) << "Can't find the leaf page in the Remove" << endl;
      throw std::bad_alloc();
    }
    //2. Fetch the leaf_page according to the key
    LeafPage * leaf_page = reinterpret_cast<LeafPage *>(page->GetData());
    //3. Check whether the leaf_page is need to redistribute or coalesce
    leaf_page->RemoveAndDeleteRecord(key, processor_);
    TreeFileManagers mgr("tree2_");
    PrintTree(mgr[0]);
    bool is_dirty = CoalesceOrRedistribute(leaf_page, transaction);
    PrintTree(mgr[1]);
    buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), is_dirty);
  }
}

/* todo
 * User needs to first find the sibling of input page. If sibling's size + input
 * page's size > page's max size, then redistribute. Otherwise, merge.
 * Using template N to represent either internal page or leaf page.
 * @return: true means target leaf page should be deleted, false means no
 * deletion happens
 */
template <typename N>
bool BPlusTree::CoalesceOrRedistribute(N *&node, Transaction *transaction) {
  //Notice: sibling place:
  //index == 0: node:sibling
  //index != 0: sibling: node
  //1. Check whether the node is the root
  if(node->IsRootPage() == true){
    return AdjustRoot(node);
  }
  //2. Check whether the leaf_page is need to redistribute or coalesce
  else if(node->GetSize() >= node->GetMinSize()){
    return false;
  }
  //3. Dealing with the redistribute or coalesce
  else{
    page_id_t parent_id = node->GetParentPageId();
    InternalPage * parent_page = reinterpret_cast<InternalPage *>(buffer_pool_manager_->FetchPage(parent_id)->GetData());
    if(parent_page == nullptr){
       LOG(ERROR) << "Can't find the parent page in the CoalesceOrRedistribute" << endl;
       throw std::bad_alloc();
    }
    int current_index = parent_page->ValueIndex(node->GetPageId());
    //Note: default is towards front, if the current_index is 0, then towards back
    int sibling_index = (current_index == 0) ? current_index + 1 : current_index - 1;
    Page * page = buffer_pool_manager_->FetchPage(parent_page->ValueAt(sibling_index));
    if(page == nullptr){
       LOG(ERROR) << "Can't find the sibling page in the CoalesceOrRedistribute" << endl;
       throw std::bad_alloc();
    }
    if(node->IsLeafPage() == true){
      LeafPage * leaf_sibling_page = reinterpret_cast<LeafPage *>(page->GetData());
      LeafPage * leaf_node = reinterpret_cast<LeafPage *>(node);
      //Note the degree = 4, which is the maxSize(),but in fact, the max item in the leaf node is 3
      // , when two nodes are 1 and 3, then we need to redistribute.
      if(leaf_sibling_page->GetSize() + node->GetSize() > node->GetMaxSize()-1){
        Redistribute(leaf_sibling_page, leaf_node, current_index);
        buffer_pool_manager_->UnpinPage(parent_id, true);
        buffer_pool_manager_->UnpinPage(leaf_sibling_page->GetPageId(), true);
        buffer_pool_manager_->UnpinPage(leaf_node->GetPageId(), true);
        return true;
      }
      else{
//        Check();
        Coalesce(leaf_sibling_page, leaf_node, parent_page,  current_index, transaction);
//        Check();
        buffer_pool_manager_->UnpinPage(parent_id, true);
        buffer_pool_manager_->UnpinPage(leaf_sibling_page->GetPageId(), true);
        buffer_pool_manager_->UnpinPage(leaf_node->GetPageId(), true);
        Check();
        return true;
      }
    }
    else{
      InternalPage * internal_sibling_page = reinterpret_cast<InternalPage *>(page->GetData());
      InternalPage * internal_node = reinterpret_cast<InternalPage *>(node);
      //Note: InternalPage has the same degree as the leaf page, but it can have 4 kids, not GetMaxSize()-1
      if(internal_sibling_page->GetSize() + node->GetSize() > node->GetMaxSize()){
        Redistribute(internal_sibling_page, internal_node, current_index);
        buffer_pool_manager_->UnpinPage(parent_id, true);
        buffer_pool_manager_->UnpinPage(internal_sibling_page->GetPageId(), true);
        buffer_pool_manager_->UnpinPage(internal_node->GetPageId(), true);
        return true;
      }
      else{
        Coalesce(internal_sibling_page, internal_node, parent_page,  current_index, transaction);
        buffer_pool_manager_->UnpinPage(parent_id, true);
        buffer_pool_manager_->UnpinPage(internal_sibling_page->GetPageId(), true);
        buffer_pool_manager_->UnpinPage(internal_node->GetPageId(), true);
        return true;
      }
    }
  }
}

/*
 * Move all the key & value pairs from one page to its sibling page, and notify
 * buffer pool manager to delete this page. Parent page must be adjusted to
 * take info of deletion into account. Remember to deal with coalesce or
 * redistribute recursively if necessary.
 * Using template N to represent either internal page or leaf page.
 * @param   neighbor_node      sibling page of input "node"
 * @param   node               input from method coalesceOrRedistribute()
 * @param   parent             parent page of input "node"
 * @return  true means parent node should be deleted, false means no deletion happened
 */
bool BPlusTree::Coalesce(LeafPage *&neighbor_node, LeafPage *&node, InternalPage *&parent, int index,
                         Transaction *transaction) {
  page_id_t parent_page_id = neighbor_node->GetParentPageId();
  InternalPage * parent_page = reinterpret_cast<InternalPage *>(buffer_pool_manager_->FetchPage(parent_page_id)->GetData());
  if(parent_page == nullptr){
    LOG(ERROR) << "Can't find the parent page in the Coalesce" << endl;
    throw std::bad_alloc();
  }
  //Note: The neighbor_node can only add all the items from the node to the end itself. So we need to change their place.
  if(index == 0){
    LeafPage * temp = neighbor_node;
    neighbor_node = node;
    node = temp;
    index = 1;
  }
  node->MoveAllTo(neighbor_node);
  if(index != 0)
    neighbor_node->SetNextPageId(node->GetNextPageId());
  parent_page->Remove(index);
  buffer_pool_manager_->UnpinPage(parent_page_id, true);
  PrintTree(mgr[3]);

  return CoalesceOrRedistribute(parent_page, transaction);
}

bool BPlusTree::Coalesce(InternalPage *&neighbor_node, InternalPage *&node, InternalPage *&parent, int index,
                         Transaction *transaction) {
  page_id_t parent_page_id = neighbor_node->GetParentPageId();
  InternalPage * parent_page = reinterpret_cast<InternalPage *>(buffer_pool_manager_->FetchPage(parent_page_id)->GetData());
  if(parent_page == nullptr){
    LOG(ERROR) << "Can't find the parent page in the Coalesce" << endl;
    throw std::bad_alloc();
  }
  //Note: The neighbor_node can only add all the items from the node to the end itself. So we need to change their place.
  if(index == 0){
    InternalPage * temp = neighbor_node;
    neighbor_node = node;
    node = temp;
    //Change the index to 1, because the deleted node is the second child of the parent.
    index = 1;
  }
  node->MoveAllTo(neighbor_node, parent->KeyAt(index), buffer_pool_manager_);
  parent_page->Remove(index);
  buffer_pool_manager_->UnpinPage(parent_page_id, true);
  PrintTree(mgr[3]);

  return CoalesceOrRedistribute(parent_page, transaction);
}

/*
 * Redistribute key & value pairs from one page to its sibling page. If index ==
 * 0, move sibling page's first key & value pair into end of input "node",
 * otherwise move sibling page's last key & value pair into head of input
 * "node".
 * Using template N to represent either internal page or leaf page.
 * @param   neighbor_node      sibling page of input "node"
 * @param   node               input from method coalesceOrRedistribute()
 */
void BPlusTree::Redistribute(LeafPage *neighbor_node, LeafPage *node, int index) {
  //index == 0: node:sibling
  //index != 0: sibling: node
  //index is the index of node in its parent_node
  InternalPage * parent_page = reinterpret_cast<InternalPage *>(buffer_pool_manager_->FetchPage(node->GetParentPageId())->GetData());
  if(parent_page == nullptr){
    LOG(ERROR) << "Can't find the parent page in the Redistribute" << endl;
    throw std::bad_alloc();
  }
  //For different neighbor_node places
  if(index == 0){
    neighbor_node->MoveFirstToEndOf(node);
    parent_page->SetKeyAt(1, neighbor_node->KeyAt(0));
  }
  else{
    neighbor_node->MoveLastToFrontOf(node);
    parent_page->SetKeyAt(index, node->KeyAt(0));
  }
  buffer_pool_manager_->UnpinPage(parent_page->GetPageId(), true);
  PrintTree(mgr[3]);
}

void BPlusTree::Redistribute(InternalPage *neighbor_node, InternalPage *node, int index) {
  InternalPage * parent_page = reinterpret_cast<InternalPage *>(buffer_pool_manager_->FetchPage(node->GetParentPageId())->GetData());
  if(parent_page == nullptr){
    LOG(ERROR) << "Can't find the parent page in the Redistribute" << endl;
    throw std::bad_alloc();
  }
  //Note for the index 0 is INVALID
  if(index == 0){
    neighbor_node->MoveFirstToEndOf(node, parent_page->KeyAt(index), buffer_pool_manager_);
    parent_page->SetKeyAt(1, neighbor_node->KeyAt(0));
  }
  else{
    neighbor_node->MoveLastToFrontOf(node, parent_page->KeyAt(index), buffer_pool_manager_);
    parent_page->SetKeyAt(index, node->KeyAt(0));
  }
  buffer_pool_manager_->UnpinPage(parent_page->GetPageId(), true);
  PrintTree(mgr[3]);

}
/*
 * Update root page if necessary
 * NOTE: size of root page can be less than min size and this method is only
 * called within coalesceOrRedistribute() method
 * case 1: when you delete the last element in root page, but root page still
 * has one last child
 * case 2: when you delete the last element in whole b+ tree
 * @return : true means root page should be deleted, false means no deletion
 * happened
 */
bool BPlusTree::AdjustRoot(BPlusTreePage *old_root_node) {
  //case 1: when you delete the last element in root page, but root page still has one last child
  if(old_root_node->IsLeafPage() == false && old_root_node->GetSize() == 1){
    InternalPage * root_page = reinterpret_cast<InternalPage *>(old_root_node);
    //Record the old_page_Id to unpin the root_page
    page_id_t old_page_id = root_page->GetPageId();
    //Update the root_page_id of old_root_node to the only child
    this->root_page_id_ = root_page->RemoveAndReturnOnlyChild();
    UpdateRootPageId(true);
    LeafPage * new_root_page = reinterpret_cast<LeafPage *>(buffer_pool_manager_->FetchPage(this->root_page_id_)->GetData());
    new_root_page->SetParentPageId(INVALID_PAGE_ID);
    //Unpin the old_page_id and new_root_page
    buffer_pool_manager_->UnpinPage(this->root_page_id_, true);
    buffer_pool_manager_->UnpinPage(old_page_id, false);
    return true;
  }
  //case 2: when you delete the last element in whole b+ tree
  else if(old_root_node->IsLeafPage() == true && old_root_node->GetSize() == 0){
    InternalPage * root_page = reinterpret_cast<InternalPage *>(old_root_node);
    //Record the old_page_Id to unpin the root_page
    page_id_t old_page_id = root_page->GetPageId();
    this->root_page_id_ = INVALID_PAGE_ID;
    UpdateRootPageId(true);
    buffer_pool_manager_->UnpinPage(old_page_id, true);
    return true;
  }
  LOG(WARNING) << "The root page is not deleted" <<endl;
  return false;
}

/*****************************************************************************
 * INDEX ITERATOR
 *****************************************************************************/
/*
 * Input parameter is void, find the left most leaf page first, then construct
 * index iterator
 * @return : index iterator
 */
IndexIterator BPlusTree::Begin() {
  Page * left_page = FindLeafPage(nullptr, INVALID_PAGE_ID, true);
  buffer_pool_manager_->UnpinPage(left_page->GetPageId(), true);
  return IndexIterator(left_page->GetPageId(), buffer_pool_manager_, 0);
}

/*
 * Input parameter is low-key, find the leaf page that contains the input key
 * first, then construct index iterator
 * @return : index iterator
 */
IndexIterator BPlusTree::Begin(const GenericKey *key) {
   Page * page = FindLeafPage(key, INVALID_PAGE_ID, false);
   LeafPage * leafpage = reinterpret_cast<LeafPage *>(page->GetData());
   RowId value;
   if(!leafpage->Lookup(key, value, processor_)){
     ASSERT(false, "The key is not in the tree when");
   }
   int index = leafpage->KeyIndex(key, processor_);
   buffer_pool_manager_->UnpinPage(page->GetPageId(), true);
   return IndexIterator(page->GetPageId(), buffer_pool_manager_, index);
}

/*
 * Input parameter is void, construct an index iterator representing the end
 * of the key/value pair in the leaf node
 * @return : index iterator
 */
IndexIterator BPlusTree::End() {
  return IndexIterator(INVALID_PAGE_ID, buffer_pool_manager_, 0);
}

/*****************************************************************************
 * UTILITIES AND DEBUG
 *****************************************************************************/
/*
 * Find leaf page containing particular key, if leftMost flag == true, find
 * the left most leaf page,
 * Note: the leaf page is pinned, you need to unpin it after use.
 */
Page *BPlusTree::FindLeafPage(const GenericKey *key, page_id_t page_id, bool leftMost) {
  if(root_page_id_ == INVALID_PAGE_ID){
    ASSERT(false, "The root page is invalid when finding the leaf page\n");
  }
  if(leftMost == false){
    Page * root_page = buffer_pool_manager_->FetchPage(root_page_id_);
    InternalPage *page = reinterpret_cast<InternalPage *>(root_page->GetData());
    page_id_t child_id = root_page_id_; //Will record the last fetch
    //Iteratively find the leaf page
    while(page->IsLeafPage() == false){
      if(processor_.CompareKeys(key, page->KeyAt(1)) < 0){
        child_id = page->ValueAt(0);
      }
      else if(processor_.CompareKeys(key, page->KeyAt(page->GetSize()-1)) >= 0){
        child_id = page->ValueAt(page->GetSize()-1);
      }
      else{
        for(int i = 1; i < page->GetSize()-1; i++)
          if(processor_.CompareKeys(key, page->KeyAt(i)) >= 0 && processor_.CompareKeys(key, page->KeyAt(i+1)) < 0){
            child_id = page->ValueAt(i);
            break;
          }
      }
      buffer_pool_manager_->UnpinPage(page->GetPageId(), true);
      page = reinterpret_cast<InternalPage *>(buffer_pool_manager_->FetchPage(child_id)->GetData());
    }
    //Now the page is leaf_page
    buffer_pool_manager_->UnpinPage(page->GetPageId(), true);
    buffer_pool_manager_->UnpinPage(root_page->GetPageId(), false);
    Page * leaf_page = buffer_pool_manager_->FetchPage(child_id);

    return leaf_page;
  }
  else
  {
    Page * root_page = buffer_pool_manager_->FetchPage(root_page_id_);
    InternalPage * interval_page = reinterpret_cast<InternalPage *>(root_page->GetData());
    page_id_t child_id = root_page_id_;
    while(interval_page->IsLeafPage() == false){
      child_id = interval_page->ValueAt(0);
      buffer_pool_manager_->UnpinPage(interval_page->GetPageId(), false);
      interval_page = reinterpret_cast<InternalPage *>(buffer_pool_manager_->FetchPage(child_id)->GetData());
    }
    buffer_pool_manager_->UnpinPage(interval_page->GetPageId(), false);
    buffer_pool_manager_->UnpinPage(root_page->GetPageId(), false);
    Page *leaf_page = buffer_pool_manager_->FetchPage(child_id);

    return leaf_page;
  }

}

/*
 * Update/Insert root page id in header page(where page_id = 0, header_page is
 * defined under include/page/header_page.h)
 * Call this method everytime root page id is changed.
 * @parameter: insert_record      default value is false. When set to true,
 * insert a record <index_name, current_page_id> into header page instead of
 * updating it.
 */
void BPlusTree::UpdateRootPageId(int insert_record) {
  if(insert_record == false){
    IndexRootsPage *root_page = reinterpret_cast<IndexRootsPage *>(buffer_pool_manager_->FetchPage(INDEX_ROOTS_PAGE_ID)->GetData());
    root_page->Insert(this->index_id_, this->root_page_id_);
    buffer_pool_manager_->UnpinPage(INDEX_ROOTS_PAGE_ID, true);
  }
  else{
    IndexRootsPage *root_page = reinterpret_cast<IndexRootsPage *>(buffer_pool_manager_->FetchPage(INDEX_ROOTS_PAGE_ID)->GetData());
    root_page->Update(this->index_id_, this->root_page_id_);
    buffer_pool_manager_->UnpinPage(INDEX_ROOTS_PAGE_ID, true);
  }
}

/**
 * This method is used for debug only, You don't need to modify
 */
void BPlusTree::ToGraph(BPlusTreePage *page, BufferPoolManager *bpm, std::ofstream &out) const {
  std::string leaf_prefix("LEAF_");
  std::string internal_prefix("INT_");
  if (page->IsLeafPage()) {
    auto *leaf = reinterpret_cast<LeafPage *>(page);
    // Print node name
    out << leaf_prefix << leaf->GetPageId();
    // Print node properties
    out << "[shape=plain color=green ";
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">P=" << leaf->GetPageId()
        << ",Parent=" << leaf->GetParentPageId() << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">"
        << "max_size=" << leaf->GetMaxSize() << ",min_size=" << leaf->GetMinSize() << ",size=" << leaf->GetSize()
        << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < leaf->GetSize(); i++) {
      out << "<TD>" << leaf->ValueAt(i).Get() << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print Leaf node link if there is a next page
    if (leaf->GetNextPageId() != INVALID_PAGE_ID) {
      out << leaf_prefix << leaf->GetPageId() << " -> " << leaf_prefix << leaf->GetNextPageId() << ";\n";
      out << "{rank=same " << leaf_prefix << leaf->GetPageId() << " " << leaf_prefix << leaf->GetNextPageId() << "};\n";
    }

    // Print parent links if there is a parent
    if (leaf->GetParentPageId() != INVALID_PAGE_ID) {
      out << internal_prefix << leaf->GetParentPageId() << ":p" << leaf->GetPageId() << " -> " << leaf_prefix
          << leaf->GetPageId() << ";\n";
    }
  } else {
    auto *inner = reinterpret_cast<InternalPage *>(page);
    // Print node name
    out << internal_prefix << inner->GetPageId();
    // Print node properties
    out << "[shape=plain color=pink ";  // why not?
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">P=" << inner->GetPageId()
        << ",Parent=" << inner->GetParentPageId() << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << inner->GetSize() << "\">"
        << "max_size=" << inner->GetMaxSize() << ",min_size=" << inner->GetMinSize() << ",size=" << inner->GetSize()
        << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < inner->GetSize(); i++) {
      out << "<TD PORT=\"p" << inner->ValueAt(i) << "\">";
      if (i > 0) {
        out << inner->KeyAt(i);
      } else {
        out << " ";
      }
      out << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print Parent link
    if (inner->GetParentPageId() != INVALID_PAGE_ID) {
      out << internal_prefix << inner->GetParentPageId() << ":p" << inner->GetPageId() << " -> " << internal_prefix
          << inner->GetPageId() << ";\n";
    }
    // Print leaves
    for (int i = 0; i < inner->GetSize(); i++) {
      auto child_page = reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(inner->ValueAt(i))->GetData());
      ToGraph(child_page, bpm, out);
      if (i > 0) {
        auto sibling_page = reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(inner->ValueAt(i - 1))->GetData());
        if (!sibling_page->IsLeafPage() && !child_page->IsLeafPage()) {
          out << "{rank=same " << internal_prefix << sibling_page->GetPageId() << " " << internal_prefix
              << child_page->GetPageId() << "};\n";
        }
        bpm->UnpinPage(sibling_page->GetPageId(), false);
      }
    }
  }
  bpm->UnpinPage(page->GetPageId(), false);
}

/**
 * This function is for debug only, you don't need to modify
 */
void BPlusTree::ToString(BPlusTreePage *page, BufferPoolManager *bpm) const {
  if (page->IsLeafPage()) {
    auto *leaf = reinterpret_cast<LeafPage *>(page);
    std::cout << "Leaf Page: " << leaf->GetPageId() << " parent: " << leaf->GetParentPageId()
              << " next: " << leaf->GetNextPageId() << std::endl;
    for (int i = 0; i < leaf->GetSize(); i++) {
      std::cout << leaf->KeyAt(i) << ",";
    }
    std::cout << std::endl;
    std::cout << std::endl;
  } else {
    auto *internal = reinterpret_cast<InternalPage *>(page);
    std::cout << "Internal Page: " << internal->GetPageId() << " parent: " << internal->GetParentPageId() << std::endl;
    for (int i = 0; i < internal->GetSize(); i++) {
      std::cout << internal->KeyAt(i) << ": " << internal->ValueAt(i) << ",";
    }
    std::cout << std::endl;
    std::cout << std::endl;
    for (int i = 0; i < internal->GetSize(); i++) {
      ToString(reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(internal->ValueAt(i))->GetData()), bpm);
      bpm->UnpinPage(internal->ValueAt(i), false);
    }
  }
}

bool BPlusTree::Check() {
  bool all_unpinned = buffer_pool_manager_->CheckAllUnpinned();
  if (!all_unpinned) {
    LOG(ERROR) << "problem in page unpin" << endl;
  }
  return all_unpinned;
}
#pragma clang diagnostic pop