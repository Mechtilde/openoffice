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



// INCLUDE ---------------------------------------------------------------

#include "scitems.hxx"
#include <editeng/boxitem.hxx>

#include "tabvwsh.hxx"
#include "document.hxx"


//------------------------------------------------------------------

void ScTabViewShell::SetDefaultFrameLine( const SvxBorderLine* pLine )
{
	if ( pLine )
	{
		delete pCurFrameLine;
		pCurFrameLine = new SvxBorderLine( &pLine->GetColor(),
											pLine->GetOutWidth(),
											pLine->GetInWidth(),
											pLine->GetDistance() );
	}
	else if ( pCurFrameLine )
	{
		delete pCurFrameLine;
		pCurFrameLine = NULL;
	}
}

//------------------------------------------------------------------

sal_Bool __EXPORT ScTabViewShell::HasSelection( sal_Bool bText ) const
{
	sal_Bool bHas = sal_False;
	ScViewData* pData = (ScViewData*)GetViewData();		// const weggecasted
	if ( bText )
	{
		//	Text enthalten: Anzahl2 >= 1
		ScDocument* pDoc = pData->GetDocument();
		ScMarkData& rMark = pData->GetMarkData();
		ScAddress aCursor( pData->GetCurX(), pData->GetCurY(), pData->GetTabNo() );
		double fVal = 0.0;
		if ( pDoc->GetSelectionFunction( SUBTOTAL_FUNC_CNT2, aCursor, rMark, fVal ) )
			bHas = ( fVal > 0.5 );
	}
	else
	{
		ScRange aRange;
        ScMarkType eMarkType = pData->GetSimpleArea( aRange );
        if ( eMarkType == SC_MARK_SIMPLE )
            bHas = ( aRange.aStart != aRange.aEnd );    // more than 1 cell
        else
            bHas = sal_True;                                // multiple selection or filtered
	}
	return bHas;
}

//------------------------------------------------------------------

void ScTabViewShell::UIDeactivated( SfxInPlaceClient* pClient )
{
    ClearHighlightRanges();

    //  Move an der ViewShell soll eigentlich vom Sfx gerufen werden, wenn sich
    //  das Frame-Window wegen unterschiedlicher Toolboxen o.ae. verschiebt
    //  (um nicht aus Versehen z.B. Zeichenobjekte zu verschieben, #56515#).
    //  Dieser Mechanismus funktioniert aber momentan nicht, darum hier der Aufruf
    //  per Hand (im Move wird verglichen, ob die Position wirklich geaendert ist).
    ForceMove();
    SfxViewShell::UIDeactivated( pClient );
}


