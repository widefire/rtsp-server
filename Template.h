#pragma once

#include <assert.h>
#include <memory.h>
#include <stdio.h>

#ifdef WIN32
#include <time.h>
#endif

inline void mswap(void *pDest, const void *pSrc, size_t iLen)
{
	assert(pDest);
	assert(pSrc);

	register size_t iLenMod4 = iLen&3;
	register size_t iLenDiv4 = iLen>>2;

	register unsigned long*srcDW = (unsigned long*)pSrc, *destDW = (unsigned long*)pDest;
	while(iLenDiv4--)
	{
		unsigned long dw = *destDW;
		*(destDW++) = *srcDW;
		*(srcDW++) = dw;
	}

	register unsigned char *srcB = (unsigned char*)srcDW, *destB = (unsigned char*)destDW;
	while(iLenMod4--)
	{
		unsigned char b = *destB;
		*(destB++) = *srcB;
		*(srcB++) = b;
	}
}

inline void zeromem(void *data,unsigned int size)
{
	memset(data,0,size);
}

template<typename T> class List
{
protected:
    T *array;
    unsigned int num;

public:

    inline List() : array(NULL), num(0) {}
    inline ~List()
    {
        Clear();
    }

    inline T* Array() const             {return array;}
    inline unsigned int Num() const     {return num;}

    inline unsigned int Add(const T& val)
    {
        array = (T*)realloc(array, sizeof(T)*++num);
        memcpy(&array[(num-1)], (void*)&val, sizeof(T));
        return num-1;
    }

    inline unsigned int SafeAdd(const T& val)
    {
        unsigned int i;
        for(i=0; i<num; i++)
        {
            if(array[i] == val)
                break;
        }

        return (i != num) ? i : Add(val);
    }

    inline void Insert(unsigned int index, const T& val)
    {
        assert(index <= num);
        if(index > num) return;

        if(!num && !index)
        {
            Add(val);
            return;
        }

        //this makes it safe to insert an item already in the list
        T *temp = (T*)malloc(sizeof(T));
        memcpy(temp, &val, sizeof(T));

        unsigned int moveCount = num-index;
        array = (T*)realloc(array, sizeof(T)*++num);
        if(moveCount)
            memmove(array+(index+1), array+index, moveCount*sizeof(T));
        memcpy(&array[index], temp, sizeof(T));

        free(temp);
    }

    inline void Remove(unsigned int index)
    {
        assert(index < num);
        if(index >= num) return;

        if(!--num) {free(array); array=NULL; return;}

        memcpy(&array[index], &array[index+1], sizeof(T)*(num-index));

        array = (T*)realloc(array, sizeof(T)*num);
    }

    inline void RemoveItem(const T& obj)
    {
        for(unsigned int i=0; i<num; i++)
        {
            if(array[i] == obj)
            {
                Remove(i);
                break;
            }
        }
    }

    inline void RemoveRange(unsigned int start, unsigned int end)
    {
        if(start > num || end > num || end <= start)
        {
            return;
        }

        unsigned int count = end-start;
        if(count == 1)
        {
            Remove(start);
            return;
        }
        else if(count == num)
        {
            Clear();
            return;
        }

        num -= count;

        unsigned int cutoffCount = num-start;
        if(cutoffCount)
            memcpy(array+start, array+end, cutoffCount*sizeof(T));
        array = (T*)realloc(array, num*sizeof(T));
    }

    inline void CopyArray(const T *new_array, unsigned int n)
    {
        if(!new_array && n)
        {
            return;
        }

        SetSize(n);

        if(!num) {array=NULL; return;}

        memcpy(array, (void*)new_array, sizeof(T)*num);
    }

    inline void InsertArray(unsigned int index, const T *new_array, unsigned int n)
    {
        assert(index <= num);

        if(index > num)
            return;

        if(!new_array && n)
        {
            return;
        }

        if(!n)
            return;

        int oldnum = num;

        SetSize(n+num);

        assert(num);

        if(!num) {array=NULL; return;}

        memmove(array+index+n, array+index, sizeof(T)*(oldnum-index));
        memcpy(array+index, new_array, sizeof(T)*n);
    }

    inline void AppendArray(const T *new_array, unsigned int n)
    {
        if(!new_array && n)
        {
            return;
        }

        if(!n)
            return;

        int oldnum = num;

        SetSize(n+num);

        assert(num);

        if(!num) {array=NULL; return;}

        memcpy(&array[oldnum], (void*)new_array, sizeof(T)*n);
    }

    inline int SetSize(unsigned int n)
    {
        if(num == n)
            return 0;
        else if(!n)
        {
            Clear();
            return 1;
        }

        int bClear=(n>num);
        unsigned int oldNum=num;

        num = n;
        array = (T*)realloc(array, sizeof(T)*num);

        if(bClear)
            zeromem(&array[oldNum], sizeof(T)*(num-oldNum));

        return 1;
    }

    inline void MoveItem(int id, int newID)
    {
        register int last = num-1;

        assert(id <= last);
        if(newID == id || id > last || newID > last)
            return;

        T val;
        memcpy(&val, array+id, sizeof(T));
        if(newID < id)
            memmove(array+newID+1, array+newID, (id-newID)*sizeof(T));
        else if(id < newID)
            memcpy(array+id, array+id+1, (newID-id)*sizeof(T));
        memcpy(array+newID, &val, sizeof(T));
        zeromem(&val, sizeof(T));
    }

    inline void MoveToFront(int id)
    {
        register int last = num-1;

        assert(id <= last);
        if(id > last || id == 0)
            return;

        T val;
        memcpy(&val, array+id, sizeof(T));
        memmove(array+1, array, id*sizeof(T));
        memcpy(array, &val, sizeof(T));
        zeromem(&val, sizeof(T));
    }

    inline void MoveToEnd(int id)
    {
        register int last = num-1;

        assert(id <= last);
        if(id >= last)
            return;

        T val;
        memcpy(&val, array+id, sizeof(T));
        memcpy(array+id, array+(id+1), (last-id)*sizeof(T)); //last-id = num-(id+1)
        memcpy(array+last, &val, sizeof(T));
        zeromem(&val, sizeof(T));
    }

    inline void RemoveDupes()
    {
        for(unsigned int i=0; i<num; i++)
        {
            T val1 = array[i];
            for(unsigned int j=i+1; j<num; j++)
            {
                T val2 = array[j];
                if(val1 == val2)
                {
                    Remove(j);
                    --j;
                }
            }
        }
    }

    inline void CopyList(const List<T>& list)
    {
        CopyArray(list.Array(), list.Num());
    }

    inline void InsertList(unsigned int index, const List<T>& list)
    {
        InsertArray(index, list.Array(), list.Num());
    }

    inline void AppendList(const List<T>& list)
    {
        AppendArray(list.Array(), list.Num());
    }

    inline void TransferFrom(List<T>& list)
    {
        if(array) Clear();
        array = list.Array();
        num   = list.Num();
        zeromem(&list, sizeof(List<T>));
    }

    inline void TransferFrom(T *arrayIn, unsigned int numIn)
    {
        if(array) Clear();
        array = arrayIn;
        num   = numIn;
    }

    inline void TransferTo(List<T>& list)
    {
        list.TransferFrom(*this);
    }

    inline void TransferTo(T *&arrayIn, unsigned int &numIn)
    {
        arrayIn = array;
        numIn = num;
        zeromem(this, sizeof(List<T>));
    }

    inline void Clear()
    {
        if(array)
        {
            free(array);
            array = NULL;
            num = 0;
        }
    }

    inline T* CreateNew()
    {
        SetSize(num+1);

        T *value = &array[num-1];

        return value;
    }

    inline T* InsertNew(int index)
    {
        T ins;

        zeromem(&ins, sizeof(T));

        Insert(index, ins);

        return &array[index];
    }

    inline List<T>& operator<<(const T& val)
    {
        Add(val);
        return *this;
    }

    inline T& GetElement(unsigned int index)
    {
        assert(index < num);
        if(index >= num) { return array[0];}
        return array[index];
    }

    inline T& operator[](unsigned int index)
    {
        assert(index < num);
        if(index >= num) { return array[0];}
        return array[index];
    }

    inline T& operator[](unsigned int index) const
    {
        assert(index < num);
        if(index >= num) { return array[0];}
        return array[index];
    }

    inline T* operator+(unsigned int index)
    {
        assert(index < num);
        if(index >= num) { return NULL;}
        return array+index;
    }

    inline T* operator+(unsigned int index) const
    {
        assert(index < num);
        if(index >= num) { return NULL;}
        return array+index;
    }

    inline T* operator-(unsigned int index)
    {
        assert(index < num);
        if(index >= num) { return NULL;}
        return array-index;
    }

    inline int HasValue(const T& val) const
    {
        for(unsigned int i=0; i<num; i++)
            if(array[i] == val) return 1;

        return 0;
    }

    inline unsigned int FindValueIndex(const T& val) const
    {
        for(unsigned int i=0; i<num; i++)
            if(array[i] == val) return i;

        return -1;
    }

    inline unsigned int FindNextValueIndex(const T& val, unsigned int lastIndex=-1) const
    {
        for(unsigned int i=lastIndex+1; i<num; i++)
            if(array[i] == val) return i;

        return -1;
    }

    inline void SwapValues(unsigned int valA, unsigned int valB)
    {
        assert(valA < num && valB < num);
        if(valA == valB || valA >= num || valB >= num)
            return;
        mswap(array+valA, array+valB, sizeof(T));
    }

    inline T& Last() const
    {
        assert(num);

        return array[num-1];
    }
};
