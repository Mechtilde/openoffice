/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/



// MARKER(update_precomp.py): autogen include statement, do not remove
#include "precompiled_connectivity.hxx"
#include "TSortIndex.hxx"
#include <algorithm>
#include <functional>

using namespace connectivity;
//------------------------------------------------------------------
/// binary_function Functor object for class OSortIndex::TIntValuePairVector::value_type returntype is bool
struct TKeyValueFunc : ::std::binary_function<OSortIndex::TIntValuePairVector::value_type,OSortIndex::TIntValuePairVector::value_type,bool>
{
	OSortIndex* pIndex;

	TKeyValueFunc(OSortIndex* _pIndex) : pIndex(_pIndex)
	{
	}
	// return false if compared values are equal otherwise true
	inline bool operator()(const OSortIndex::TIntValuePairVector::value_type& lhs,const OSortIndex::TIntValuePairVector::value_type& rhs)	const
	{
		const ::std::vector<OKeyType>& aKeyType = pIndex->getKeyType();
		::std::vector<OKeyType>::const_iterator aIter = aKeyType.begin();
		for (::std::vector<sal_Int16>::size_type i=0;aIter != aKeyType.end(); ++aIter,++i)
		{
			const bool nGreater = (pIndex->getAscending(i) == SQL_ASC) ? false : true;
			const bool nLess = !nGreater;

			// compare depending for type
			switch (*aIter)
			{
				case SQL_ORDERBYKEY_STRING:
				{
					sal_Int32 nRes = lhs.second->getKeyString(i).compareTo(rhs.second->getKeyString(i));
					if (nRes < 0)
						return nLess;
					else if (nRes > 0)
						return nGreater;
				}
				break;
				case SQL_ORDERBYKEY_DOUBLE:
				{
					double d1 = lhs.second->getKeyDouble(i);
					double d2 = rhs.second->getKeyDouble(i);

					if (d1 < d2)
						return nLess;
					else if (d1 > d2)
						return nGreater;
				}
				break;
                case SQL_ORDERBYKEY_NONE:
                    break;
			}
		}

		// know we know that the values are equal
		return false;
	}
};

// -----------------------------------------------------------------------------
::vos::ORef<OKeySet> OSortIndex::CreateKeySet()
{
	Freeze();

	::vos::ORef<OKeySet> pKeySet = new OKeySet();
	pKeySet->get().reserve(m_aKeyValues.size());
	::std::transform(m_aKeyValues.begin()
					,m_aKeyValues.end()
                    ,::std::back_inserter(pKeySet->get())
					,::std::select1st<TIntValuePairVector::value_type>());
	pKeySet->setFrozen();
	return pKeySet;
}
// -----------------------------------------------------------------------------
OSortIndex::OSortIndex(	const ::std::vector<OKeyType>& _aKeyType,
                        const ::std::vector<TAscendingOrder>& _aAscending)
	:m_aKeyType(_aKeyType)
    ,m_aAscending(_aAscending)
    ,m_bFrozen(sal_False)
{
}
//------------------------------------------------------------------
OSortIndex::~OSortIndex()
{
}
//------------------------------------------------------------------
void OSortIndex::AddKeyValue(OKeyValue * pKeyValue)
{
	OSL_ENSURE(pKeyValue,"Can not be null here!");
	if(m_bFrozen)
	{
		m_aKeyValues.push_back(TIntValuePairVector::value_type(pKeyValue->getValue(),NULL));
		delete pKeyValue;
	}
	else
		m_aKeyValues.push_back(TIntValuePairVector::value_type(pKeyValue->getValue(),pKeyValue));
}


//------------------------------------------------------------------
void OSortIndex::Freeze()
{
	OSL_ENSURE(! m_bFrozen,"OSortIndex::Freeze: already frozen!");
	// Sortierung:
	if (m_aKeyType[0] != SQL_ORDERBYKEY_NONE)
		// we will sort ourself when the first keyType say so
		::std::sort(m_aKeyValues.begin(),m_aKeyValues.end(),TKeyValueFunc(this));

	TIntValuePairVector::iterator aIter = m_aKeyValues.begin();
	for(;aIter != m_aKeyValues.end();++aIter)
	{
		delete aIter->second;
		aIter->second = NULL;
	}

	m_bFrozen = sal_True;
}

//------------------------------------------------------------------
sal_Int32 OSortIndex::GetValue(sal_Int32 nPos) const
{
	OSL_ENSURE(nPos > 0,"OSortIndex::GetValue: nPos == 0");
	OSL_ENSURE((size_t)nPos <= m_aKeyValues.size(),"OSortIndex::GetValue: Zugriff ausserhalb der Array-Grenzen");

	if (!m_bFrozen && m_aKeyType[0] != SQL_ORDERBYKEY_NONE)
	{
		OSL_ASSERT("OSortIndex::GetValue: Invalid use of index!");
		return 0;
	}
	return m_aKeyValues[nPos-1].first;
}
// -----------------------------------------------------------------------------
OKeyValue::OKeyValue()
{
}
// -----------------------------------------------------------------------------
OKeyValue::OKeyValue(sal_Int32 nVal)
: m_nValue(nVal)
{
}
// -----------------------------------------------------------------------------
OKeyValue::~OKeyValue()
{
}
// -----------------------------------------------------------------------------
OKeyValue* OKeyValue::createKeyValue(sal_Int32 _nVal)
{
	return new OKeyValue(_nVal);
}
// -----------------------------------------------------------------------------

