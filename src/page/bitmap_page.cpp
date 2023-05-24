#include "page/bitmap_page.h"

#include "glog/logging.h"

/**
 * TODO: Student Implement
 */

std::bitset<8> BYTE("00000000");

template <size_t PageSize>
bool BitmapPage<PageSize>::IsEmpty() const{
  return this->page_allocated_ == 0;
}

template <size_t PageSize>
bool BitmapPage<PageSize>::IsFull() const{
  return this->page_allocated_ == GetMaxSupportedSize();
}

template <size_t PageSize>
BitmapPage<PageSize>::BitmapPage()
{
  this->next_free_page_ = 0;
  this->page_allocated_ = 0;
  for(size_t i=0; i<MAX_CHARS; i++)
    this->bytes[i] = 0;
}

//bitmap_data is an array of bytes
template <size_t PageSize>
BitmapPage<PageSize>::BitmapPage(char *bitmap_data)
{
  uint32_t* ptr = reinterpret_cast<uint32_t*> (bitmap_data);
  this->page_allocated_ = (*ptr);
  this->next_free_page_ = *(ptr+1);
  //Transfer the bit data to array "byte" from the original data
  //2 uint32 = 8 bytes
  for(size_t i=0; i<MAX_CHARS; i++)
    this->bytes[i] = bitmap_data[i+8];
}

template <size_t PageSize>
bool BitmapPage<PageSize>::AllocatePage(uint32_t &page_offset) {
  if(this->page_allocated_ == GetMaxSupportedSize()){
    return false;
  }
  page_offset = this->next_free_page_;
  this->page_allocated_ += 1;
  this->bytes[page_offset / 8] |= (1 << (page_offset%8));
  if(this->page_allocated_ != GetMaxSupportedSize())
  if(IsPageFree(page_offset+1))
    next_free_page_ = page_offset+1;
  else{
    while(!IsPageFree(page_offset))
    {
      page_offset++;
      page_offset %= GetMaxSupportedSize();
    }
    next_free_page_ = page_offset;
  }
    return true;
}

/**
 * TODO: Student Implement
 */
template <size_t PageSize>
bool BitmapPage<PageSize>::DeAllocatePage(uint32_t page_offset) {
  if(IsPageFree(page_offset))
    return false;
  this->bytes[page_offset / 8] &= ~(1 << (page_offset%8));
  this->page_allocated_ -= 1;
  this->next_free_page_ = page_offset;
  return true;
}

/**
 * TODO: Student Implement
 */
template <size_t PageSize>
bool BitmapPage<PageSize>::IsPageFree(uint32_t page_offset) const {
  if(page_offset >= GetMaxSupportedSize())
    return false;
  if((this->bytes[page_offset / 8] & (1 << (page_offset%8))) == 0){
    return true;
  }
  return false;
}

template <size_t PageSize>
bool BitmapPage<PageSize>::IsPageFreeLow(uint32_t byte_index, uint8_t bit_index) const {
  if(byte_index * 8 + bit_index >= GetMaxSupportedSize())
    return false;
  if((this->bytes[byte_index] & (1 << bit_index)) == 0){
    return true;
  }
  return false;
}

template class BitmapPage<64>;

template class BitmapPage<128>;

template class BitmapPage<256>;

template class BitmapPage<512>;

template class BitmapPage<1024>;

template class BitmapPage<2048>;

template class BitmapPage<4096>;