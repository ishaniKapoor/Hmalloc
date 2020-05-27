#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

#include "hmalloc.h"

/*
  typedef struct hm_stats {
  long pages_mapped;
  long pages_unmapped;
  long chunks_allocated;
  long chunks_freed;
  long free_length;
  } hm_stats;
*/

typedef struct free_list
{
  size_t size;
  struct free_list* next;
} free_list;

static free_list* head;
const size_t PAGE_SIZE = 4096;
static hm_stats stats; // This initializes the stats to 0.


long
free_list_length()
{
    // TODO: Calculate the length of the free list.
    long len = 0;
    free_list* current_node = head; //start from the head
    while (current_node != 0)
    {
      len += 1;
      current_node = current_node->next;
    }
    return len;
}

hm_stats*
hgetstats()
{
    stats.free_length = free_list_length();
    return &stats;
}

void
hprintstats()
{
    stats.free_length = free_list_length();
    fprintf(stderr, "\n== husky malloc stats ==\n");
    fprintf(stderr, "Mapped:   %ld\n", stats.pages_mapped);
    fprintf(stderr, "Unmapped: %ld\n", stats.pages_unmapped);
    fprintf(stderr, "Allocs:   %ld\n", stats.chunks_allocated);
    fprintf(stderr, "Frees:    %ld\n", stats.chunks_freed);
    fprintf(stderr, "Freelen:  %ld\n", stats.free_length);
}

static
size_t
div_up(size_t xx, size_t yy)
{
    // This is useful to calculate # of pages
    // for large allocations.
    size_t zz = xx / yy;

    if (zz * yy == xx) {
        return zz;
    }
    else {
        return zz + 1;
    }
}

void
coalesce_helper(free_list* n)
{
  if (head == 0)
  {
    head = n;
    return;
  }

  free_list* current_node = head;
  free_list* prev_node = 0;
  while (current_node != 0)
  {
    if ((void*) current_node > (void*) n)
    {
      size_t prev_size = 0;
      if (prev_node != 0)
      {
        prev_size = prev_node->size;
      }

      if (((void*) prev_node + prev_size == (void*) n) && ((void*) n + n->size == (void*) current_node))
      {
        prev_node->size = prev_node->size + n->size + current_node->size;        prev_node->next = current_node->next;

      }
      else if ((void*) prev_node + prev_size == (void*) n)
      {
        prev_node->size = prev_node->size + n->size;
      }
      else if ((void*) n + n->size == (void*) current_node)
      {
        n->size = n->size + current_node->size;

	if (prev_node != 0)
	{
          prev_node->next = n;
	}
	n->next = current_node->next;
      }
      else
      {
        if (prev_node != 0)
	{
          prev_node->next = n;
	}
	n->next = current_node;
      }

      if (prev_node == 0)
      {
        head = n;
      }
      break;
    }

    prev_node = current_node;
    current_node = current_node->next;
  }
}


void*
hmalloc(size_t size)
{
    stats.chunks_allocated += 1;
    size += sizeof(size_t);

    // TODO: Actually allocate memory with mmap and a free list.
    if (size < PAGE_SIZE)
    {
      free_list* block = 0;
      free_list* current_node = head;
      free_list* prev_node = 0;

      while(current_node != 0)
      {
        if (current_node->size >= size)
	{
          block = current_node;
          if (prev_node != 0)
	  {
            prev_node->next = current_node->next;
	  } 
	  else
	  {
           head = current_node->next;
	  }
	  break;
	}
        prev_node = current_node;
	current_node = current_node->next;
      }

      if (block == 0)
      {
         block = mmap(0, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
         assert(block != MAP_FAILED);
         stats.pages_mapped += 1;
	 block->size = PAGE_SIZE;
      }

      if ((block->size > size) && (block->size - size >= sizeof(free_list)))
      {
        void* add = (void*) block + size;
	free_list* new_node = (free_list*) add;
	new_node->size = block->size - size;
        coalesce_helper(new_node);
        block->size = size;
      }
      return (void*) block + sizeof(size_t);
    } 
    else
    {
      int page_no = (size + PAGE_SIZE - 1) /PAGE_SIZE;
      size_t size = page_no * PAGE_SIZE;
      free_list* block = mmap(0, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
      assert(block != MAP_FAILED);
      stats.pages_mapped += page_no;

      block->size = size;
      return (void*) block + sizeof(size_t);
    }
}



void
hfree(void* item)
{
    stats.chunks_freed += 1;

    // TODO: Actually free the item.

    free_list* block = (free_list*) (item - sizeof(size_t));

    if (block->size < PAGE_SIZE)
    {
      coalesce_helper(block);
    } 
    else
    {
      int page_no = (block->size + PAGE_SIZE - 1) / PAGE_SIZE;
      int rv = munmap((void*) block, block->size);
      assert(rv != -1);
      stats.pages_unmapped += page_no;
    }
}
