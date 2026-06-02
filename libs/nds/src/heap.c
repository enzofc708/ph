#include "nds/heap.h"

#define SetBit(data, st, bits, val)       \
    {                                     \
        u32 maskBits = ((1 << bits) - 1); \
        u32 newVal   = val & maskBits;    \
        maskBits <<= st;                  \
        data &= ~maskBits;                \
        data |= newVal << st;             \
    };

static inline u32 Diff_Ptr_inline(const void *start, const void *end) {
    return end - start;
}

static inline void *AddU32_inline(void *ptr, u32 val) {
    return (void *) (val + (u32) ptr);
}

static inline void Fill32_inline(Heap_Header *header, void *address, u32 size) {
    if ((u16) (header->flags & 0xFF) & 1) {
        Fill32(0, address, size);
    }
}

static inline Heap_EXPHHeader *GetEXPH_inline(Heap_Header *header) {
    return (u32) header + sizeof(Heap_Header);
}

THUMB void Heap_GetRegion(Heap_Region *region, Heap_EXPHBlockHeader *block) {
    region->start = (u32) block - (u16) (block->flags >> 8 & 0x7F);
    region->end   = block->size + ((u32) block + sizeof(Heap_EXPHBlockHeader));
}

Heap_EXPHBlockHeader *Heap_RemoveBlock(Heap_EXPHBlockList *list, Heap_EXPHBlockHeader *block) {
    Heap_EXPHBlockHeader *const p = block->prev;
    Heap_EXPHBlockHeader *const n = block->next;

    if (p) {
        p->next = n;
    } else {
        list->head = n;
    }

    if (n) {
        n->prev = p;
    } else {
        list->tail = p;
    }

    return p;
}

Heap_EXPHBlockHeader *Heap_InsertBlock(Heap_EXPHBlockList *list, Heap_EXPHBlockHeader *block, Heap_EXPHBlockHeader *prev) {
    Heap_EXPHBlockHeader *next;

    block->prev = prev;
    if (prev) {
        next       = prev->next;
        prev->next = block;
    } else {
        next       = list->head;
        list->head = block;
    }

    block->next = next;
    if (next) {
        next->prev = block;
    } else {
        list->tail = block;
    }

    return block;
}

Heap_EXPHBlockHeader *Heap_InitBlock(const Heap_Region *r, u16 stamp) {
    Heap_EXPHBlockHeader *block = r->start;
    block->stamp                = stamp;
    block->flags                = 0;
    block->size                 = r->end - ((u32) block + 0x10);
    block->prev                 = NULL;
    block->next                 = NULL;
    return block;
}

Heap_Header *Heap_EXPHInit(void *start, void *end, u16 flag) {
    Heap_EXPHBlockHeader *emptyBlock;
    Heap_Region region;
    Heap_Header *header         = start;
    Heap_EXPHHeader *EXPHHeader = (void *) ((u32) header + sizeof(Heap_Header));

    func_0201776c(header, 'EXPH', (void *) ((u32) EXPHHeader + sizeof(Heap_EXPHHeader)), end, flag);

    EXPHHeader->id    = 0;
    EXPHHeader->flags = 0;
    SetBit(EXPHHeader->flags, 0, 1, 0);

    region.start = header->startAddr;
    region.end   = header->endAddr;
    emptyBlock   = Heap_InitBlock(&region, 'FR');

    EXPHHeader->freeBlocks.head = emptyBlock;
    EXPHHeader->freeBlocks.tail = emptyBlock;
    EXPHHeader->usedBlocks.head = NULL;
    EXPHHeader->usedBlocks.tail = NULL;

    return header;
}

void *Heap_AllocFreeBlock(Heap_EXPHHeader *EXPHHeader, Heap_EXPHBlockHeader *blockHeader, void *block, u32 size,
                          u16 direction) {
    Heap_Region free1;
    Heap_Region free2;
    Heap_EXPHBlockHeader *prev;
    Heap_EXPHBlockHeader *usedBlock;
    Heap_Region r;

    Heap_GetRegion(&free1, blockHeader);
    free2.end   = free1.end;
    free2.start = AddU32_inline(block, size);
    free1.end   = (u32) block - (u32) sizeof(Heap_EXPHBlockHeader);
    prev        = Heap_RemoveBlock(&EXPHHeader->freeBlocks, blockHeader);

    if (free1.end - free1.start < (u32) 0x14) {
        free1.end = free1.start;
    } else {
        prev = Heap_InsertBlock(&EXPHHeader->freeBlocks, Heap_InitBlock(&free1, 'FR'), prev);
    }

    if (free2.end - free2.start < (u32) 0x14) {
        free2.start = free2.end;
    } else {
        Heap_InsertBlock(&EXPHHeader->freeBlocks, Heap_InitBlock(&free2, 'FR'), prev);
    }

    Fill32_inline((u32) EXPHHeader - sizeof(Heap_Header), free1.end, free2.start - free1.end);

    r.start   = (u32) block - (u32) sizeof(Heap_EXPHBlockHeader);
    r.end     = free2.start;
    usedBlock = Heap_InitBlock(&r, 'UD');
    SetBit(usedBlock->flags, 15, 1, direction);
    SetBit(usedBlock->flags, 8, 7, (u16) Diff_Ptr_inline(free1.end, usedBlock));
    SetBit(usedBlock->flags, 0, 8, EXPHHeader->id);
    Heap_InsertBlock(&EXPHHeader->usedBlocks, usedBlock, EXPHHeader->usedBlocks.tail);
    return block;
}

void *Heap_EXPHAllocHead(Heap_Header *header, u32 size, int alignment) {
    Heap_EXPHHeader *expHeader = GetEXPH_inline(header);
    Heap_EXPHBlockHeader *blockHeader;
    const u32 flag1 = (u16) (expHeader->flags & 1) == 0;

    Heap_EXPHBlockHeader *foundBlockHeader = NULL;
    u32 foundSize                          = 0xffffffff;
    void *foundBlock                       = NULL;

    for (blockHeader = expHeader->freeBlocks.head; blockHeader; blockHeader = blockHeader->next) {
        void *const mblock   = AddU32_inline(blockHeader, sizeof(Heap_EXPHBlockHeader));
        void *const reqBlock = ((alignment - 1) + (u32) mblock) & ~(alignment - 1);
        const u32 offset     = reqBlock - mblock;

        if (blockHeader->size >= size + offset && foundSize > blockHeader->size) {
            foundBlockHeader = blockHeader;
            foundSize        = blockHeader->size;
            foundBlock       = reqBlock;

            if (flag1 || foundSize == size) {
                break;
            }
        }
    }

    if (!foundBlockHeader) {
        return NULL;
    }

    return Heap_AllocFreeBlock(expHeader, foundBlockHeader, foundBlock, size, 0);
}

void *Heap_EXPHAllocTail(Heap_Header *header, u32 size, int alignment) {
    Heap_EXPHHeader *expHeader = GetEXPH_inline(header);
    Heap_EXPHBlockHeader *blockHeader;

    const u32 flag1 = (u16) (expHeader->flags & 1) == 0;

    Heap_EXPHBlockHeader *foundBlockHeader = NULL;
    u32 foundSize                          = 0xffffffff;
    void *foundBlock                       = NULL;

    for (blockHeader = expHeader->freeBlocks.tail; blockHeader; blockHeader = blockHeader->prev) {
        void *const mblock    = AddU32_inline(blockHeader, sizeof(Heap_EXPHBlockHeader));
        void *const mblockEnd = AddU32_inline(mblock, blockHeader->size);
        void *const reqBlock  = (u32) (mblockEnd - size) & ~(alignment - 1);

        if (reqBlock - mblock >= 0 && foundSize > blockHeader->size) {
            foundBlockHeader = blockHeader;
            foundSize        = blockHeader->size;
            foundBlock       = reqBlock;

            if (flag1 || foundSize == size) {
                break;
            }
        }
    }

    if (!foundBlockHeader) {
        return NULL;
    }

    return Heap_AllocFreeBlock(expHeader, foundBlockHeader, foundBlock, size, 1);
}

u32 Heap_EmptyRegion(Heap_EXPHHeader *expHeader, const Heap_Region *r) {
    Heap_EXPHBlockHeader *freeBlockHeader = NULL;
    Heap_Region freeRgn                   = *r;
    Heap_EXPHBlockHeader *block;

    for (block = expHeader->freeBlocks.head; block; block = block->next) {
        if (block < r->start) {
            freeBlockHeader = block;
            continue;
        }

        if (block == r->end) {
            freeRgn.end = AddU32_inline(AddU32_inline(block, sizeof(Heap_EXPHBlockHeader)), block->size);
            Heap_RemoveBlock(&expHeader->freeBlocks, block);
        }
        break;
    }

    if (freeBlockHeader &&
        AddU32_inline(AddU32_inline(freeBlockHeader, sizeof(Heap_EXPHBlockHeader)), freeBlockHeader->size) == r->start) {
        freeRgn.start   = freeBlockHeader;
        freeBlockHeader = Heap_RemoveBlock(&expHeader->freeBlocks, freeBlockHeader);
    }

    if (freeRgn.end - freeRgn.start < sizeof(Heap_EXPHBlockHeader)) {
        return 0;
    }

    Heap_InsertBlock(&expHeader->freeBlocks, Heap_InitBlock(&freeRgn, 'FR'), freeBlockHeader);
    return 1;
}

Heap_Header *Heap_EXPHCreate(void *startAddress, u32 size, u16 optFlag) {
    void *endAddress;
    Heap_Header *header;
    endAddress   = ((u32) AddU32_inline(startAddress, size)) & ~3;
    startAddress = (u32) startAddress + 3 & ~3;

    if (startAddress > endAddress || endAddress - startAddress < (u32) 0x4c) {
        return NULL;
    }

    header = Heap_EXPHInit(startAddress, endAddress, optFlag);
    return header;
}

void Heap_EXPHDestroy(Heap_Header *heap) {
    func_020177c8(heap);
}

void *Heap_EXPHNew(Heap_Header *heap, u32 size, int alignment) {
    void *h = NULL;

    if (size == 0) {
        size = 1;
    }

    size = (size + 3) & ~3;

    if (alignment >= 0) {
        h = Heap_EXPHAllocHead(heap, size, alignment);
    } else {
        h = Heap_EXPHAllocTail(heap, size, -alignment);
    }

    return h;
}

u32 Heap_EXPHResizeBlock(Heap_Header *heap, void *memBlock, u32 size) {
    Heap_EXPHHeader *expHeader;
    Heap_EXPHBlockHeader *blockHeader;

    expHeader   = AddU32_inline(heap, sizeof(Heap_Header));
    blockHeader = (u32) memBlock - sizeof(Heap_EXPHBlockHeader);

    size = (size + 3) & ~3;
    if (size == blockHeader->size) {
        return size;
    }

    if (size > blockHeader->size) {
        void *end = AddU32_inline(AddU32_inline(blockHeader, sizeof(Heap_EXPHBlockHeader)), blockHeader->size);
        Heap_EXPHBlockHeader *block;
        Heap_Region tmpRgn;
        void *tmpStart;
        Heap_EXPHBlockHeader *newPrev;

        for (block = expHeader->freeBlocks.head; block; block = block->next) {
            if (block == end) {
                break;
            }
        }

        if (!block || size > blockHeader->size + sizeof(Heap_EXPHBlockHeader) + block->size) {
            return 0;
        }

        Heap_GetRegion(&tmpRgn, block);
        newPrev = Heap_RemoveBlock(&expHeader->freeBlocks, block);

        tmpStart     = tmpRgn.start;
        tmpRgn.start = AddU32_inline(memBlock, size);

        if (tmpRgn.end - tmpRgn.start < sizeof(Heap_EXPHBlockHeader)) {
            tmpRgn.start = tmpRgn.end;
        }

        blockHeader->size = tmpRgn.start - memBlock;

        if (tmpRgn.end - tmpRgn.start >= sizeof(Heap_EXPHBlockHeader)) {
            Heap_InsertBlock(&expHeader->freeBlocks, Heap_InitBlock(&tmpRgn, 'FR'), newPrev);
        }

        Fill32_inline(heap, tmpStart, tmpRgn.start - tmpStart);

    } else {
        Heap_Region tmpRgn;
        const u32 oldBlockSize = blockHeader->size;

        tmpRgn.start      = AddU32_inline(memBlock, size);
        tmpRgn.end        = AddU32_inline(AddU32_inline(blockHeader, sizeof(Heap_EXPHBlockHeader)), blockHeader->size);
        blockHeader->size = size;

        if (!Heap_EmptyRegion(expHeader, &tmpRgn)) {
            blockHeader->size = oldBlockSize;
        }
    }

    return blockHeader->size;
}

void Heap_EXPHFreeBlock(Heap_Header *heap, void *memBlock) {
    Heap_Header *header               = heap;
    Heap_EXPHHeader *expHeader        = AddU32_inline(header, sizeof(Heap_Header));
    Heap_EXPHBlockHeader *blockHeader = (Heap_EXPHBlockHeader *) (memBlock - sizeof(Heap_EXPHBlockHeader));
    Heap_Region region;

    Heap_GetRegion(&region, blockHeader);
    Heap_RemoveBlock(&expHeader->usedBlocks, blockHeader);
    Heap_EmptyRegion(expHeader, &region);
}

u32 Heap_EXPHGetEmptySize(Heap_Header *heap) {
    u32 totalSize = 0;
    Heap_EXPHBlockHeader *blockHeader;
    Heap_EXPHHeader *expHeader = AddU32_inline(heap, sizeof(Heap_Header));

    for (blockHeader = expHeader->freeBlocks.head; blockHeader; blockHeader = blockHeader->next) {
        totalSize += blockHeader->size;
    }

    return totalSize;
}

u32 Heap_EXPHGetMaxEmptySize(Heap_Header *heap, int alignment) {

    alignment = abs(alignment);

    {
        Heap_EXPHHeader *expHeader = AddU32_inline(heap, sizeof(Heap_Header));
        u32 maxSize                = 0;
        u32 offsetMin              = 0xFFFFFFFF;
        Heap_EXPHBlockHeader *blockHeader;

        for (blockHeader = expHeader->freeBlocks.head; blockHeader; blockHeader = blockHeader->next) {
            void *baseAddress =
                ((u32) AddU32_inline(blockHeader, sizeof(Heap_EXPHBlockHeader)) + (alignment - 1)) & ~(alignment - 1);

            if (baseAddress < AddU32_inline(AddU32_inline(blockHeader, sizeof(Heap_EXPHBlockHeader)), blockHeader->size)) {
                const u32 blockSize =
                    AddU32_inline(AddU32_inline(blockHeader, sizeof(Heap_EXPHBlockHeader)), blockHeader->size) - baseAddress;
                const u32 offset = baseAddress - AddU32_inline(blockHeader, sizeof(Heap_EXPHBlockHeader));

                if (maxSize < blockSize || (maxSize == blockSize && offsetMin > offset)) {
                    maxSize   = blockSize;
                    offsetMin = offset;
                }
            }
        }
        return maxSize;
    }
}

static inline Heap_FRMHHeader *Get_FRMHHeader_inline(Heap_Header *header) {
    return AddU32_inline(header, sizeof(Heap_Header));
}

Heap_Header *Heap_FRMHInit(void *startAddress, void *endAddress, u16 optFlag) {
    Heap_Header *header        = startAddress;
    Heap_FRMHHeader *frmHeader = (void *) header + sizeof(Heap_Header);

    func_0201776c(header, 'FRMH', (void *) frmHeader + sizeof(Heap_FRMHHeader), endAddress, optFlag);

    frmHeader->head  = header->startAddr;
    frmHeader->tail  = header->endAddr;
    frmHeader->state = NULL;

    return header;
}

void *Heap_FRMHAllocHead(Heap_FRMHHeader *header, u32 size, int alignment) {
    void *newBlock   = ((alignment - 1) + ((u32) header->head)) & ~(alignment - 1);
    void *endAddress = AddU32_inline(newBlock, size);

    if ((u32) endAddress > (u32) header->tail) {
        return NULL;
    }

    Fill32_inline((void *) ((u32) (header) - sizeof(Heap_Header)), header->head, endAddress - header->head);
    header->head = endAddress;

    return newBlock;
}

void *Heap_FRMHAllocTail(Heap_FRMHHeader *header, u32 size, int alignment) {
    void *newBlock = (u32) (header->tail - size) & ~(alignment - 1);

    if ((u32) (newBlock) < (u32) (header->head)) {
        return NULL;
    }

    Fill32_inline((void *) ((u32) (header) - sizeof(Heap_Header)), newBlock, header->tail - newBlock);
    header->tail = newBlock;

    return newBlock;
}

void Heap_FRMHFreeHead(Heap_Header *header) {
    Heap_FRMHHeader *frmHeader = (u32) header + sizeof(Heap_Header);
    frmHeader->head            = header->startAddr;
    frmHeader->state           = NULL;
}

void Heap_FRMHFreeTail(Heap_Header *header) {
    Heap_FRMHHeader *frmHeader = Get_FRMHHeader_inline(header);
    Heap_FRMHState *pState;

    for (pState = frmHeader->state; pState; pState = pState->prev) {
        pState->tail = header->endAddr;
    }

    frmHeader->tail = header->endAddr;
}

Heap_Header *Heap_FRMHCreate(void *startAddress, u32 size, u16 optFlag) {
    void *endAddress;
    Heap_Header *header;

    endAddress   = (size + (u32) startAddress) & ~3;
    startAddress = (3 + (u32) startAddress) & ~3;

    if ((u32) (startAddress) > (u32) (endAddress) ||
        endAddress - startAddress < sizeof(Heap_Header) + sizeof(Heap_FRMHHeader)) {
        return NULL;
    }

    header = Heap_FRMHInit(startAddress, endAddress, optFlag);
    return header;
}

void Heap_FRMHDestroy(Heap_Header *heap) {
    func_020177c8(heap);
}

void *Heap_FRMHNew(Heap_Header *heap, u32 size, int alignment) {
    void *memory;
    Heap_FRMHHeader *header;

    header = (void *) heap + sizeof(Heap_Header);

    if (size == 0) {
        size = 1;
    }

    size = size + 3 & ~3;

    if (alignment >= 0) {
        memory = Heap_FRMHAllocHead(header, size, alignment);
    } else {
        memory = Heap_FRMHAllocTail(header, size, -alignment);
    }

    return memory;
}

void Heap_FRMHFreeBlock(Heap_Header *heap, int mode) {
    if (mode & 1) {
        Heap_FRMHFreeHead(heap);
    }

    if (mode & 2) {
        Heap_FRMHFreeTail(heap);
    }
}

u32 Heap_FRMHGetMaxEmptySize(Heap_Header *heap, int alignment) {
    const Heap_FRMHHeader *header;
    const void *block;

    alignment = abs(alignment);
    header    = AddU32_inline(heap, sizeof(Heap_Header));
    block     = (alignment - 1) + (u32) header->head & ~(alignment - 1);

    if ((u32) block > (u32) header->tail) {
        return 0;
    }

    return header->tail - (u32) block;
}

u32 Heap_FRMHSaveState(Heap_Header *heap, u32 id) {
    Heap_FRMHHeader *header = Get_FRMHHeader_inline(heap);
    void *oldHead           = header->head;

    Heap_FRMHState *state = Heap_FRMHAllocHead(header, sizeof(Heap_FRMHState), 4);
    if (!state) {
        return 0;
    }

    state->id     = id;
    state->head   = oldHead;
    state->tail   = header->tail;
    state->prev   = header->state;
    header->state = state;

    return 1;
}

u32 Heap_FRMHLoadState(Heap_Header *heap, u32 id) {
    Heap_FRMHHeader *header = AddU32_inline(heap, sizeof(Heap_Header));
    Heap_FRMHState *state   = header->state;

    if (id != 0) {
        while (state) {
            if (state->id == id) {
                break;
            }
            state = state->prev;
        }
    }

    if (!state) {
        return 0;
    }

    header->head  = state->head;
    header->tail  = state->tail;
    header->state = state->prev;

    return 1;
}

u32 Heap_FRMHFreeEmpty(Heap_Header *heap) {
    Heap_Header *header        = heap;
    Heap_FRMHHeader *frmHeader = AddU32_inline(header, sizeof(Heap_Header));

    if (0 < ((u32) header->endAddr - (u32) frmHeader->tail)) {
        return 0;
    }

    frmHeader->tail = header->endAddr = frmHeader->head;

    return ((u32) header->endAddr - (u32) heap);
}