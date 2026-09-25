// ----------------------------------------------------------------------- //
//
// MODULE  : SpecialFXList.h
//
// PURPOSE : List of CSpecialFX class objects
//
// CREATED : 10/21/97
//
// ----------------------------------------------------------------------- //

#ifndef __SPECIAL_FX_LIST_H__
#define __SPECIAL_FX_LIST_H__

#include "ltlink.h"
#include "SpecialFX.h"

#define  DEFAULT_MAX_NUM	50
// THE HARD CEILING ON EVERY FX LIST, and the one that was actually deciding
// how many bullet holes and shell casings survive. The per-type table in
// SFXMgr.cpp asks for a number and Create() clamps it to this, so raising that
// table from 200 to 3000 changed nothing until this moved too.
//
// The list holds a POINTER and a uint32 age per slot - 4096 slots is 32 KB -
// and Add() walks the array once to age it, which at ten casings a second is
// not a cost worth protecting against. Lists that ask for 50 still get 50.
#define	 MAX_NUM_LINKS	  	4096

class CSpecialFXList
{
	public :

		int GetSize()		const { return m_nArraySize; }
		int GetNumItems()	const { return m_nElements; }
        LTBOOL IsEmpty()     const { return (LTBOOL)(m_nElements == 0); }

		CSpecialFXList()
		{
            m_nArraySize = LTNULL;
            m_pArray     = LTNULL;
            m_pAgeArray  = LTNULL;
			m_nElements  = 0;
		}

        LTBOOL Create(unsigned int nMaxNum=DEFAULT_MAX_NUM)
		{
            if (m_pArray) return LTFALSE;  // Already created

			m_nArraySize = nMaxNum < MAX_NUM_LINKS ? nMaxNum : MAX_NUM_LINKS;
			m_pArray = debug_newa(CSpecialFX*, m_nArraySize);
            if (!m_pArray) return LTFALSE;

            m_pAgeArray = debug_newa(uint32, m_nArraySize);
            if (!m_pAgeArray) return LTFALSE;

			m_nElements = 0;

			memset(m_pArray, 0, sizeof(CSpecialFX*)*m_nArraySize);
            memset(m_pAgeArray, 0, sizeof(uint32)*m_nArraySize);

            return LTTRUE;
		}

        CSpecialFX* operator[] (unsigned int nIndex)
		{
            if (!m_pArray || nIndex < 0 || nIndex >= m_nArraySize) return LTNULL;
			return m_pArray[nIndex];
		}

		~CSpecialFXList();

        LTBOOL Add(CSpecialFX* pFX);

        LTBOOL Remove(CSpecialFX* pFX);

	private :

		CSpecialFX**	m_pArray;		// Array of special fx
        uint32*         m_pAgeArray;    // Age special fx in array
		unsigned int	m_nArraySize;	// Size of array
		unsigned int	m_nElements;	// Number of elements in array
};

#endif // __SPECIAL_FX_LIST_H__