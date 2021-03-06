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



#ifndef _POLYGON_POINT_HXX
#define _POLYGON_POINT_HXX

#include <vector>

//////////////////////////////////////////////////////////////////////////////

template < class Point > class SimplePointEntry
{
	Point											maPoint;

public:
	SimplePointEntry()
	:	maPoint(Point::getEmptyPoint())
	{
	}

	SimplePointEntry(const Point& rInitPoint)
	:	maPoint(rInitPoint)
	{
	}

	const Point& getPoint() const
	{
		return maPoint;
	}
	
	void setPoint(const Point& rValue)
	{
		maPoint = rValue;
	}

	bool operator==(const SimplePointEntry& rEntry) const
	{
		return (maPoint == rEntry.maPoint);
	}
};

//////////////////////////////////////////////////////////////////////////////

template < class Vector > class SimpleBezierEntry
{
	Vector											maBackward;
	Vector											maForward;

public:
	SimpleBezierEntry()
	:	maBackward(Vector::getEmptyVector()),
		maForward(Vector::getEmptyVector())
	{
	}

	SimpleBezierEntry(const Vector& rInitBackward, const Vector& rInitForward)
	:	maBackward(rInitBackward),
		maForward(rInitForward)
	{
	}

	const Vector& getBackwardVector() const
	{
		return maBackward;
	}
	
	void setBackwardVector(const Vector& rValue)
	{
		maBackward = rValue;
	}

	const Vector& getForwardVector() const
	{
		return maForward;
	}

	void setForwardVector(const Vector& rValue)
	{
		maForward = rValue;
	}

	bool isBezierNeeded()
	{
		if(maBackward != Vector::getEmptyVector() || maForward != Vector::getEmptyVector())
			return true;
		return false;
	}

	bool operator==(const SimpleBezierEntry& rEntry) const
	{
		return ((maBackward == rEntry.maBackward) && (maForward == rEntry.maForward));
	}

	void doInvertForFlip()
	{
		maBackward = -maBackward;
		maForward = -maForward;
	}
};

//////////////////////////////////////////////////////////////////////////////

template < class Point, class Vector > class PolygonPointList
{
	typedef SimplePointEntry< Point > LocalSimplePointEntry;
	typedef SimpleBezierEntry< Vector > LocalSimpleBezierEntry;
	typedef ::std::vector< LocalSimplePointEntry > SimplePointVector;
	typedef ::std::vector< LocalSimpleBezierEntry > SimpleBezierVector;

	sal_uInt32										mnBezierCount;
	SimplePointVector								maPoints;
	SimpleBezierVector*								mpVectors;

	unsigned										mbIsClosed : 1;

	void implTryToReduceToPointVector()
	{
		if(!mnBezierCount && mpVectors)
		{
			delete mpVectors;
			mpVectors = 0L;
		}
	}

public:
	bool isBezier() const
	{
		return bool(mnBezierCount);
	}

	bool isClosed() const
	{
		return bool(mbIsClosed);
	}

	void setClosed(bool bNew)
	{
		mbIsClosed = bNew;
	}

	sal_uInt32 count() const
	{
		return maPoints.size();
	}

	PolygonPointList()
	:	mnBezierCount(0L),
		mpVectors(0L),
		mbIsClosed(false)
	{
		// complete initialization with defaults
	}

	PolygonPointList(const PolygonPointList& rSource)
	:	mnBezierCount(0L),
		maPoints(rSource.maPoints),
		mpVectors(0L),
		mbIsClosed(rSource.mbIsClosed)
	{
		// complete initialization using copy
		if(rSource.mpVectors && rSource.mnBezierCount)
		{
			mpVectors = new SimpleBezierVector(*rSource.mpVectors);
			mnBezierCount = rSource.mnBezierCount;
		}
	}

	PolygonPointList(const PolygonPointList& rSource, sal_uInt32 nIndex, sal_uInt32 nCount)
	:	mnBezierCount(0L),
		maPoints(nCount),
		mpVectors(0L),
		mbIsClosed(rSource.mbIsClosed)
	{
		// complete initialization using partly copy
		if(nCount)
		{
			// copy point data
			{
				SimplePointVector::const_iterator aStart(rSource.maPoints.begin());
				aStart += nIndex;
				SimplePointVector::const_iterator aEnd(aStart);
				aEnd += nCount;
				maPoints.insert(0L, aStart, aEnd);
			}

			// copy bezier data
			if(rSource.mpVectors && rSource.mnBezierCount)
			{
				mpVectors = new SimpleBezierVector();
				mpVectors->reserve(nCount);

				SimpleBezierVector::iterator aStart(mpVectors->begin());
				aStart += nIndex;
				SimpleBezierVector::iterator aEnd(aStart);
				aEnd += nCount;

				for( ; aStart != aEnd; ++aStart )
				{
					if(aStart->IsBezierNeeded())
					{
						mnBezierCount++;
					}

					mpVectors->push_back(*aStart);
				}

				// maybe vectors are not needed anymore, try to reduce memory footprint
				implTryToReduceToPointVector();
			}
		}
	}

	~PolygonPointList()
	{
		if(mpVectors)
		{
			delete mpVectors;
		}
	}

	bool isEqual(const PolygonPointList& rPointList) const
	{
		// same point count?
		if(maPoints.size() != rPointList.maPoints.size())
			return false;

		// if zero points the polys are equal
		if(!maPoints.size())
			return true;

		// if bezier count used it needs to be equal
		if(mnBezierCount != rPointList.mnBezierCount)
			return false;

		// compare point content
		if(maPoints != rPointList.maPoints)
			return false;

		// beziercounts are equal: if it's zero, we are done
		if(!mnBezierCount)
			return true;

		// beziercounts are equal and not zero; compare them
		OSL_ENSURE(0L != mpVectors, "Error: Bezier list needs to exist here(!)");
		OSL_ENSURE(0L != rPointList.mpVectors, "Error: Bezier list needs to exist here(!)");

		return (*mpVectors == *rPointList.mpVectors);
	}

	const Point& getPoint(sal_uInt32 nIndex) const
	{
		return maPoints[nIndex].getPoint();
	}

	void setPoint(sal_uInt32 nIndex, const Point& rValue)
	{
		maPoints[nIndex].setPoint(rValue);
	}

	const Vector& getBackwardVector(sal_uInt32 nIndex) const
	{
		if(mpVectors)
			return ((*mpVectors)[nIndex]).getBackwardVector();
		else
			return Vector::getEmptyVector();
	}

	void setBackwardVector(sal_uInt32 nIndex, const Vector& rValue)
	{
		if(mpVectors)
		{
			LocalSimpleBezierEntry& rDest = (*mpVectors)[nIndex];
			bool bBezierNeededBefore(rDest.isBezierNeeded());
			((*mpVectors)[nIndex]).setBackwardVector(rValue);
			bool bBezierNeededAfter(rDest.isBezierNeeded());

			if(bBezierNeededBefore != bBezierNeededAfter)
			{
				if(bBezierNeededAfter)
					mnBezierCount++;
				else
					mnBezierCount--;
			}
		}
		else
		{
			bool bEmptyVector(rValue == Vector::getEmptyVector());
			
			if(bEmptyVector)
				return;

			mpVectors = new SimpleBezierVector(maPoints.size());
			((*mpVectors)[nIndex]).setBackwardVector(rValue);
			mnBezierCount++;
		}
	}
	
	const Vector& getForwardVector(sal_uInt32 nIndex) const
	{
		if(mpVectors)
			return ((*mpVectors)[nIndex]).getForwardVector();
		else
			return Vector::getEmptyVector();
	}

	void setForwardVector(sal_uInt32 nIndex, const Vector& rValue)
	{
		if(mpVectors)
		{
			LocalSimpleBezierEntry& rDest = (*mpVectors)[nIndex];
			bool bBezierNeededBefore(rDest.isBezierNeeded());
			((*mpVectors)[nIndex]).setForwardVector(rValue);
			bool bBezierNeededAfter(rDest.isBezierNeeded());

			if(bBezierNeededBefore != bBezierNeededAfter)
			{
				if(bBezierNeededAfter)
					mnBezierCount++;
				else
					mnBezierCount--;
			}
		}
		else
		{
			bool bEmptyVector(rValue == Vector::getEmptyVector());
			
			if(bEmptyVector)
				return;

			mpVectors = new SimpleBezierVector(maPoints.size());
			((*mpVectors)[nIndex]).setForwardVector(rValue);
			mnBezierCount++;
		}
	}

	void insert(sal_uInt32 nIndex, const Point& rPoint, sal_uInt32 nCount)
	{
		if(nCount)
		{
			// maybe vectors are not needed anymore, try to reduce memory footprint
			implTryToReduceToPointVector();

			// add nCount copies of rPoint
			{
				LocalSimplePointEntry aNode(rPoint);
				SimplePointVector::iterator aIndex(maPoints.begin());
				aIndex += nIndex;
				maPoints.insert(aIndex, nCount, aNode);
			}

			// add nCount empty entries to keep indices synchronized
			if(mpVectors)
			{
				LocalSimpleBezierEntry aNode;
				SimpleBezierVector::iterator aIndex(mpVectors->begin());
				aIndex += nIndex;
				mpVectors->insert(aIndex, nCount, aNode);
			}
		}
	}

	void insert(sal_uInt32 nIndex, const PolygonPointList& rSource)
	{
		const sal_uInt32 nCount(rSource.maPoints.size());

		if(nCount)
		{
			// instert point data
			{
				SimplePointVector::iterator aIndex(maPoints.begin());
				aIndex += nIndex;

				SimplePointVector::const_iterator aStart(rSource.maPoints.begin());
				SimplePointVector::const_iterator aEnd(rSource.maPoints.end());

				maPoints.insert(aIndex, aStart, aEnd);
			}

			// insert bezier data
			if(rSource.mpVectors && rSource.mnBezierCount)
			{
				SimpleBezierVector::iterator aIndex(mpVectors->begin());
				aIndex += nIndex;

				SimpleBezierVector::iterator aStart(rSource.mpVectors->begin());
				SimpleBezierVector::iterator aEnd(rSource.mpVectors->end());

				if(!mpVectors)
				{
					mpVectors = new SimpleBezierVector(maPoints.size() - nCount);
				}

				mpVectors->insert(aIndex, aStart, aEnd);

				mnBezierCount += rSource.mnBezierCount;
			}
			else
			{
				// maybe vectors are not needed anymore, try to reduce memory footprint
				implTryToReduceToPointVector();

				// add nCount empty entries to keep indices synchronized
				if(mpVectors)
				{
					LocalSimpleBezierEntry aNode;
					SimpleBezierVector::iterator aIndex(mpVectors->begin());
					aIndex += nIndex;
					mpVectors->insert(aIndex, nCount, aNode);
				}
			}
		}
	}

	void remove(sal_uInt32 nIndex, sal_uInt32 nCount)
	{
		if(nCount)
		{
			// maybe vectors are not needed anymore, try to reduce memory footprint
			implTryToReduceToPointVector();

			// remove point data
			{
				SimplePointVector::iterator aStart(maPoints.begin());
				aStart += nIndex;
				const SimplePointVector::iterator aEnd(aStart + nCount);

				maPoints.erase(aStart, aEnd);
			}

			// remove bezier data
			if(mpVectors)
			{
				SimpleBezierVector::iterator aStart(mpVectors->begin());
				aStart += nIndex;
				const SimpleBezierVector::iterator aEnd(aStart + nCount);

				// take care for correct mnBezierCount BEFORE erase
				if(mnBezierCount)
				{
					SimpleBezierVector::iterator aTestIter(aStart);
					
					for( ; mnBezierCount && aTestIter != aEnd; ++aTestIter)
					{
						if(aTestIter->isBezierNeeded())
							mnBezierCount--;
					}
				}

				if(mnBezierCount)
				{
					// erase nodes
					mpVectors->erase(aStart, aEnd);
				}
				else
				{
					// try to reduce, maybe 0L == mnBezierCount
					implTryToReduceToPointVector();
				}
			}
		}
	}

	void flip()
	{
		if(maPoints.size() > 1)
		{
			// maybe vectors are not needed anymore, try to reduce memory footprint
			implTryToReduceToPointVector();

			// calculate half size
			const sal_uInt32 nHalfSize(maPoints.size() >> 1L);

			// flip point data
			{
				SimplePointVector::iterator aStart(maPoints.begin());
				SimplePointVector::iterator aEnd(maPoints.end());
				
				for(sal_uInt32 a(0); a < nHalfSize; a++)
				{
					LocalSimplePointEntry aTemp = *aStart;
					*aStart++ = *aEnd;
					*aEnd-- = aTemp;
				}
			}

			// flip bezier data
			if(mpVectors)
			{
				SimpleBezierVector::iterator aStart(mpVectors->begin());
				SimpleBezierVector::iterator aEnd(mpVectors->end());
				
				for(sal_uInt32 a(0); a < nHalfSize; a++)
				{
					LocalSimpleBezierEntry aTemp = *aStart;
					aTemp.doInvertForFlip();
					*aStart = *aEnd;
					aStart->doInvertForFlip();
					aStart++;
					*aEnd-- = aTemp;
				}

				// also flip vectors of middle point (if existing)
				if(maPoints.size() % 2)
				{
					(*mpVectors)[nHalfSize].doInvertForFlip();
				}
			}
		}
	}
};

//////////////////////////////////////////////////////////////////////////////

#endif _POLYGON_POINT_HXX
