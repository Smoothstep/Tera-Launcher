
#include "allocation.h"

extern size_t g_iAllocated = 0;
extern size_t g_iLimit = 1073741824;

void * Allocate(size_t size)
{
	if (g_iAllocated + size > g_iLimit)
	{
		return NULL;
	}

	void *p = malloc(size);

	if (p)
	{
		g_iAllocated += size;
	}

	return p;
}

void * Allocate(void * opaque, size_t items, size_t size)
{
	size_t allocSize = items * size;

	if (!opaque)
	{
		if (g_iAllocated + allocSize > g_iLimit)
		{
			return NULL;
		}

		void *p = malloc(allocSize);

		if (p)
		{
			g_iAllocated += allocSize;
		}

		return p;
	}

	void *p = malloc(allocSize);

	return p;
}

void Deallocate(void * opaque, void * p)
{
	if (p)
	{
		free(p);
	}
}

void SetAllocationLimit(size_t size)
{
	g_iLimit = size;
}

size_t GetAllocationLimit()
{
	return g_iLimit;
}

size_t GetAllocationSize()
{
	return g_iAllocated;
}
