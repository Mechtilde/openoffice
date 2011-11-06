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

#include "precompiled_sc.hxx"
#include "AccessibleGlobal.hxx"

using ::com::sun::star::uno::RuntimeException;
using ::com::sun::star::uno::Reference;
using ::com::sun::star::uno::Sequence;
using ::std::set;

ScAccessibleStateSet::ScAccessibleStateSet()
{
}

ScAccessibleStateSet::~ScAccessibleStateSet()
{
}

// XAccessibleStateSet

sal_Bool SAL_CALL ScAccessibleStateSet::isEmpty() throw (RuntimeException)
{
    return maStates.empty();
}

sal_Bool SAL_CALL ScAccessibleStateSet::contains(sal_Int16 nState) 
    throw (RuntimeException)
{
    return maStates.count(nState) != 0;
}

sal_Bool SAL_CALL ScAccessibleStateSet::containsAll(
    const Sequence<sal_Int16>& aStateSet) throw (RuntimeException)
{
    sal_Int32 n = aStateSet.getLength();
    for (sal_Int32 i = 0; i < n; ++i)
    {
        if (!maStates.count(aStateSet[i]))
            // This state is not set.
            return false;
    }
    // All specified states are set.
    return true;
}

Sequence<sal_Int16> SAL_CALL ScAccessibleStateSet::getStates() 
    throw (RuntimeException)
{
    Sequence<sal_Int16> aSeq(0);
    set<sal_Int16>::const_iterator itr = maStates.begin(), itrEnd = maStates.end();
    for (size_t i = 0; itr != itrEnd; ++itr, ++i)
    {
        aSeq.realloc(i+1);
        aSeq[i] = *itr;
    }
    return aSeq;
}

void ScAccessibleStateSet::insert(sal_Int16 nState)
{
    maStates.insert(nState);
}

void ScAccessibleStateSet::clear()
{
    maStates.clear();
}

