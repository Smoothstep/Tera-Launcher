#ifndef __ALLOCATION__H_
#define __ALLOCATION__H_

#include <cstdlib>
#include <vector>

extern void* Allocate(size_t size);
extern void* Allocate(void* opaque, size_t items, size_t size);
extern void Deallocate(void* opaque, void* p);

extern void SetAllocationLimit(size_t size);

extern size_t GetAllocationLimit();
extern size_t GetAllocationSize();

extern size_t g_iAllocated;
extern size_t g_iLimit;

template<typename T>
class MEMVector : public std::vector<T>
{
public:
	inline bool resize(size_t size);
	inline bool reserve(size_t size);

	inline void clear();

	~MEMVector();
};

template<typename T>
inline bool MEMVector<T>::resize(size_t size)
{
	if (g_iAllocated + size > g_iLimit)
	{
		return false;
	}

	try
	{
		std::vector<T>::reserve(size);
	}
	catch (...)
	{
		return false;
	}

	this->_Myfirst()= this->data();
	this->_Mylast()	= this->data() + this->capacity();
	this->_Myend()	= this->data() + this->capacity();

	g_iAllocated += size;

	return true;
}

template<typename T>
inline bool MEMVector<T>::reserve(size_t size)
{
	if (g_iAllocated + size > g_iLimit)
	{
		return false;
	}

	try
	{
		std::vector<T>::reserve(size);
	}
	catch (...)
	{
		return false;
	}

	g_iAllocated += size;

	return true;
}

template<typename T>
inline void MEMVector<T>::clear()
{
	g_iAllocated -= std::vector<T>::size();
	std::vector<T>::clear();
}

template<typename T>
inline MEMVector<T>::~MEMVector()
{
	g_iAllocated -= std::vector<T>::size();
}

#endif