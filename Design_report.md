## Minisql Design Report

3210105944 黄锦骏

### 1.  Disk Manager

#### 1.1 模块介绍

+ 功能介绍：Disk Manager负责数据库文件中数据页page的分配和回收，以及数据页中数据的读取和写出。

+ 结构：由Meta page, Bitmap page, Extent pages三部分组成，其在内存中的组织可以参考下图。其中一个Bitmap page和Extent pages组成一个extent

  ![image-20230521162534593](C:\Users\squarehuang\AppData\Roaming\Typora\typora-user-images\image-20230521162534593.png)

  + Meta page: 作为Disk Manager的元数据，它记录了Disk中全部已经分配的page number，extent number和每个extent中已经使用过的page数量

  + Bitmap Page: 在一个Extent中，标记该Extent中一段连续数据页的分配情况。其中记录了当前extent中已经分配的page number，以及该extent中的next_free_page_，并用一个char数组来标记该page是否已经分配过，每一个byte用于标记八个page.

  + Extent Page：具体存储数据的page，具有page_id_属性，为buffer\_pool\_manager所访问的逻辑地址，pin\_count属性用于记录有多少线程正在访问该page，is\_dirty用于标记该page是否有数据，一个page latch，和一个data数组用于存储真实数据

#### 1.2 具体实现细节

+ Bitmap Page: 
  + 添加了一个Bitmap Page的构造函数，用于将大小为PAGE_SIZE的char数组内数据赋值给Bitmap Page中的各个属性。添加了判断Bitmap Page为空和满的两个函数IsEmpty和IsFull
  + AllocatePage：首先需要判断Bitmap_page是否已经分配完全，然后根据传入的offset计算出是哪个byte的哪一位。计算出后进行将该位标记为1。next_free_page从当前page_offset开始遍历直到发现为空
  + DeAlloacatePage: 首先判断传入的page_offset所对应的page是否free, 再进行标记更新

+ DiskManager:

  + AllocatePage：实际上调用了Bitmap_page的allocatepage方法来分配page. 首先从物理内存中读取bitmap_page，如果Bitmap_page未满，就使用该bitmap_page进行分配。否则读取一个新的Bitmap_page，每次分配后要更新meta_page的元数据，最后要将bitmap_page重新写入到内存中去

  + DeAllocatePage: 同理，根据输入的logical_page_id计算出一个extent中的物理页page_index和extent的index(Bitmap_page_index)，再实际调用bitmap_page.DeAllocatePage释放page并更新元数据.特别注意当释放page后如果bitmap_page为空，则需要将meta_page中的extent数目减1

  + 数据页中数据的读入和写出由WritePhysicalPage和ReadPhysicalPage实现

  + MapPageId: 实现的是逻辑id到物理id的映射

    ```c++
    physical_id = logical_page_id + 2 + logical_page_id / BITMAP_SIZE
    ```

### 2.  BufferPool Manager

#### 2.1 模块介绍

+ 功能介绍：BufferPool模块主要负责将磁盘中的数据页从内存中来回移动到磁盘，实现了类似于缓存的操作

+ 结构：

  + 属性元素：![image-20230522091132934](C:\Users\squarehuang\AppData\Roaming\Typora\typora-user-images\image-20230522091132934.png)

  + 功能函数：

    ![image-20230522091341614](C:\Users\squarehuang\AppData\Roaming\Typora\typora-user-images\image-20230522091341614.png)

#### 2.2 具体实现细节

+ LRU替换算法：即最近最少使用算法，是一种内存数据淘汰策略，使用常见是当内存不足时，需要淘汰最近最少使用的数据。具体实现采用双向链表和Hashmap的方式实现

  ![image-20230522091647636](C:\Users\squarehuang\AppData\Roaming\Typora\typora-user-images\image-20230522091647636.png)

  LRU_list用于存储当前在LRU_list中的数据页，Hashmap LRU_map用于标记某个数据页当前是否在LRU_list中(便于快速查找)，max_pages是LRU_list中最大的page数目上限

  + 具体实现思想：每当有空闲的数据页时(即没有占用数据页的进程)那么就将该数据页加入到LRU_list的头部，并在map中标记该数据页已经在LRU_list中。当数据页被占用后，就应从LRU_List中删除。当需要替换时就返回LRU_list中头部的元素，因为每次插入到头部可以保证头部元素是最近访问过的

+ BufferPool Manager: 依次介绍每个模块的算法设计思路

  + FetchPage：

    > 1.     Search the page table for the requested page (P).
    > 1.1    If P exists, pin it and return it immediately.
    > 1.2    If P does not exist, find a replacement page (R) from either the free list or the replacer.Note that pages are always found from the free list first.
    > 2.     If R is dirty, write it back to the disk.
    > 3.     Delete R from the page table and insert P.
    > 4.     Update P's metadata, read in the page content from disk, and then return a pointer to P.

  + NewPage：

    > 1.   If all the pages in the buffer pool are pinned, return nullptr.
    > 2.   Pick a victim page P from either the free list or the replacer. Always pick from the free list first.
    > 3.   Update P's metadata, zero out memory and add P to the page table.
    > 4.   Set the page ID output parameter. Return a pointer to P.

  + UnpinPage

    > 1. Search the page_table_ to check whether the page is in the buffer pool.
    > 2. Check whether the page is pinned or not. If it is pinned, it can't be unpinned again.
    > 3. If the page is pinned, unpin it and update the metadata.
    > 4. Check whether its pin number is 0 after unpinning.

  + FlushPage

    > 1. Check whether the page is in the buffer pool.
    > 2. Write the page into the disk

  + DeletePage

    > 0.   Make sure you call DeallocatePage!
    > 1.   Search the page table for the requested page (P).
    >      1.1   If P does not exist, return true.
    >      1.2.   If P exists, but has a non-zero pin-count, return false. Someone is using the page.
    > 2.   Otherwise, P can be deleted. Remove P from the page table, reset its metadata and return it to the free list.

  ### 3. Record Manager:

  #### 3.1 模块介绍

  ##### 3.1.1 Table_page:

  + 实现构成：table_page的具体实现分为两个部分，table_Page外部作为双向链表的连接部分，内部tuple的插入，更新，删除部分，但在删除部分仅仅是打上了逻辑上的DeletedFlag标记，没有实际删除

  + 物理组织：table_page作为一个数据页，大小仍然为PAGE_SIZE，物理上由table_page_header, free_space和insert_tuples所构成。table_page_header结构如下

    ![image-20230523131012435](C:\Users\squarehuang\AppData\Roaming\Typora\typora-user-images\image-20230523131012435.png)

    访问数据通过FreeSpacePointer以及偏移的计算来访问，每一个tuple有自己的slot_number用于访问自身的offset和size，再通过自身的offset访问该Page中的具体存储地址. 

  ##### 3.1.2 table_heap

  + 

  ### 4. Index Manager

  #### 4.1 模块介绍

  

  #### 4.2 具体实现细节

  

  ### 5. Catalog Manager

  #### 5.1 模块介绍
  
  #### 5.2 具体实现细节
  
  
  
  ### 6. Planner Manager
  
  #### 6.1 模块介绍
  
  #### 6.2 具体实现细节
  
  
  
  ### 7. Executor Manager
  
  #### 7.1 模块介绍
  
  #### 7.2 具体实现细节

