// very simple heap allocation
// todo (maybe):
// - guard marks around blocks
// - linked list
//
// by oggzee

// note:
// some regions of MEM2 get reset during IOS reload
// MEM1 will be reset before game load

#include <ogcsys.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>

#include "mem.h"
#include "util.h" // required for memcheck

static heap mem1;
static heap mem2;
static void *mem2_start = NULL;

mem_blk* blk_find_size(blk_list *bl, int size)
{
	int i;
	for (i=0; i < bl->num; i++) {
		if (size <= bl->list[i].size) {
			return &bl->list[i];
		}
	}
	return NULL;
}

mem_blk* blk_find_ptr(blk_list *bl, void *ptr)
{
	int i;
	for (i=0; i < bl->num; i++) {
		if (ptr == bl->list[i].ptr) {
			return &bl->list[i];
		}
	}
	return NULL;
}

mem_blk* blk_find_ptr_end(blk_list *bl, void *ptr)
{
	int i;
	for (i=0; i < bl->num; i++) {
		if (ptr == bl->list[i].ptr + bl->list[i].size) {
			return &bl->list[i];
		}
	}
	return NULL;
}

// find first blk that is after ptr
// (so that we can insert before it to keep blocks sorted)
mem_blk* blk_find_ptr_after(blk_list *bl, void *ptr)
{
	int i;
	for (i=0; i < bl->num; i++) {
		if (ptr > bl->list[i].ptr) {
			return &bl->list[i];
		}
	}
	if (bl->num >= MAX_MEM_BLK) return NULL;
	return &bl->list[bl->num]; // return one after last
}

// inser before blk b
mem_blk* blk_insert(blk_list *bl, mem_blk *b)
{
	int i, n;
	if (!b) return NULL;
	if (bl->num >= MAX_MEM_BLK) {
		// list full
		// fatal error
		return NULL;
	}
	i = b - bl->list; // index of b in list
	if (i < 0 || i > bl->num) {
		// out of range
		// fatal error
		return NULL;
	}
	n = bl->num - i; // num of blks after b
	bl->num++;
	if (n) {
		// move blks from b up
		memmove(b+1, b, n*sizeof(mem_blk));
	}
	// clear current one (now unused)
	memset(b, 0, sizeof(mem_blk));
	return b;
}

void blk_remove(blk_list *bl, mem_blk *b)
{
	int i, n;
	if (!b) {
		// fatal error
		return;
	}
	if (bl->num == 0) {
		// fatal error
		return;
	}
	i = b - bl->list; // index of b in list
	if (i < 0 || i >= bl->num) {
		// out of range
		// fatal error
		return;
	}
	bl->num--;
	n = bl->num - i; // num of blks after b
	if (n) {
		// move blks after b down
		memmove(b, b+1, n*sizeof(mem_blk));
		// clear last one (now unused)
		memset(b+n, 0, sizeof(mem_blk));
	}
}

mem_blk* blk_merge_add(blk_list *list, mem_blk *ab)
{
	mem_blk *fb;
	// try to extend start of an existing free block
	fb = blk_find_ptr(list, ab->ptr + ab->size);
	if (fb) {
		fb->ptr -= ab->size;
		fb->size += ab->size;
	} else {
		// try to extend end of an existing free block
		fb = blk_find_ptr_end(list, ab->ptr);
		if (fb) {
			fb->size += ab->size;
		} else {
			// insert a new free block
			fb = blk_find_ptr_after(list, ab->ptr);
			if (!fb) {
				// fatal
				return NULL;
			}
			fb = blk_insert(list, fb);
			if (!fb) {
				// fatal
				return NULL;
			}
			*fb = *ab;
		}
	}
	return fb;
}

void *heap_alloc(heap *h, int size)
{
	mem_blk *ab, *fb;
	// round up to 32bit (4byte) aligned size
	if (size == 0) size = 4;
	size = ((size+3) >> 2) << 2;
	if (h->used_list.num >= MAX_MEM_BLK - 1) return NULL;
	fb = blk_find_size(&h->free_list, size);
	if (!fb) return NULL;
	ab = &h->used_list.list[ h->used_list.num ];
	h->used_list.num++;
	ab->ptr  = fb->ptr;
	ab->size = size;
	fb->ptr  += size;
	fb->size -= size;
	if (fb->size == 0) {
		blk_remove(&h->free_list, fb);
	}
	memset(ab->ptr, 0, size);
	return ab->ptr;
}

int heap_free(heap *h, void *ptr)
{
	mem_blk *ab, *fb;
	if (!ptr) return 0;
	ab = blk_find_ptr(&h->used_list, ptr);
	if (!ab) return -1;
	// try to extend start of an existing free block
	fb = blk_merge_add(&h->free_list, ab);
	if (!fb) {
		// fatal
		return -1;
	}
	blk_remove(&h->used_list, ab);
	return 0;
}

void *heap_realloc(heap *h, void *ptr, int size)
{
	mem_blk *ab, *fb, bb;
	void *new_ptr;
	int delta;

	// round up to 32bit (4byte) aligned size
	if (size == 0) size = 4;
	size = ((size+3) >> 2) << 2;
	
	// new allocation
	if (ptr == NULL) {
		return heap_alloc(h, size);
	}

	// find existing
	ab = blk_find_ptr(&h->used_list, ptr);
	if (!ab) {
		// fatal
		return NULL;
	}

	// size equal? - do nothing
	if (size == ab->size) {
		return ptr;
	}

	// size larger?
	if (size > ab->size) {
		delta = size - ab->size;
		// find a free block at the end
		fb = blk_find_ptr(&h->free_list, ptr + ab->size);
		if (fb && fb->size >= delta ) {
			// extend
			memset(fb->ptr, 0, delta);
			fb->ptr  += delta;
			fb->size -= delta;
			ab->size = size;
			if (fb->size == 0) {
				blk_remove(&h->free_list, fb);
			}
			return ptr;
		}
		// can't extend
		new_ptr = heap_alloc(h, size);
		if (!new_ptr) {
			return NULL;
		}
		memcpy(new_ptr, ptr, ab->size);
		memset(new_ptr + ab->size, 0, delta);
		heap_free(h, ptr);
		return new_ptr;
	}

	// size smaller
	// insert or merge a free block
	bb.ptr  = ab->ptr + size;
	bb.size = ab->size - size;
	fb = blk_merge_add(&h->free_list, &bb);
	if (!fb) {
		// fatal
		return NULL;
	}
	ab->size = size;
	return ptr;
}

void heap_init(heap *h, void *ptr, int size)
{
	int d;
	// init
	memset(h, 0, sizeof(heap));
	h->start = ptr;
	h->size = size;
	// round to 4 bytes
	d = ((((unsigned)ptr + 3) >> 2) << 2) - (unsigned)ptr;
	ptr += d;
	size -= d;
	size = (size >> 2) << 2;
	h->free_list.num = 1;
	h->free_list.list[0].ptr = ptr;
	h->free_list.list[0].size = size;
}

int heap_ptr_inside(heap *h, void *ptr)
{
	return (ptr >= h->start && ptr < h->start + h->size);
}

void heap_stat(heap *h, heap_stats *s)
{
	int i;
	void *ptr;
	memset(s, 0, sizeof(heap_stats));
	s->highptr = h->start;
	for (i=0; i<h->used_list.num; i++) {
		s->used += h->used_list.list[i].size;
		ptr = h->used_list.list[i].ptr + h->used_list.list[i].size;
		if (ptr > s->highptr) s->highptr = ptr;
	}
	for (i=0; i<h->free_list.num; i++) {
		s->free += h->free_list.list[i].size;
	}
	s->size = s->used + s->free;
}

void mem_init()
{
	void *m1_start = (void*)0x80004000;
	void *m1_end   = (void*)0x80a00000;
	void *m2_start;
	int   m2_size;
	m2_size = SYS_GetArena2Hi() - SYS_GetArena2Lo();
	// leave 512k of mem2 (sys_arena2) free
	// 211k will be used by wpad, 300k remains just in case...
	m2_size = (m2_size >> 2 << 2) - 512*1024;
	m2_start = SYS_AllocArena2MemLo(m2_size, 32);
	heap_init(&mem1, m1_start, m1_end - m1_start);
	heap_init(&mem2, m2_start, m2_size);
}

void *mem1_alloc(int size)
{
	return heap_alloc(&mem1, size);
}

void *mem2_alloc(int size)
{
	return heap_alloc(&mem2, size);
}

void *mem_alloc(int size)
{
	void *ptr;
	// round up to 32bit (4byte) aligned size
	if (size == 0) size = 4;
	size = ((size+3) >> 2) << 2;
	// mem2
	ptr = mem2_alloc(size);
	if (ptr) return ptr;
	// mem1
	ptr = mem1_alloc(size);
	if (ptr) return ptr;
	// sys
	ptr = memalign(32, size);
	memset(ptr, 0, size);
	return ptr;
}

// defaults to MEM2
void *mem_realloc(void *ptr, int size)
{
	// round up to 32bit (4byte) aligned size
	if (size == 0) size = 4;
	size = ((size+3) >> 2) << 2;
	// first time
	if (ptr == NULL) {
		return mem_alloc(size);
	}
	// mem2
	if (heap_ptr_inside(&mem2, ptr)) {
		return heap_realloc(&mem2, ptr, size);
	}
	// mem1
	if (heap_ptr_inside(&mem1, ptr)) {
		return heap_realloc(&mem1, ptr, size);
	}
	// sys
	// note, this one doesn't clean up newly allocated
	// part of mem while the above do
	return realloc(ptr, size);
}

// defaults to MEM1
void *mem1_realloc(void *ptr, int size)
{
	if (ptr) return mem_realloc(ptr, size);
	return heap_alloc(&mem1, size);
}

void mem_free(void *ptr)
{
	if (ptr == NULL) return;
	// mem2
	if (heap_ptr_inside(&mem2, ptr)) {
		heap_free(&mem2, ptr);
		return;
	}
	// mem1
	if (heap_ptr_inside(&mem1, ptr)) {
		heap_free(&mem1, ptr);
		return;
	}
	// sys
	free(ptr);
}

#if 0
struct mallinfo {
  int arena;    /* total space allocated from system */
  int ordblks;  /* number of non-inuse chunks */
  int smblks;   /* unused -- always zero */
  int hblks;    /* number of mmapped regions */
  int hblkhd;   /* total space in mmapped regions */
  int usmblks;  /* unused -- always zero */
  int fsmblks;  /* unused -- always zero */
  int uordblks; /* total allocated space */
  int fordblks; /* total non-inuse space */
  int keepcost; /* top-most, releasable (via malloc_trim) space */
};   
#endif
void print_mallinfo()
{
	struct mallinfo m = mallinfo();
	#define PM(X) printf("%8s : %d\n", #X, m.X)
	PM(arena);
	PM(ordblks);
	PM(smblks);
	PM(hblks);
	PM(hblkhd);
	PM(usmblks);
	PM(fsmblks);
	PM(uordblks);
	PM(fordblks);
	PM(keepcost);
	#undef PM
}

void mem_stat_str(char * buffer)
{
	heap_stats hs1, hs2;
	heap_stat(&mem1, &hs1);
	heap_stat(&mem2, &hs2);
	sprintf(buffer, "\n");
	#define fMB (1024.0 * 1024.0)
	sprintf(buffer, "%smem1: s:%5.2f u:%5.2f f:%5.2f t:%d,%d\n", buffer,
			hs1.size / fMB,
			hs1.used / fMB,
			hs1.free / fMB,
			mem1.used_list.num, mem1.free_list.num);
	sprintf(buffer, "%smem2: s:%5.2f u:%5.2f f:%5.2f t:%d,%d\n", buffer,
			hs2.size / fMB,
			hs2.used / fMB,
			hs2.free / fMB,
			mem2.used_list.num, mem2.free_list.num);
	sprintf(buffer, "%sm1+2: s:%5.2f u:%5.2f f:%5.2f\n", buffer,
			(hs1.size+hs2.size) / fMB,
			(hs1.used+hs2.used) / fMB,
			(hs1.free+hs2.free) / fMB);
	void *p;
	int size;
	struct mallinfo m = mallinfo();
	for (size = 10*1024*1024; size > 0; size -= 16*1024) {
		p = memalign(32, size);
		if (p) {
			m = mallinfo();
			free(p);
			break;
		}
	}
	sprintf(buffer, "%ssys1: s:%5.2f u:%5.2f f:%5.2f mx:%.2f\n", buffer,
			m.arena / fMB,
			(m.uordblks-size) / fMB,
			(m.fordblks+size) / fMB,
			size / fMB);
	sprintf(buffer, "%stotl: s:%5.2f u:%5.2f f:%5.2f\n", buffer,
			(hs1.size+hs2.size + m.arena) / fMB,
			(hs1.used+hs2.used + m.uordblks-size) / fMB,
			(hs1.free+hs2.free + m.fordblks+size) / fMB);
	//printf("max malloc: %.2f\n", (float)size/1024/1024);
}

void mem_stat()
{
	char buffer[1000];
	mem_stat_str(buffer);
	printf("\n%s", buffer);

	/*
	printf("\n");
	printf("slot1: t:%d u:%d f:%d\n", MAX_MEM_BLK, mem1.used_list.num, mem1.free_list.num);
	printf("slot2: t:%d u:%d f:%d\n", MAX_MEM_BLK, mem2.used_list.num, mem2.free_list.num);
	printf("adr1: %p-%p h:%p\n", mem1.start, mem1.start+mem1.size, hs1.highptr);
	printf("adr2: %p-%p h:%p\n", mem2.start, mem2.start+mem2.size, hs2.highptr);
	memstat2();
	*/
	
}


// moved from util.c:


void* LARGE_memalign(size_t align, size_t size)
{
	return mem_alloc(size);
}

void* LARGE_alloc(size_t size)
{
	return mem_alloc(size);
}

void* LARGE_realloc(void *ptr, size_t size)
{
	return mem_realloc(ptr, size);
}

void LARGE_free(void *ptr)
{
	return mem_free(ptr);
}

/*
size_t LARGE_used()
{
	size_t size = SYS_GetArena2Lo() - (void*)0x90000000;
	return size;
}
*/

void memstat2()
{
	void *m2base = (void*)0x90000000;
	void *m2lo = SYS_GetArena2Lo();
	void *m2hi = SYS_GetArena2Hi();
	void *ipclo = __SYS_GetIPCBufferLo();
	void *ipchi = __SYS_GetIPCBufferHi();
	size_t isize = ipchi - ipclo;
	printf("\n");
	printf("MEM2: %p %p %p\n", m2base, m2lo, m2hi);
	printf("s:%d u:%d f:%d\n", m2hi - m2base, m2lo - m2base, m2hi - m2lo);
	printf("s:%.2f MB u:%.2f MB f:%.2f MB\n",
			(float)(m2hi - m2base)/1024/1024,
			(float)(m2lo - m2base)/1024/1024,
			(float)(m2hi - m2lo)/1024/1024);
	printf("IPC:  %p %p %d\n", ipclo, ipchi, isize);
}

// save M2 ptr
void util_init()
{
	void _con_alloc_buf(s32 *conW, s32 *conH);
	_con_alloc_buf(NULL, NULL);
	//mem2_start = SYS_GetArena2Lo();
	heap_stats hs;
	heap_stat(&mem2, &hs);
	mem2_start = hs.highptr;
}

void util_clear()
{
	// game start: 0x80004000
	// game end:   0x80a00000 approx
	void *game_start = (void*)0x80004000;
	void *game_end   = (void*)0x80a00000;
	u32 size;

	// clear mem1 main
	size = game_end - game_start;
	//printf("Clear %p [%x]\n", game_start, size); __console_flush(0);
	memset(game_start, 0, size);
	DCFlushRange(game_start, size);

	// clear mem2
	if (mem2_start == NULL) return;
	size = SYS_GetArena2Lo() - mem2_start;
	//printf("Clear %p [%x]\n", mem2_start, size); __console_flush(0); sleep(2);
	memset(mem2_start, 0, size);
	DCFlushRange(mem2_start, size);

	// clear mem1 heap
	// find appropriate size
	void *p;
	for (size = 10*1024*1024; size > 0; size -= 128*1024) {
		p = memalign(32, size);
		if (p) {
			//printf("Clear %p [%x] %p\n", p, size, p+size);
			//__console_flush(0); sleep(2);
			memset(p, 0, size);
			DCFlushRange(p, size);
			free(p);
			break;
		}
	}
}

