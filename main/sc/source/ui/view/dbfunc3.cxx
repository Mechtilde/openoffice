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

#include "dbfunc.hxx"
#include "scitems.hxx"
#include <sfx2/bindings.hxx>
#include <vcl/svapp.hxx>
#include <vcl/msgbox.hxx>
#include <vcl/sound.hxx>
#include <vcl/waitobj.hxx>
#include <svl/zforlist.hxx>
#include <sfx2/app.hxx>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/container/XNameAccess.hpp>
#include <com/sun/star/sheet/DataPilotFieldFilter.hpp>
#include <com/sun/star/sheet/DataPilotFieldGroupBy.hpp>
#include <com/sun/star/sheet/DataPilotFieldOrientation.hpp>
#include <com/sun/star/sheet/DataPilotFieldSortMode.hpp>
#include <com/sun/star/sheet/DataPilotTableHeaderData.hpp>
#include <com/sun/star/sheet/GeneralFunction.hpp>
#include <com/sun/star/sheet/MemberResultFlags.hpp>
#include <com/sun/star/sheet/XDimensionsSupplier.hpp>
#include <com/sun/star/sheet/XDrillDownDataSupplier.hpp>

#include "global.hxx"
#include "globstr.hrc"
#include "sc.hrc"
#include "undotab.hxx"
#include "undodat.hxx"
#include "dbcolect.hxx"
#include "rangenam.hxx"
#include "rangeutl.hxx"
#include "docsh.hxx"
#include "olinetab.hxx"
#include "consoli.hxx"
#include "olinefun.hxx"
#include "dpobject.hxx"
#include "dpsave.hxx"
#include "dpdimsave.hxx"
#include "dbdocfun.hxx"
#include "dpoutput.hxx"
#include "dptabsrc.hxx"
#include "editable.hxx"
#include "docpool.hxx"
#include "patattr.hxx"
#include "unonames.hxx"
#include "cell.hxx"
#include "userlist.hxx"

#include <hash_set>
#include <hash_map>
#include <memory>
#include <list>
#include <vector>

using namespace com::sun::star;
using ::com::sun::star::uno::Any;
using ::com::sun::star::uno::Sequence;
using ::com::sun::star::uno::Reference;
using ::com::sun::star::uno::UNO_QUERY;
using ::com::sun::star::beans::XPropertySet;
using ::com::sun::star::container::XNameAccess;
using ::com::sun::star::sheet::XDimensionsSupplier;
using ::rtl::OUString;
using ::rtl::OUStringHash;
using ::rtl::OUStringBuffer;
using ::std::auto_ptr;
using ::std::list;
using ::std::vector;
using ::std::hash_map;
using ::std::hash_set;

// STATIC DATA -----------------------------------------------------------


//==================================================================

//
//			Outliner
//

//	Outline-Gruppierung erzeugen

void ScDBFunc::MakeOutline( sal_Bool bColumns, sal_Bool bRecord )
{
	ScRange aRange;
	if (GetViewData()->GetSimpleArea(aRange) == SC_MARK_SIMPLE)
	{
		ScDocShell* pDocSh = GetViewData()->GetDocShell();
		ScOutlineDocFunc aFunc(*pDocSh);
		aFunc.MakeOutline( aRange, bColumns, bRecord, sal_False );
	}
	else
		ErrorMessage(STR_NOMULTISELECT);
}

//	Outline-Gruppierung loeschen

void ScDBFunc::RemoveOutline( sal_Bool bColumns, sal_Bool bRecord )
{
	ScRange aRange;
	if (GetViewData()->GetSimpleArea(aRange) == SC_MARK_SIMPLE)
	{
		ScDocShell* pDocSh = GetViewData()->GetDocShell();
		ScOutlineDocFunc aFunc(*pDocSh);
		aFunc.RemoveOutline( aRange, bColumns, bRecord, sal_False );
	}
	else
		ErrorMessage(STR_NOMULTISELECT);
}

//	Menue-Status: Outlines loeschen

void ScDBFunc::TestRemoveOutline( sal_Bool& rCol, sal_Bool& rRow )
{
	sal_Bool bColFound = sal_False;
	sal_Bool bRowFound = sal_False;

	SCCOL nStartCol, nEndCol;
	SCROW nStartRow, nEndRow;
	SCTAB nStartTab, nEndTab;
	if (GetViewData()->GetSimpleArea(nStartCol,nStartRow,nStartTab,nEndCol,nEndRow,nEndTab) == SC_MARK_SIMPLE)
	{
		SCTAB nTab = nStartTab;
		ScDocument* pDoc = GetViewData()->GetDocument();
		ScOutlineTable* pTable = pDoc->GetOutlineTable( nTab );
		if (pTable)
		{
			ScOutlineArray* pArray;
			ScOutlineEntry* pEntry;
			SCCOLROW nStart;
			SCCOLROW nEnd;
			sal_Bool bColMarked = ( nStartRow == 0 && nEndRow == MAXROW );
			sal_Bool bRowMarked = ( nStartCol == 0 && nEndCol == MAXCOL );

			//	Spalten

			if ( !bRowMarked || bColMarked )		// nicht wenn ganze Zeilen markiert
			{
				pArray = pTable->GetColArray();
				ScSubOutlineIterator aColIter( pArray );
				while ((pEntry=aColIter.GetNext()) != NULL && !bColFound)
				{
					nStart = pEntry->GetStart();
					nEnd   = pEntry->GetEnd();
					if ( nStartCol<=static_cast<SCCOL>(nEnd) && nEndCol>=static_cast<SCCOL>(nStart) )
						bColFound = sal_True;
				}
			}

			//	Zeilen

			if ( !bColMarked || bRowMarked )		// nicht wenn ganze Spalten markiert
			{
				pArray = pTable->GetRowArray();
				ScSubOutlineIterator aRowIter( pArray );
				while ((pEntry=aRowIter.GetNext()) != NULL && !bRowFound)
				{
					nStart = pEntry->GetStart();
					nEnd   = pEntry->GetEnd();
					if ( nStartRow<=nEnd && nEndRow>=nStart )
						bRowFound = sal_True;
				}
			}
		}
	}

	rCol = bColFound;
	rRow = bRowFound;
}

void ScDBFunc::RemoveAllOutlines( sal_Bool bRecord )
{
	SCTAB nTab = GetViewData()->GetTabNo();
	ScDocShell* pDocSh = GetViewData()->GetDocShell();
	ScOutlineDocFunc aFunc(*pDocSh);

	HideCursor();
	sal_Bool bOk = aFunc.RemoveAllOutlines( nTab, bRecord, sal_False );
	ShowCursor();

	if (bOk)
		UpdateScrollBars();
}

//	Auto-Outlines

void ScDBFunc::AutoOutline( sal_Bool bRecord )
{
	SCTAB nTab = GetViewData()->GetTabNo();
	ScRange aRange( 0,0,nTab, MAXCOL,MAXROW,nTab );		// ganze Tabelle, wenn nichts markiert
	ScMarkData& rMark = GetViewData()->GetMarkData();
	if ( rMark.IsMarked() || rMark.IsMultiMarked() )
	{
		rMark.MarkToMulti();
		rMark.GetMultiMarkArea( aRange );
	}

	ScDocShell* pDocSh = GetViewData()->GetDocShell();
	ScOutlineDocFunc aFunc(*pDocSh);
	aFunc.AutoOutline( aRange, bRecord, sal_False );
}

//	Outline-Ebene auswaehlen

void ScDBFunc::SelectLevel( sal_Bool bColumns, sal_uInt16 nLevel, sal_Bool bRecord, sal_Bool bPaint )
{
	SCTAB nTab = GetViewData()->GetTabNo();
	ScDocShell* pDocSh = GetViewData()->GetDocShell();
	ScOutlineDocFunc aFunc(*pDocSh);

	HideCursor();
	sal_Bool bOk = aFunc.SelectLevel( nTab, bColumns, nLevel, bRecord, bPaint, sal_False );
	ShowCursor();

	if (bOk)
		UpdateScrollBars();
}

//	einzelne Outline-Gruppe einblenden

void ScDBFunc::ShowOutline( sal_Bool bColumns, sal_uInt16 nLevel, sal_uInt16 nEntry, sal_Bool bRecord, sal_Bool bPaint )
{
	SCTAB nTab = GetViewData()->GetTabNo();
	ScDocShell* pDocSh = GetViewData()->GetDocShell();
	ScOutlineDocFunc aFunc(*pDocSh);

	HideCursor();
	sal_Bool bOk = aFunc.ShowOutline( nTab, bColumns, nLevel, nEntry, bRecord, bPaint, sal_False );
	ShowCursor();

	if ( bOk && bPaint )
		UpdateScrollBars();
}

//	einzelne Outline-Gruppe ausblenden

void ScDBFunc::HideOutline( sal_Bool bColumns, sal_uInt16 nLevel, sal_uInt16 nEntry, sal_Bool bRecord, sal_Bool bPaint )
{
	SCTAB nTab = GetViewData()->GetTabNo();
	ScDocShell* pDocSh = GetViewData()->GetDocShell();
	ScOutlineDocFunc aFunc(*pDocSh);

	HideCursor();
	sal_Bool bOk = aFunc.HideOutline( nTab, bColumns, nLevel, nEntry, bRecord, bPaint, sal_False );
	ShowCursor();

	if ( bOk && bPaint )
		UpdateScrollBars();
}

//	Menue-Status: markierten Bereich ein-/ausblenden

sal_Bool ScDBFunc::OutlinePossible(sal_Bool bHide)
{
	sal_Bool bEnable = sal_False;

	SCCOL nStartCol;
	SCROW nStartRow;
	SCTAB nStartTab;
	SCCOL nEndCol;
	SCROW nEndRow;
	SCTAB nEndTab;

	if (GetViewData()->GetSimpleArea(nStartCol,nStartRow,nStartTab,nEndCol,nEndRow,nEndTab) == SC_MARK_SIMPLE)
	{
		ScDocument* pDoc = GetViewData()->GetDocument();
		SCTAB nTab = GetViewData()->GetTabNo();
		ScOutlineTable* pTable = pDoc->GetOutlineTable( nTab );
		if (pTable)
		{
			ScOutlineArray* pArray;
			ScOutlineEntry* pEntry;
			SCCOLROW nStart;
			SCCOLROW nEnd;

			//	Spalten

			pArray = pTable->GetColArray();
			ScSubOutlineIterator aColIter( pArray );
			while ((pEntry=aColIter.GetNext()) != NULL && !bEnable)
			{
				nStart = pEntry->GetStart();
				nEnd   = pEntry->GetEnd();
				if ( bHide )
				{
					if ( nStartCol<=static_cast<SCCOL>(nEnd) && nEndCol>=static_cast<SCCOL>(nStart) )
						if (!pEntry->IsHidden())
							bEnable = sal_True;
				}
				else
				{
					if ( nStart>=nStartCol && nEnd<=nEndCol )
						if (pEntry->IsHidden())
							bEnable = sal_True;
				}
			}

			//	Zeilen

			pArray = pTable->GetRowArray();
			ScSubOutlineIterator aRowIter( pArray );
			while ((pEntry=aRowIter.GetNext()) != NULL)
			{
				nStart = pEntry->GetStart();
				nEnd   = pEntry->GetEnd();
				if ( bHide )
				{
					if ( nStartRow<=nEnd && nEndRow>=nStart )
						if (!pEntry->IsHidden())
							bEnable = sal_True;
				}
				else
				{
					if ( nStart>=nStartRow && nEnd<=nEndRow )
						if (pEntry->IsHidden())
							bEnable = sal_True;
				}
			}
		}
	}

	return bEnable;
}

//	markierten Bereich einblenden

void ScDBFunc::ShowMarkedOutlines( sal_Bool bRecord )
{
	ScRange aRange;
	if (GetViewData()->GetSimpleArea(aRange) == SC_MARK_SIMPLE)
	{
		ScDocShell* pDocSh = GetViewData()->GetDocShell();
		ScOutlineDocFunc aFunc(*pDocSh);
		HideCursor();
		sal_Bool bDone = aFunc.ShowMarkedOutlines( aRange, bRecord, sal_False );
		ShowCursor();
		if (bDone)
			UpdateScrollBars();
	}
	else
		ErrorMessage(STR_NOMULTISELECT);
}

//	markierten Bereich ausblenden

void ScDBFunc::HideMarkedOutlines( sal_Bool bRecord )
{
	ScRange aRange;
	if (GetViewData()->GetSimpleArea(aRange) == SC_MARK_SIMPLE)
	{
		ScDocShell* pDocSh = GetViewData()->GetDocShell();
		ScOutlineDocFunc aFunc(*pDocSh);
		HideCursor();
		sal_Bool bDone = aFunc.HideMarkedOutlines( aRange, bRecord, sal_False );
		ShowCursor();
		if (bDone)
			UpdateScrollBars();
	}
	else
		ErrorMessage(STR_NOMULTISELECT);
}

//	--------------------------------------------------------------------------

//
//			Teilergebnisse
//

void ScDBFunc::DoSubTotals( const ScSubTotalParam& rParam, sal_Bool bRecord,
							const ScSortParam* pForceNewSort )
{
	sal_Bool bDo = !rParam.bRemoveOnly;							// sal_False = nur loeschen

	ScDocShell* pDocSh = GetViewData()->GetDocShell();
	ScDocument* pDoc = pDocSh->GetDocument();
	ScMarkData& rMark = GetViewData()->GetMarkData();
	SCTAB nTab = GetViewData()->GetTabNo();
	if (bRecord && !pDoc->IsUndoEnabled())
		bRecord = sal_False;

	ScDBData* pDBData = pDoc->GetDBAtArea( nTab, rParam.nCol1, rParam.nRow1,
												rParam.nCol2, rParam.nRow2 );
	if (!pDBData)
	{
		DBG_ERROR( "SubTotals: keine DBData" );
		return;
	}

	ScEditableTester aTester( pDoc, nTab, 0,rParam.nRow1+1, MAXCOL,MAXROW );
	if (!aTester.IsEditable())
	{
		ErrorMessage(aTester.GetMessageId());
		return;
	}

	if (pDoc->HasAttrib( rParam.nCol1, rParam.nRow1+1, nTab,
						 rParam.nCol2, rParam.nRow2, nTab, HASATTR_MERGED | HASATTR_OVERLAPPED ))
	{
		ErrorMessage(STR_MSSG_INSERTCELLS_0);	// nicht in zusammengefasste einfuegen
		return;
	}

	WaitObject aWait( GetViewData()->GetDialogParent() );
	sal_Bool bOk = sal_True;
	sal_Bool bDelete = sal_False;
	if (rParam.bReplace)
		if (pDoc->TestRemoveSubTotals( nTab, rParam ))
		{
			bDelete = sal_True;
			bOk = ( MessBox( GetViewData()->GetDialogParent(), WinBits(WB_YES_NO | WB_DEF_YES),
				// "StarCalc" "Daten loeschen?"
				ScGlobal::GetRscString( STR_MSSG_DOSUBTOTALS_0 ),
				ScGlobal::GetRscString( STR_MSSG_DOSUBTOTALS_1 ) ).Execute()
				== RET_YES );
		}

	if (bOk)
	{
		ScDocShellModificator aModificator( *pDocSh );

		ScSubTotalParam aNewParam( rParam );		// Bereichsende wird veraendert
		ScDocument*		pUndoDoc = NULL;
		ScOutlineTable*	pUndoTab = NULL;
		ScRangeName*	pUndoRange = NULL;
		ScDBCollection* pUndoDB = NULL;
		SCTAB 			nTabCount = 0;				// fuer Referenz-Undo

		if (bRecord)										// alte Daten sichern
		{
			sal_Bool bOldFilter = bDo && rParam.bDoSort;

			nTabCount = pDoc->GetTableCount();
			pUndoDoc = new ScDocument( SCDOCMODE_UNDO );
			ScOutlineTable* pTable = pDoc->GetOutlineTable( nTab );
			if (pTable)
			{
				pUndoTab = new ScOutlineTable( *pTable );

				SCCOLROW nOutStartCol;							// Zeilen/Spaltenstatus
				SCCOLROW nOutStartRow;
				SCCOLROW nOutEndCol;
				SCCOLROW nOutEndRow;
				pTable->GetColArray()->GetRange( nOutStartCol, nOutEndCol );
				pTable->GetRowArray()->GetRange( nOutStartRow, nOutEndRow );

				pUndoDoc->InitUndo( pDoc, nTab, nTab, sal_True, sal_True );
				pDoc->CopyToDocument( static_cast<SCCOL>(nOutStartCol), 0, nTab, static_cast<SCCOL>(nOutEndCol), MAXROW, nTab, IDF_NONE, sal_False, pUndoDoc );
				pDoc->CopyToDocument( 0, nOutStartRow, nTab, MAXCOL, nOutEndRow, nTab, IDF_NONE, sal_False, pUndoDoc );
			}
			else
				pUndoDoc->InitUndo( pDoc, nTab, nTab, sal_False, bOldFilter );

			//	Datenbereich sichern - incl. Filter-Ergebnis
			pDoc->CopyToDocument( 0,rParam.nRow1+1,nTab, MAXCOL,rParam.nRow2,nTab,
									IDF_ALL, sal_False, pUndoDoc );

			//	alle Formeln wegen Referenzen
			pDoc->CopyToDocument( 0,0,0, MAXCOL,MAXROW,nTabCount-1,
										IDF_FORMULA, sal_False, pUndoDoc );

			//	DB- und andere Bereiche
			ScRangeName* pDocRange = pDoc->GetRangeName();
			if (pDocRange->GetCount())
				pUndoRange = new ScRangeName( *pDocRange );
			ScDBCollection* pDocDB = pDoc->GetDBCollection();
			if (pDocDB->GetCount())
				pUndoDB = new ScDBCollection( *pDocDB );
		}

//		pDoc->SetOutlineTable( nTab, NULL );
		ScOutlineTable*	pOut = pDoc->GetOutlineTable( nTab );
		if (pOut)
			pOut->GetRowArray()->RemoveAll();		// nur Zeilen-Outlines loeschen

		if (rParam.bReplace)
			pDoc->RemoveSubTotals( nTab, aNewParam );
		sal_Bool bSuccess = sal_True;
		if (bDo)
		{
			// Sortieren
			if ( rParam.bDoSort || pForceNewSort )
			{
				pDBData->SetArea( nTab, aNewParam.nCol1,aNewParam.nRow1, aNewParam.nCol2,aNewParam.nRow2 );

				//	Teilergebnis-Felder vor die Sortierung setzen
				//	(doppelte werden weggelassen, kann darum auch wieder aufgerufen werden)

				ScSortParam aOldSort;
				pDBData->GetSortParam( aOldSort );
				ScSortParam aSortParam( aNewParam, pForceNewSort ? *pForceNewSort : aOldSort );
				Sort( aSortParam, sal_False, sal_False );
			}

			bSuccess = pDoc->DoSubTotals( nTab, aNewParam );
		}
		ScRange aDirtyRange( aNewParam.nCol1, aNewParam.nRow1, nTab,
			aNewParam.nCol2, aNewParam.nRow2, nTab );
		pDoc->SetDirty( aDirtyRange );

		if (bRecord)
		{
//			ScDBData* pUndoDBData = pDBData ? new ScDBData( *pDBData ) : NULL;
			pDocSh->GetUndoManager()->AddUndoAction(
				new ScUndoSubTotals( pDocSh, nTab,
										rParam, aNewParam.nRow2,
										pUndoDoc, pUndoTab, // pUndoDBData,
										pUndoRange, pUndoDB ) );
		}

		if (!bSuccess)
		{
			// "Kann keine Zeilen einfuegen"
			ErrorMessage(STR_MSSG_DOSUBTOTALS_2);
		}

													// merken
		pDBData->SetSubTotalParam( aNewParam );
		pDBData->SetArea( nTab, aNewParam.nCol1,aNewParam.nRow1, aNewParam.nCol2,aNewParam.nRow2 );
		pDoc->CompileDBFormula();

		DoneBlockMode();
		InitOwnBlockMode();
		rMark.SetMarkArea( ScRange( aNewParam.nCol1,aNewParam.nRow1,nTab,
									aNewParam.nCol2,aNewParam.nRow2,nTab ) );
        MarkDataChanged();

		pDocSh->PostPaint( 0,0,nTab, MAXCOL,MAXROW,nTab,
												PAINT_GRID | PAINT_LEFT | PAINT_TOP | PAINT_SIZE );

		aModificator.SetDocumentModified();

		SelectionChanged();
	}
}

//
//			Consolidate
//

void ScDBFunc::Consolidate( const ScConsolidateParam& rParam, sal_Bool bRecord )
{
	ScDocShell* pDocShell = GetViewData()->GetDocShell();
	pDocShell->DoConsolidate( rParam, bRecord );
	SetTabNo( rParam.nTab, sal_True );
}

//
//			Pivot
//

String lcl_MakePivotTabName( const String& rPrefix, SCTAB nNumber )
{
	String aName = rPrefix;
	aName += String::CreateFromInt32( nNumber );
	return aName;
}

bool ScDBFunc::MakePivotTable( const ScDPSaveData& rData, const ScRange& rDest, sal_Bool bNewTable,
								const ScDPObject& rSource, sal_Bool bApi )
{
	//	#70096# error message if no fields are set
	//	this must be removed when drag&drop of fields from a toolbox is available

	if ( rData.IsEmpty() && !bApi )
	{
		ErrorMessage(STR_PIVOT_NODATA);
		return false;
	}

	ScDocShell* pDocSh	= GetViewData()->GetDocShell();
	ScDocument* pDoc 	= GetViewData()->GetDocument();
	sal_Bool bUndo(pDoc->IsUndoEnabled());

	ScRange aDestRange = rDest;
	if ( bNewTable )
	{
		SCTAB nSrcTab = GetViewData()->GetTabNo();

		String aName( ScGlobal::GetRscString(STR_PIVOT_TABLE) );
		String aStr;

		pDoc->GetName( nSrcTab, aStr );
		aName += '_';
		aName += aStr;
		aName += '_';

		SCTAB nNewTab = nSrcTab+1;

		SCTAB i=1;
		while ( !pDoc->InsertTab( nNewTab, lcl_MakePivotTabName( aName, i ) ) && i <= MAXTAB )
			i++;

		sal_Bool bAppend = ( nNewTab+1 == pDoc->GetTableCount() );
		if (bUndo)
		{
			pDocSh->GetUndoManager()->AddUndoAction(
						new ScUndoInsertTab( pDocSh, nNewTab, bAppend, lcl_MakePivotTabName( aName, i ) ));
		}

		GetViewData()->InsertTab( nNewTab );
		SetTabNo( nNewTab, sal_True );

		aDestRange = ScRange( 0, 0, nNewTab );
	}

	ScDPObject* pDPObj = pDoc->GetDPAtCursor(
							aDestRange.aStart.Col(), aDestRange.aStart.Row(), aDestRange.aStart.Tab() );

	ScDPObject aObj( rSource );
	aObj.SetOutRange( aDestRange );
    if ( pDPObj && !rData.GetExistingDimensionData() )
    {
        // copy dimension data from old object - lost in the dialog
        //! change the dialog to keep the dimension data

        ScDPSaveData aNewData( rData );
        const ScDPSaveData* pOldData = pDPObj->GetSaveData();
        if ( pOldData )
        {
            const ScDPDimensionSaveData* pDimSave = pOldData->GetExistingDimensionData();
            aNewData.SetDimensionData( pDimSave );
        }
        aObj.SetSaveData( aNewData );
    }
    else
        aObj.SetSaveData( rData );

	sal_Bool bAllowMove = ( pDPObj != NULL );	// allow re-positioning when editing existing table

	ScDBDocFunc aFunc( *pDocSh );
	bool bSuccess = aFunc.DataPilotUpdate( pDPObj, &aObj, sal_True, sal_False, bAllowMove );

	CursorPosChanged();		// shells may be switched

	if ( bNewTable )
	{
		pDocSh->PostPaintExtras();
		SFX_APP()->Broadcast( SfxSimpleHint( SC_HINT_TABLES_CHANGED ) );
	}

	return bSuccess;
}

void ScDBFunc::DeletePivotTable()
{
	ScDocShell* pDocSh	  = GetViewData()->GetDocShell();
	ScDocument* pDoc	  = pDocSh->GetDocument();
	ScDPObject*	pDPObj    = pDoc->GetDPAtCursor( GetViewData()->GetCurX(),
												  GetViewData()->GetCurY(),
												  GetViewData()->GetTabNo() );
	if ( pDPObj )
	{
		ScDBDocFunc aFunc( *pDocSh );
		aFunc.DataPilotUpdate( pDPObj, NULL, sal_True, sal_False );
		CursorPosChanged();		// shells may be switched
	}
	else
		ErrorMessage(STR_PIVOT_NOTFOUND);
}
sal_uLong RefreshDPObject( ScDPObject *pDPObj, ScDocument *pDoc, ScDocShell *pDocSh, sal_Bool bRecord, sal_Bool bApi )
{
	if( !pDPObj )
		return STR_PIVOT_NOTFOUND;

    if ( pDocSh && !pDoc )
        pDoc = pDocSh->GetDocument();

	if( !pDoc  )
        return static_cast<sal_uLong>(-1);

	if( !pDocSh && ( pDocSh = PTR_CAST( ScDocShell, pDoc->GetDocumentShell() ) ) == NULL )
        return static_cast<sal_uLong>(-1);

	if( sal_uLong nErrId = pDPObj->RefreshCache() )
		return nErrId;
	else if ( nErrId == 0 )
	{
		//Refresh all dpobjects
		ScDPCollection* pDPCollection = pDoc->GetDPCollection();
		sal_uInt16 nCount = pDPCollection->GetCount();
		for (sal_uInt16 i=0; i<nCount; i++)
		{
			if ( (*pDPCollection)[i]->GetCacheId() == pDPObj->GetCacheId()  )
			{
				ScDBDocFunc aFunc( * pDocSh );
				if ( !aFunc.DataPilotUpdate( (*pDPCollection)[i], (*pDPCollection)[i], bRecord, bApi ) )
					break;
			}       
		}
		
		return nErrId;
	}

	return 0U;
}

sal_uLong  ScDBFunc::RecalcPivotTable()
{
	ScDocShell* pDocSh  = GetViewData()->GetDocShell();
	ScDocument* pDoc	= GetViewData()->GetDocument();

	//	old pivot not used any more

	ScDPObject*	pDPObj	= pDoc->GetDPAtCursor( GetViewData()->GetCurX(),
												  GetViewData()->GetCurY(),
												  GetViewData()->GetTabNo() );
	if ( pDPObj )
	{
        // Wang Xu Ming -- 2009-6-17
        // DataPilot Migration
        //ScDBDocFunc aFunc( *pDocSh );
        //aFunc.DataPilotUpdate( pDPObj, pDPObj, sal_True, sal_False );
        //CursorPosChanged();      // shells may be switched
        sal_uLong nErrId = RefreshDPObject( pDPObj, pDoc, pDocSh, sal_True, sal_False );//pDPObj->RefreshCache();
        if ( nErrId == 0 )
        {
            // There is no undo for the refresh of the cache table, but the undo history for cell changes
            // remains valid and should be preserved, so the history isn't cleared here.
            //GetViewData()->GetDocShell()->GetUndoManager()->Clear();
        }
        else if (nErrId <= USHRT_MAX)
            ErrorMessage(static_cast<sal_uInt16>(nErrId));
      return nErrId;
      // End Comments
	}
	else
		ErrorMessage(STR_PIVOT_NOTFOUND);
    return STR_PIVOT_NOTFOUND;
}

void ScDBFunc::GetSelectedMemberList( ScStrCollection& rEntries, long& rDimension )
{
    ScDPObject* pDPObj = GetViewData()->GetDocument()->GetDPAtCursor( GetViewData()->GetCurX(),
            							GetViewData()->GetCurY(), GetViewData()->GetTabNo() );
    if ( !pDPObj )
        return;

    long nStartDimension = -1;
    long nStartHierarchy = -1;
    long nStartLevel     = -1;

    ScRangeListRef xRanges;
    GetViewData()->GetMultiArea( xRanges );         // incl. cursor if nothing is selected
    sal_uLong nRangeCount = xRanges->Count();
    sal_Bool bContinue = sal_True;

    for (sal_uLong nRangePos=0; nRangePos<nRangeCount && bContinue; nRangePos++)
    {
        ScRange aRange = *xRanges->GetObject(nRangePos);
        SCCOL nStartCol = aRange.aStart.Col();
        SCROW nStartRow = aRange.aStart.Row();
        SCCOL nEndCol = aRange.aEnd.Col();
        SCROW nEndRow = aRange.aEnd.Row();
        SCTAB nTab = aRange.aStart.Tab();

        for (SCROW nRow=nStartRow; nRow<=nEndRow && bContinue; nRow++)
            for (SCCOL nCol=nStartCol; nCol<=nEndCol && bContinue; nCol++)
            {
                sheet::DataPilotTableHeaderData aData;
                pDPObj->GetHeaderPositionData(ScAddress(nCol, nRow, nTab), aData);
                if ( aData.Dimension < 0 )
                    bContinue = sal_False;              // not part of any dimension
                else
                {
                    if ( nStartDimension < 0 )      // first member?
                    {
                        nStartDimension = aData.Dimension;
                        nStartHierarchy = aData.Hierarchy;
                        nStartLevel     = aData.Level;
                    }
                    if ( aData.Dimension != nStartDimension ||
                         aData.Hierarchy != nStartHierarchy ||
                         aData.Level     != nStartLevel )
                    {
                        bContinue = sal_False;          // cannot mix dimensions
                    }
                }
                if ( bContinue )
                {
                    // accept any part of a member description, also subtotals,
                    // but don't stop if empty parts are contained
                    if ( aData.Flags & sheet::MemberResultFlags::HASMEMBER )
                    {
                        StrData* pNew = new StrData( aData.MemberName );
                        if ( !rEntries.Insert( pNew ) )
                            delete pNew;
                    }
                }
            }
    }

    rDimension = nStartDimension;   // dimension from which the found members came
    if (!bContinue)
        rEntries.FreeAll();         // remove all if not valid
}

sal_Bool ScDBFunc::HasSelectionForDateGroup( ScDPNumGroupInfo& rOldInfo, sal_Int32& rParts )
{
    // determine if the date group dialog has to be shown for the current selection

    sal_Bool bFound = sal_False;

    SCCOL nCurX = GetViewData()->GetCurX();
    SCROW nCurY = GetViewData()->GetCurY();
    SCTAB nTab = GetViewData()->GetTabNo();
    ScDocument* pDoc = GetViewData()->GetDocument();

    ScDPObject* pDPObj = pDoc->GetDPAtCursor( nCurX, nCurY, nTab );
    if ( pDPObj )
    {
        ScStrCollection aEntries;
        long nSelectDimension = -1;
        GetSelectedMemberList( aEntries, nSelectDimension );

        if ( aEntries.GetCount() > 0 )
        {
            sal_Bool bIsDataLayout;
            String aDimName = pDPObj->GetDimName( nSelectDimension, bIsDataLayout );
            String aBaseDimName( aDimName );

            sal_Bool bInGroupDim = sal_False;
            sal_Bool bFoundParts = sal_False;

            ScDPDimensionSaveData* pDimData =
                const_cast<ScDPDimensionSaveData*>( pDPObj->GetSaveData()->GetExistingDimensionData() );
            if ( pDimData )
            {
                const ScDPSaveNumGroupDimension* pNumGroupDim = pDimData->GetNumGroupDim( aDimName );
                const ScDPSaveGroupDimension* pGroupDim = pDimData->GetNamedGroupDim( aDimName );
                if ( pNumGroupDim )
                {
                    //  existing num group dimension

                    if ( pNumGroupDim->GetDatePart() != 0 )
                    {
                        //  dimension has date info -> edit settings of this dimension
                        //  (parts are collected below)

                        rOldInfo = pNumGroupDim->GetDateInfo();
                        bFound = sal_True;
                    }
                    else if ( pNumGroupDim->GetInfo().DateValues )
                    {
                        //  Numerical grouping with DateValues flag is used for grouping
                        //  of days with a "Number of days" value.

                        rOldInfo = pNumGroupDim->GetInfo();
                        rParts = com::sun::star::sheet::DataPilotFieldGroupBy::DAYS;               // not found in CollectDateParts
                        bFoundParts = sal_True;
                        bFound = sal_True;
                    }
                    bInGroupDim = sal_True;
                }
                else if ( pGroupDim )
                {
                    //  existing additional group dimension

                    if ( pGroupDim->GetDatePart() != 0 )
                    {
                        //  dimension has date info -> edit settings of this dimension
                        //  (parts are collected below)

                        rOldInfo = pGroupDim->GetDateInfo();
                        aBaseDimName = pGroupDim->GetSourceDimName();
                        bFound = sal_True;
                    }
                    bInGroupDim = sal_True;
                }
            }
            if ( bFound && !bFoundParts )
            {
                // collect date parts from all group dimensions
                rParts = pDimData->CollectDateParts( aBaseDimName );
            }
            if ( !bFound && !bInGroupDim )
            {
                // create new date group dimensions if the selection is a single cell
                // in a normal dimension with date content

                ScRange aSelRange;
                if ( (GetViewData()->GetSimpleArea( aSelRange ) == SC_MARK_SIMPLE) &&
                        aSelRange.aStart == aSelRange.aEnd )
                {
                    SCCOL nSelCol = aSelRange.aStart.Col();
                    SCROW nSelRow = aSelRange.aStart.Row();
                    SCTAB nSelTab = aSelRange.aStart.Tab();
                    if ( pDoc->HasValueData( nSelCol, nSelRow, nSelTab ) )
                    {
                        sal_uLong nIndex = static_cast<const SfxUInt32Item*>(pDoc->GetAttr(
                                        nSelCol, nSelRow, nSelTab, ATTR_VALUE_FORMAT))->GetValue();
                        short nType = pDoc->GetFormatTable()->GetType(nIndex);
                        if ( nType == NUMBERFORMAT_DATE || nType == NUMBERFORMAT_TIME || nType == NUMBERFORMAT_DATETIME )
                        {
                            bFound = sal_True;
                            // use currently selected value for automatic limits
                            if( rOldInfo.AutoStart )
                                rOldInfo.Start = pDoc->GetValue( aSelRange.aStart );
                            if( rOldInfo.AutoEnd )
                                rOldInfo.End = pDoc->GetValue( aSelRange.aStart );
                        }
                    }
                }
            }
        }
    }

    return bFound;
}

sal_Bool ScDBFunc::HasSelectionForNumGroup( ScDPNumGroupInfo& rOldInfo )
{
    // determine if the numeric group dialog has to be shown for the current selection

    sal_Bool bFound = sal_False;

    SCCOL nCurX = GetViewData()->GetCurX();
    SCROW nCurY = GetViewData()->GetCurY();
    SCTAB nTab = GetViewData()->GetTabNo();
    ScDocument* pDoc = GetViewData()->GetDocument();

    ScDPObject* pDPObj = pDoc->GetDPAtCursor( nCurX, nCurY, nTab );
    if ( pDPObj )
    {
        ScStrCollection aEntries;
        long nSelectDimension = -1;
        GetSelectedMemberList( aEntries, nSelectDimension );

        if ( aEntries.GetCount() > 0 )
        {
            sal_Bool bIsDataLayout;
            String aDimName = pDPObj->GetDimName( nSelectDimension, bIsDataLayout );

            sal_Bool bInGroupDim = sal_False;

            ScDPDimensionSaveData* pDimData =
                const_cast<ScDPDimensionSaveData*>( pDPObj->GetSaveData()->GetExistingDimensionData() );
            if ( pDimData )
            {
                const ScDPSaveNumGroupDimension* pNumGroupDim = pDimData->GetNumGroupDim( aDimName );
                if ( pNumGroupDim )
                {
                    //  existing num group dimension
                    //  -> edit settings of this dimension

                    rOldInfo = pNumGroupDim->GetInfo();
                    bFound = sal_True;
                }
                else if ( pDimData->GetNamedGroupDim( aDimName ) )
                    bInGroupDim = sal_True;                                    // in a group dimension
            }
            if ( !bFound && !bInGroupDim )
            {
                // create a new num group dimension if the selection is a single cell
                // in a normal dimension with numeric content

                ScRange aSelRange;
                if ( (GetViewData()->GetSimpleArea( aSelRange ) == SC_MARK_SIMPLE) &&
                        aSelRange.aStart == aSelRange.aEnd )
                {
                    if ( pDoc->HasValueData( aSelRange.aStart.Col(), aSelRange.aStart.Row(),
                                             aSelRange.aStart.Tab() ) )
                    {
                        bFound = sal_True;
                        // use currently selected value for automatic limits
                        if( rOldInfo.AutoStart )
                            rOldInfo.Start = pDoc->GetValue( aSelRange.aStart );
                        if( rOldInfo.AutoEnd )
                            rOldInfo.End = pDoc->GetValue( aSelRange.aStart );
                    }
                }
            }
        }
    }

    return bFound;
}

void ScDBFunc::DateGroupDataPilot( const ScDPNumGroupInfo& rInfo, sal_Int32 nParts )
{
    ScDPObject* pDPObj = GetViewData()->GetDocument()->GetDPAtCursor( GetViewData()->GetCurX(),
                                        GetViewData()->GetCurY(), GetViewData()->GetTabNo() );
    if ( pDPObj )
    {
        ScStrCollection aEntries;
        long nSelectDimension = -1;
        GetSelectedMemberList( aEntries, nSelectDimension );

        if ( aEntries.GetCount() > 0 )
        {
            sal_Bool bIsDataLayout;
            String aDimName = pDPObj->GetDimName( nSelectDimension, bIsDataLayout );

            ScDPSaveData aData( *pDPObj->GetSaveData() );
            ScDPDimensionSaveData* pDimData = aData.GetDimensionData();     // created if not there

            // find original base
            String aBaseDimName = aDimName;
            if( const ScDPSaveGroupDimension* pBaseGroupDim = pDimData->GetNamedGroupDim( aDimName ) )
                aBaseDimName = pBaseGroupDim->GetSourceDimName();

            // remove all existing parts (the grouping is built completely new)

            /*  Remove numeric group dimension (exists once at most). No need
                to delete anything in save data (grouping was done inplace in
                an existing base dimension). */
            pDimData->RemoveNumGroupDimension( aBaseDimName );

            /*  Remove named group dimension(s). Collect deleted dimension
                names which may be reused while recreating the groups.
                Dimensions have to be removed from dimension save data and from
                save data too. */
            std::vector< String > aDeletedNames;
            const ScDPSaveGroupDimension* pExistingGroup = pDimData->GetGroupDimForBase( aBaseDimName );
            while ( pExistingGroup )
            {
                String aGroupDimName = pExistingGroup->GetGroupDimName();
                pDimData->RemoveGroupDimension( aGroupDimName );     // pExistingGroup is deleted

                // also remove SaveData settings for the dimension that no longer exists
                aData.RemoveDimensionByName( aGroupDimName );

                /*  The name can be used for the new group dimensions, although
                    it is still in use with the DataPilotSource. */
                aDeletedNames.push_back( aGroupDimName );

                // see if there are more group dimensions
                pExistingGroup = pDimData->GetGroupDimForBase( aBaseDimName );

                if ( pExistingGroup && pExistingGroup->GetGroupDimName() == aGroupDimName )
                {
                    // still get the same group dimension?
                    DBG_ERROR("couldn't remove group dimension");
                    pExistingGroup = NULL;      // avoid endless loop
                }
            }

            if ( nParts )
            {
                // create date group dimensions

                ScDPNumGroupInfo aEmpty;
                bool bFirst = true;
                sal_Int32 nMask = 1;
                for (sal_uInt16 nBit=0; nBit<32; nBit++)
                {
                    if ( nParts & nMask )
                    {
                        if ( bFirst )
                        {
                            // innermost part: create NumGroupDimension (replacing original values)
                            // Dimension name is left unchanged

                            if ( (nParts == sheet::DataPilotFieldGroupBy::DAYS) && (rInfo.Step >= 1.0) )
                            {
                                // only days, and a step value specified: use numerical grouping
                                // with DateValues flag, not date grouping

                                ScDPNumGroupInfo aNumInfo( rInfo );
                                aNumInfo.DateValues = sal_True;

                                ScDPSaveNumGroupDimension aNumGroupDim( aBaseDimName, aNumInfo );
                                pDimData->AddNumGroupDimension( aNumGroupDim );
                            }
                            else
                            {
                                ScDPSaveNumGroupDimension aNumGroupDim( aBaseDimName, rInfo, nMask );
                                pDimData->AddNumGroupDimension( aNumGroupDim );
                            }

                            bFirst = false;
                        }
                        else
                        {
                            // additional parts: create GroupDimension (shown as additional dimensions)
                            String aGroupDimName = pDimData->CreateDateGroupDimName( nMask, *pDPObj, true, &aDeletedNames );
                            ScDPSaveGroupDimension aGroupDim( aBaseDimName, aGroupDimName );
                            aGroupDim.SetDateInfo( rInfo, nMask );
                            pDimData->AddGroupDimension( aGroupDim );

                            // set orientation
                            ScDPSaveDimension* pSaveDimension = aData.GetDimensionByName( aGroupDimName );
                            if ( pSaveDimension->GetOrientation() == sheet::DataPilotFieldOrientation_HIDDEN )
                            {
                                ScDPSaveDimension* pOldDimension = aData.GetDimensionByName( aBaseDimName );
                                pSaveDimension->SetOrientation( pOldDimension->GetOrientation() );
                                long nPosition = 0;     //! before (immediate) base
                                aData.SetPosition( pSaveDimension, nPosition );
                            }
                        }
                    }
                    nMask *= 2;
                }
            }

            // apply changes
            ScDBDocFunc aFunc( *GetViewData()->GetDocShell() );
            ScDPObject* pNewObj = new ScDPObject( *pDPObj );
            pNewObj->SetSaveData( aData );
            aFunc.DataPilotUpdate( pDPObj, pNewObj, sal_True, sal_False );
            delete pNewObj;

            // unmark cell selection
            Unmark();
        }
    }
}

void ScDBFunc::NumGroupDataPilot( const ScDPNumGroupInfo& rInfo )
{
    ScDPObject* pDPObj = GetViewData()->GetDocument()->GetDPAtCursor( GetViewData()->GetCurX(),
                                        GetViewData()->GetCurY(), GetViewData()->GetTabNo() );
    if ( pDPObj )
    {
        ScStrCollection aEntries;
        long nSelectDimension = -1;
        GetSelectedMemberList( aEntries, nSelectDimension );

        if ( aEntries.GetCount() > 0 )
        {
            sal_Bool bIsDataLayout;
            String aDimName = pDPObj->GetDimName( nSelectDimension, bIsDataLayout );

            ScDPSaveData aData( *pDPObj->GetSaveData() );
            ScDPDimensionSaveData* pDimData = aData.GetDimensionData();     // created if not there

            ScDPSaveNumGroupDimension* pExisting = pDimData->GetNumGroupDimAcc( aDimName );
            if ( pExisting )
            {
                // modify existing group dimension
                pExisting->SetGroupInfo( rInfo );
            }
            else
            {
                // create new group dimension
                ScDPSaveNumGroupDimension aNumGroupDim( aDimName, rInfo );
                pDimData->AddNumGroupDimension( aNumGroupDim );
            }

            // apply changes
            ScDBDocFunc aFunc( *GetViewData()->GetDocShell() );
            ScDPObject* pNewObj = new ScDPObject( *pDPObj );
            pNewObj->SetSaveData( aData );
            aFunc.DataPilotUpdate( pDPObj, pNewObj, sal_True, sal_False );
            delete pNewObj;

            // unmark cell selection
            Unmark();
        }
    }
}

void ScDBFunc::GroupDataPilot()
{
    ScDPObject* pDPObj = GetViewData()->GetDocument()->GetDPAtCursor( GetViewData()->GetCurX(),
                                        GetViewData()->GetCurY(), GetViewData()->GetTabNo() );
    if ( pDPObj )
    {
        ScStrCollection aEntries;
        long nSelectDimension = -1;
        GetSelectedMemberList( aEntries, nSelectDimension );

        if ( aEntries.GetCount() > 0 )
        {
            sal_Bool bIsDataLayout;
            String aDimName = pDPObj->GetDimName( nSelectDimension, bIsDataLayout );

            ScDPSaveData aData( *pDPObj->GetSaveData() );
            ScDPDimensionSaveData* pDimData = aData.GetDimensionData();     // created if not there

            // find original base
            String aBaseDimName( aDimName );
            const ScDPSaveGroupDimension* pBaseGroupDim = pDimData->GetNamedGroupDim( aDimName );
            if ( pBaseGroupDim )
            {
                // any entry's SourceDimName is the original base
                aBaseDimName = pBaseGroupDim->GetSourceDimName();
            }

            // find existing group dimension
            // (using the selected dim, can be intermediate group dim)
            ScDPSaveGroupDimension* pGroupDimension = pDimData->GetGroupDimAccForBase( aDimName );

            // remove the selected items from their groups
            // (empty groups are removed, too)
            sal_uInt16 nEntryCount = aEntries.GetCount();
            sal_uInt16 nEntry;
            if ( pGroupDimension )
            {
                for (nEntry=0; nEntry<nEntryCount; nEntry++)
                {
                    String aEntryName = aEntries[nEntry]->GetString();
                    if ( pBaseGroupDim )
                    {
                        // for each selected (intermediate) group, remove all its items
                        // (same logic as for adding, below)
                        const ScDPSaveGroupItem* pBaseGroup = pBaseGroupDim->GetNamedGroup( aEntryName );
                        if ( pBaseGroup )
                            pBaseGroup->RemoveElementsFromGroups( *pGroupDimension );   // remove all elements
                        else
                            pGroupDimension->RemoveFromGroups( aEntryName );
                    }
                    else
                        pGroupDimension->RemoveFromGroups( aEntryName );
                }
            }

            ScDPSaveGroupDimension* pNewGroupDim = NULL;
            if ( !pGroupDimension )
            {
                // create a new group dimension
                String aGroupDimName = pDimData->CreateGroupDimName( aBaseDimName, *pDPObj, false, NULL );
                pNewGroupDim = new ScDPSaveGroupDimension( aBaseDimName, aGroupDimName );

                pGroupDimension = pNewGroupDim;     // make changes to the new dim if none existed

                if ( pBaseGroupDim )
                {
                    // If it's a higher-order group dimension, pre-allocate groups for all
                    // non-selected original groups, so the individual base members aren't
                    // used for automatic groups (this would make the original groups hard
                    // to find).
                    //! Also do this when removing groups?
                    //! Handle this case dynamically with automatic groups?

                    long nGroupCount = pBaseGroupDim->GetGroupCount();
                    for ( long nGroup = 0; nGroup < nGroupCount; nGroup++ )
                    {
                        const ScDPSaveGroupItem* pBaseGroup = pBaseGroupDim->GetGroupByIndex( nGroup );

                        StrData aStrData( pBaseGroup->GetGroupName() );
                        sal_uInt16 nCollIndex;
                        if ( !aEntries.Search( &aStrData, nCollIndex ) )    //! ignore case?
                        {
                            // add an additional group for each item that is not in the selection
                            ScDPSaveGroupItem aGroup( pBaseGroup->GetGroupName() );
                            aGroup.AddElementsFromGroup( *pBaseGroup );
                            pGroupDimension->AddGroupItem( aGroup );
                        }
                    }
                }
            }
            String aGroupDimName = pGroupDimension->GetGroupDimName();

            //! localized prefix string
            String aGroupName = pGroupDimension->CreateGroupName( String::CreateFromAscii("Group") );
            ScDPSaveGroupItem aGroup( aGroupName );
            for (nEntry=0; nEntry<nEntryCount; nEntry++)
            {
                String aEntryName = aEntries[nEntry]->GetString();
                if ( pBaseGroupDim )
                {
                    // for each selected (intermediate) group, add all its items
                    const ScDPSaveGroupItem* pBaseGroup = pBaseGroupDim->GetNamedGroup( aEntryName );
                    if ( pBaseGroup )
                        aGroup.AddElementsFromGroup( *pBaseGroup );
                    else
                        aGroup.AddElement( aEntryName );    // no group found -> automatic group, add the item itself
                }
                else
                    aGroup.AddElement( aEntryName );        // no group dimension, add all items directly
            }

            pGroupDimension->AddGroupItem( aGroup );

            if ( pNewGroupDim )
            {
                pDimData->AddGroupDimension( *pNewGroupDim );
                delete pNewGroupDim;        // AddGroupDimension copies the object
                // don't access pGroupDimension after here
            }
            pGroupDimension = pNewGroupDim = NULL;

            // set orientation
            ScDPSaveDimension* pSaveDimension = aData.GetDimensionByName( aGroupDimName );
            if ( pSaveDimension->GetOrientation() == sheet::DataPilotFieldOrientation_HIDDEN )
            {
                ScDPSaveDimension* pOldDimension = aData.GetDimensionByName( aDimName );
                pSaveDimension->SetOrientation( pOldDimension->GetOrientation() );
                long nPosition = 0;     //! before (immediate) base
                aData.SetPosition( pSaveDimension, nPosition );
            }

            // apply changes
            ScDBDocFunc aFunc( *GetViewData()->GetDocShell() );
            ScDPObject* pNewObj = new ScDPObject( *pDPObj );
            pNewObj->SetSaveData( aData );
            aFunc.DataPilotUpdate( pDPObj, pNewObj, sal_True, sal_False );
            delete pNewObj;

            // unmark cell selection
            Unmark();
        }
    }
}

void ScDBFunc::UngroupDataPilot()
{
    ScDPObject* pDPObj = GetViewData()->GetDocument()->GetDPAtCursor( GetViewData()->GetCurX(),
                                        GetViewData()->GetCurY(), GetViewData()->GetTabNo() );
    if ( pDPObj )
    {
        ScStrCollection aEntries;
        long nSelectDimension = -1;
        GetSelectedMemberList( aEntries, nSelectDimension );

        if ( aEntries.GetCount() > 0 )
        {
            sal_Bool bIsDataLayout;
            String aDimName = pDPObj->GetDimName( nSelectDimension, bIsDataLayout );

            ScDPSaveData aData( *pDPObj->GetSaveData() );
            ScDPDimensionSaveData* pDimData = aData.GetDimensionData();     // created if not there
            //! test first if DimensionData exists?

            sal_Bool bApply = sal_False;

            ScDPSaveGroupDimension* pGroupDim = pDimData->GetNamedGroupDimAcc( aDimName );
            const ScDPSaveNumGroupDimension* pNumGroupDim = pDimData->GetNumGroupDim( aDimName );
            if ( ( pGroupDim && pGroupDim->GetDatePart() != 0 ) ||
                 ( pNumGroupDim && pNumGroupDim->GetDatePart() != 0 ) )
            {
                // Date grouping: need to remove all affected group dimensions.
                // This is done using DateGroupDataPilot with nParts=0.

                DateGroupDataPilot( ScDPNumGroupInfo(), 0 );
                // bApply remains FALSE
                // dimension pointers become invalid
            }
            else if ( pGroupDim )
            {
                sal_uInt16 nEntryCount = aEntries.GetCount();
                for (sal_uInt16 nEntry=0; nEntry<nEntryCount; nEntry++)
                {
                    String aEntryName = aEntries[nEntry]->GetString();
                    pGroupDim->RemoveGroup( aEntryName );
                }
                // remove group dimension if empty
                bool bEmptyDim = pGroupDim->IsEmpty();
                if ( !bEmptyDim )
                {
                    // If all remaining groups in the dimension aren't shown, remove
                    // the dimension too, as if it was completely empty.
                    ScStrCollection aVisibleEntries;
                    pDPObj->GetMemberResultNames( aVisibleEntries, nSelectDimension );
                    bEmptyDim = pGroupDim->HasOnlyHidden( aVisibleEntries );
                }
                if ( bEmptyDim )
                {
                    pDimData->RemoveGroupDimension( aDimName );     // pGroupDim is deleted

                    // also remove SaveData settings for the dimension that no longer exists
                    aData.RemoveDimensionByName( aDimName );
                }
                bApply = sal_True;
            }
            else if ( pNumGroupDim )
            {
                // remove the numerical grouping
                pDimData->RemoveNumGroupDimension( aDimName );
                // SaveData settings can remain unchanged - the same dimension still exists
                bApply = sal_True;
            }

            if ( bApply )
            {
                // apply changes
                ScDBDocFunc aFunc( *GetViewData()->GetDocShell() );
                ScDPObject* pNewObj = new ScDPObject( *pDPObj );
                pNewObj->SetSaveData( aData );
                aFunc.DataPilotUpdate( pDPObj, pNewObj, sal_True, sal_False );
                delete pNewObj;

                // unmark cell selection
                Unmark();
            }
        }
    }
}

static OUString lcl_replaceMemberNameInSubtotal(const OUString& rSubtotal, const OUString& rMemberName)
{
    sal_Int32 n = rSubtotal.getLength();
    const sal_Unicode* p = rSubtotal.getStr();
    OUStringBuffer aBuf, aWordBuf;
    for (sal_Int32 i = 0; i < n; ++i)
    {
        sal_Unicode c = p[i];
        if (c == sal_Unicode(' '))
        {
            OUString aWord = aWordBuf.makeStringAndClear();
            if (aWord.equals(rMemberName))
                aBuf.append(sal_Unicode('?'));
            else
                aBuf.append(aWord);
            aBuf.append(c);
        }
        else if (c == sal_Unicode('\\'))
        {
            // Escape a backslash character.
            aWordBuf.append(c);
            aWordBuf.append(c);
        }
        else if (c == sal_Unicode('?'))
        {
            // A literal '?' must be escaped with a backslash ('\');
            aWordBuf.append(sal_Unicode('\\'));
            aWordBuf.append(c);
        }
        else
            aWordBuf.append(c);
    }

    if (aWordBuf.getLength() > 0)
    {
        OUString aWord = aWordBuf.makeStringAndClear();
        if (aWord.equals(rMemberName))
            aBuf.append(sal_Unicode('?'));
        else
            aBuf.append(aWord);
    }

    return aBuf.makeStringAndClear();
}

void ScDBFunc::DataPilotInput( const ScAddress& rPos, const String& rString )
{
    using namespace ::com::sun::star::sheet;

    String aNewName( rString );

    ScDocument* pDoc = GetViewData()->GetDocument();
    ScDPObject* pDPObj = pDoc->GetDPAtCursor( rPos.Col(), rPos.Row(), rPos.Tab() );
    if (!pDPObj)
        return;

    String aOldText;
    pDoc->GetString( rPos.Col(), rPos.Row(), rPos.Tab(), aOldText );

    if ( aOldText == rString )
    {
        // nothing to do: silently exit
        return;
    }

    sal_uInt16 nErrorId = 0;

    pDPObj->BuildAllDimensionMembers();
    ScDPSaveData aData( *pDPObj->GetSaveData() );
    sal_Bool bChange = sal_False;

    sal_uInt16 nOrient = DataPilotFieldOrientation_HIDDEN;
    long nField = pDPObj->GetHeaderDim( rPos, nOrient );
    if ( nField >= 0 )
    {
        // changing a field title
        if ( aData.GetExistingDimensionData() )
        {
            // only group dimensions can be renamed

            ScDPDimensionSaveData* pDimData = aData.GetDimensionData();
            ScDPSaveGroupDimension* pGroupDim = pDimData->GetNamedGroupDimAcc( aOldText );
            if ( pGroupDim )
            {
                // valid name: not empty, no existing dimension (group or other)
                if ( rString.Len() && !pDPObj->IsDimNameInUse(rString) )
                {
                    pGroupDim->Rename( aNewName );

                    // also rename in SaveData to preserve the field settings
                    ScDPSaveDimension* pSaveDim = aData.GetDimensionByName( aOldText );
                    pSaveDim->SetName( aNewName );

                    bChange = sal_True;
                }
                else
                    nErrorId = STR_INVALIDNAME;
            }
        }
        else if (nOrient == DataPilotFieldOrientation_COLUMN || nOrient == DataPilotFieldOrientation_ROW)
        {
            sal_Bool bDataLayout = false;
            String aDimName = pDPObj->GetDimName(nField, bDataLayout);
            ScDPSaveDimension* pDim = bDataLayout ? aData.GetDataLayoutDimension() : aData.GetDimensionByName(aDimName);
            if (pDim)
            {
                if (rString.Len())
                {
                    if (rString.EqualsIgnoreCaseAscii(aDimName))
                    {    
                        pDim->RemoveLayoutName();
                        bChange = true;
                    }
                    else if (!pDPObj->IsDimNameInUse(rString))
                    {
                        pDim->SetLayoutName(rString);
                        bChange = true;
                    }
                    else
                        nErrorId = STR_INVALIDNAME;
                }
                else
                    nErrorId = STR_INVALIDNAME;
            }
        }
    }
    else if (pDPObj->IsDataDescriptionCell(rPos))
    {
        // There is only one data dimension.
        ScDPSaveDimension* pDim = aData.GetFirstDimension(sheet::DataPilotFieldOrientation_DATA);
        if (pDim)
        {
            if (rString.Len())
            {
                if (rString.EqualsIgnoreCaseAscii(pDim->GetName()))
                {
                    pDim->RemoveLayoutName();
                    bChange = true;
                }
                else if (!pDPObj->IsDimNameInUse(rString))
                {
                    pDim->SetLayoutName(rString);
                    bChange = true;
                }
                else
                    nErrorId = STR_INVALIDNAME;
            }
            else
                nErrorId = STR_INVALIDNAME;
        }
    }
    else
    {
        // This is not a field header.
        sheet::DataPilotTableHeaderData aPosData;
        pDPObj->GetHeaderPositionData(rPos, aPosData);

        if ( (aPosData.Flags & MemberResultFlags::HASMEMBER) && aOldText.Len() )
        {
            if ( aData.GetExistingDimensionData() && !(aPosData.Flags & MemberResultFlags::SUBTOTAL))
            {
                sal_Bool bIsDataLayout;
                String aDimName = pDPObj->GetDimName( aPosData.Dimension, bIsDataLayout );

                ScDPDimensionSaveData* pDimData = aData.GetDimensionData();
                ScDPSaveGroupDimension* pGroupDim = pDimData->GetNamedGroupDimAcc( aDimName );
                if ( pGroupDim )
                {
                    // valid name: not empty, no existing group in this dimension
                    //! ignore case?
                    if ( aNewName.Len() && !pGroupDim->GetNamedGroup( aNewName ) )
                    {
                        ScDPSaveGroupItem* pGroup = pGroupDim->GetNamedGroupAcc( aOldText );
                        if ( pGroup )
                            pGroup->Rename( aNewName );     // rename the existing group
                        else
                        {
                            // create a new group to replace the automatic group
                            ScDPSaveGroupItem aGroup( aNewName );
                            aGroup.AddElement( aOldText );
                            pGroupDim->AddGroupItem( aGroup );
                        }

                        // in both cases also adjust savedata, to preserve member settings (show details)
                        ScDPSaveDimension* pSaveDim = aData.GetDimensionByName( aDimName );
                        ScDPSaveMember* pSaveMember = pSaveDim->GetExistingMemberByName( aOldText );
                        if ( pSaveMember )
                            pSaveMember->SetName( aNewName );

                        bChange = sal_True;
                    }
                    else
                        nErrorId = STR_INVALIDNAME;
                 }
            }
            else if ((aPosData.Flags & MemberResultFlags::GRANDTOTAL))
            {
                aData.SetGrandTotalName(rString);
                bChange = true;
            }
            else if (aPosData.Dimension >= 0 && aPosData.MemberName.getLength() > 0)
            {
                sal_Bool bDataLayout = false;
                String aDimName = pDPObj->GetDimName(static_cast<long>(aPosData.Dimension), bDataLayout);
                if (bDataLayout)
                {
                    // data dimension
                    do
                    {
                        if ((aPosData.Flags & MemberResultFlags::SUBTOTAL))
                            break;

                        ScDPSaveDimension* pDim = aData.GetDimensionByName(aPosData.MemberName);
                        if (!pDim)
                            break;

                        if (!rString.Len())
                        {
                            nErrorId = STR_INVALIDNAME;
                            break;
                        }

                        if (aPosData.MemberName.equalsIgnoreAsciiCase(rString))
                        {
                            pDim->RemoveLayoutName();
                            bChange = true;
                        }
                        else if (!pDPObj->IsDimNameInUse(rString))
                        {
                            pDim->SetLayoutName(rString);
                            bChange = true;
                        }
                        else
                            nErrorId = STR_INVALIDNAME;
                    }
                    while (false);
                }
                else
                {
                    // field member
                    do
                    {
                        ScDPSaveDimension* pDim = aData.GetDimensionByName(aDimName);
                        if (!pDim)
                            break;

                        ScDPSaveMember* pMem = pDim->GetExistingMemberByName(aPosData.MemberName);
                        if (!pMem)
                            break;

                        if ((aPosData.Flags & MemberResultFlags::SUBTOTAL))
                        {
                            // Change subtotal only when the table has one data dimension.
                            if (aData.GetDataDimensionCount() > 1)
                                break;

                            // display name for subtotal is allowed only if the subtotal type is 'Automatic'.
                            if (pDim->GetSubTotalsCount() != 1)
                                break;

                            if (pDim->GetSubTotalFunc(0) != sheet::GeneralFunction_AUTO)
                                break;

                            const OUString* pLayoutName = pMem->GetLayoutName();
                            String aMemberName;
                            if (pLayoutName)
                                aMemberName = *pLayoutName;
                            else
                                aMemberName = aPosData.MemberName;

                            String aNew = lcl_replaceMemberNameInSubtotal(rString, aMemberName);
                            pDim->SetSubtotalName(aNew);
                            bChange = true;
                        }
                        else
                        {
                            // Check to make sure the member name isn't
                            // already used.
                            if (rString.Len())
                            {
                                if (rString.EqualsIgnoreCaseAscii(pMem->GetName()))
                                {
                                    pMem->RemoveLayoutName();
                                    bChange = true;
                                }
                                else if (!pDim->IsMemberNameInUse(rString))
                                {
                                    pMem->SetLayoutName(rString);
                                    bChange = true;
                                }
                                else
                                    nErrorId = STR_INVALIDNAME;
                            }
                            else
                                nErrorId = STR_INVALIDNAME;
                        }
                    }
                    while (false);
                }
            }
        }
    }

    if ( bChange )
    {
        // apply changes
        ScDBDocFunc aFunc( *GetViewData()->GetDocShell() );
        ScDPObject* pNewObj = new ScDPObject( *pDPObj );
        pNewObj->SetSaveData( aData );
        aFunc.DataPilotUpdate( pDPObj, pNewObj, sal_True, sal_False );
        delete pNewObj;
    }
    else
    {
        if ( !nErrorId )
            nErrorId = STR_ERR_DATAPILOT_INPUT;
        ErrorMessage( nErrorId );
    }
}

void lcl_MoveToEnd( ScDPSaveDimension& rDim, const String& rItemName )
{
    ScDPSaveMember* pNewMember = NULL;
    const ScDPSaveMember* pOldMember = rDim.GetExistingMemberByName( rItemName );
    if ( pOldMember )
        pNewMember = new ScDPSaveMember( *pOldMember );
    else
        pNewMember = new ScDPSaveMember( rItemName );
    rDim.AddMember( pNewMember );
    // AddMember takes ownership of the new pointer,
    // puts it to the end of the list even if it was in the list before.
}

struct ScOUStringCollate
{
    CollatorWrapper* mpCollator;

    ScOUStringCollate(CollatorWrapper* pColl) : mpCollator(pColl) {}

    bool operator()(const rtl::OUString& rStr1, const rtl::OUString& rStr2) const
    {
        return ( mpCollator->compareString(rStr1, rStr2) < 0 );
    }
};

bool ScDBFunc::DataPilotSort( const ScAddress& rPos, bool bAscending, sal_uInt16* pUserListId )
{
    ScDocument* pDoc = GetViewData()->GetDocument();
    ScDPObject* pDPObj = pDoc->GetDPAtCursor(rPos.Col(), rPos.Row(), rPos.Tab());
    if (!pDPObj)
        return false;

    // We need to run this to get all members later.
    if ( pUserListId )
        pDPObj->BuildAllDimensionMembers();
    
    sal_uInt16 nOrientation;
    long nDimIndex = pDPObj->GetHeaderDim(rPos, nOrientation);
    if (nDimIndex < 0)
        // Invalid dimension index.  Bail out.
        return false;

    sal_Bool bDataLayout;
    ScDPSaveData* pSaveData = pDPObj->GetSaveData();
    if (!pSaveData)
        return false;

    ScDPSaveData aNewSaveData(*pSaveData);
    String aDimName = pDPObj->GetDimName(nDimIndex, bDataLayout);
    ScDPSaveDimension* pSaveDim = aNewSaveData.GetDimensionByName(aDimName);
    if (!pSaveDim)
        return false;

    // manual evaluation of sort order is only needed if a user list id is given
    if ( pUserListId )
    {
        typedef ScDPSaveDimension::MemberList MemList;
        const MemList& rDimMembers = pSaveDim->GetMembers();
        list<OUString> aMembers;
        hash_set<OUString, ::rtl::OUStringHash> aMemberSet;
        size_t nMemberCount = 0;
        for (MemList::const_iterator itr = rDimMembers.begin(), itrEnd = rDimMembers.end();
              itr != itrEnd; ++itr)
        {
            ScDPSaveMember* pMem = *itr;
            aMembers.push_back(pMem->GetName());
            aMemberSet.insert(pMem->GetName());
            ++nMemberCount;
        }

        // Sort the member list in ascending order.
        ScOUStringCollate aCollate( ScGlobal::GetCollator() );
        aMembers.sort(aCollate);

        // Collect and rank those custom sort strings that also exist in the member name list.

        typedef hash_map<OUString, sal_uInt16, OUStringHash> UserSortMap;
        UserSortMap aSubStrs;
        sal_uInt16 nSubCount = 0;
        if (pUserListId)
        {
            ScUserList* pUserList = ScGlobal::GetUserList();
            if (!pUserList)
                return false;
    
            {
                sal_uInt16 n = pUserList->GetCount();
                if (!n || *pUserListId >= n)
                    return false;
            }
        
            ScUserListData* pData = static_cast<ScUserListData*>((*pUserList)[*pUserListId]);
            if (pData)
            {
                sal_uInt16 n = pData->GetSubCount();
                for (sal_uInt16 i = 0; i < n; ++i)
                {
                    OUString aSub = pData->GetSubStr(i);
                    if (!aMemberSet.count(aSub))
                        // This string doesn't exist in the member name set.  Don't add this.
                        continue;

                    aSubStrs.insert(UserSortMap::value_type(aSub, nSubCount++));
                }
            }
        }

        // Rank all members.

        vector<OUString> aRankedNames(nMemberCount);
        sal_uInt16 nCurStrId = 0;
        for (list<OUString>::const_iterator itr = aMembers.begin(), itrEnd = aMembers.end(); 
              itr != itrEnd; ++itr)
        {
            OUString aName = *itr;
            sal_uInt16 nRank = 0;
            UserSortMap::const_iterator itrSub = aSubStrs.find(aName);
            if (itrSub == aSubStrs.end())
                nRank = nSubCount + nCurStrId++;
            else
                nRank = itrSub->second;

            if (!bAscending)
                nRank = static_cast< sal_uInt16 >( nMemberCount - nRank - 1 );

            aRankedNames[nRank] = aName;
        }

        // Re-order ScDPSaveMember instances with the new ranks.

        for (vector<OUString>::const_iterator itr = aRankedNames.begin(), itrEnd = aRankedNames.end();
              itr != itrEnd; ++itr)
        {
            const ScDPSaveMember* pOldMem = pSaveDim->GetExistingMemberByName(*itr);
            if (!pOldMem)
                // All members are supposed to be present.
                continue;

            ScDPSaveMember* pNewMem = new ScDPSaveMember(*pOldMem);
            pSaveDim->AddMember(pNewMem);
        }

        // Set the sorting mode to manual for now.  We may introduce a new sorting
        // mode later on.

        sheet::DataPilotFieldSortInfo aSortInfo;
        aSortInfo.Mode = sheet::DataPilotFieldSortMode::MANUAL;
        pSaveDim->SetSortInfo(&aSortInfo);
    }
    else
    {
        // without user list id, just apply sorting mode

        sheet::DataPilotFieldSortInfo aSortInfo;
        aSortInfo.Mode = sheet::DataPilotFieldSortMode::NAME;
        aSortInfo.IsAscending = bAscending;
        pSaveDim->SetSortInfo(&aSortInfo);
    }

    // Update the datapilot with the newly sorted field members.

    auto_ptr<ScDPObject> pNewObj(new ScDPObject(*pDPObj));
    pNewObj->SetSaveData(aNewSaveData);
    ScDBDocFunc aFunc(*GetViewData()->GetDocShell());

    return aFunc.DataPilotUpdate(pDPObj, pNewObj.get(), true, false);
}

sal_Bool ScDBFunc::DataPilotMove( const ScRange& rSource, const ScAddress& rDest )
{
    sal_Bool bRet = sal_False;
    ScDocument* pDoc = GetViewData()->GetDocument();
    ScDPObject* pDPObj = pDoc->GetDPAtCursor( rSource.aStart.Col(), rSource.aStart.Row(), rSource.aStart.Tab() );
    if ( pDPObj && pDPObj == pDoc->GetDPAtCursor( rDest.Col(), rDest.Row(), rDest.Tab() ) )
    {
        sheet::DataPilotTableHeaderData aDestData;
        pDPObj->GetHeaderPositionData( rDest, aDestData );
        bool bValid = ( aDestData.Dimension >= 0 );        // dropping onto a field

        // look through the source range
        std::hash_set< rtl::OUString, rtl::OUStringHash, std::equal_to<rtl::OUString> > aMembersSet;   // for lookup
        std::vector< rtl::OUString > aMembersVector;  // members in original order, for inserting
        aMembersVector.reserve( std::max( static_cast<SCSIZE>( rSource.aEnd.Col() - rSource.aStart.Col() + 1 ),
                                          static_cast<SCSIZE>( rSource.aEnd.Row() - rSource.aStart.Row() + 1 ) ) );
        for (SCROW nRow = rSource.aStart.Row(); bValid && nRow <= rSource.aEnd.Row(); ++nRow )
            for (SCCOL nCol = rSource.aStart.Col(); bValid && nCol <= rSource.aEnd.Col(); ++nCol )
            {
                sheet::DataPilotTableHeaderData aSourceData;
                pDPObj->GetHeaderPositionData( ScAddress( nCol, nRow, rSource.aStart.Tab() ), aSourceData );
                if ( aSourceData.Dimension == aDestData.Dimension && aSourceData.MemberName.getLength() )
                {
                    if ( aMembersSet.find( aSourceData.MemberName ) == aMembersSet.end() )
                    {
                        aMembersSet.insert( aSourceData.MemberName );
                        aMembersVector.push_back( aSourceData.MemberName );
                    }
                    // duplicates are ignored
                }
                else
                    bValid = false;     // empty (subtotal) or different field
            }

        if ( bValid )
        {
            sal_Bool bIsDataLayout;
            String aDimName = pDPObj->GetDimName( aDestData.Dimension, bIsDataLayout );
            if ( !bIsDataLayout )
            {
                ScDPSaveData aData( *pDPObj->GetSaveData() );
                ScDPSaveDimension* pDim = aData.GetDimensionByName( aDimName );

                // get all member names in source order
                uno::Sequence<rtl::OUString> aMemberNames;
                pDPObj->GetMemberNames( aDestData.Dimension, aMemberNames );

                bool bInserted = false;

                sal_Int32 nMemberCount = aMemberNames.getLength();
                for (sal_Int32 nMemberPos=0; nMemberPos<nMemberCount; ++nMemberPos)
                {
                    String aMemberStr( aMemberNames[nMemberPos] );

                    if ( !bInserted && aMemberNames[nMemberPos] == aDestData.MemberName )
                    {
                        // insert dragged items before this item
                        for ( std::vector<rtl::OUString>::const_iterator aIter = aMembersVector.begin();
                              aIter != aMembersVector.end(); ++aIter )
                            lcl_MoveToEnd( *pDim, *aIter );
                        bInserted = true;
                    }

                    if ( aMembersSet.find( aMemberStr ) == aMembersSet.end() )  // skip dragged items
                        lcl_MoveToEnd( *pDim, aMemberStr );
                }
                // insert dragged item at end if dest wasn't found (for example, empty)
                if ( !bInserted )
                    for ( std::vector<rtl::OUString>::const_iterator aIter = aMembersVector.begin();
                          aIter != aMembersVector.end(); ++aIter )
                        lcl_MoveToEnd( *pDim, *aIter );

                // Items that were in SaveData, but not in the source, end up at the start of the list.

                // set flag for manual sorting
                sheet::DataPilotFieldSortInfo aSortInfo;
                aSortInfo.Mode = sheet::DataPilotFieldSortMode::MANUAL;
                pDim->SetSortInfo( &aSortInfo );

                // apply changes
                ScDBDocFunc aFunc( *GetViewData()->GetDocShell() );
                ScDPObject* pNewObj = new ScDPObject( *pDPObj );
                pNewObj->SetSaveData( aData );
                aFunc.DataPilotUpdate( pDPObj, pNewObj, sal_True, sal_False );      //! bApi for drag&drop?
                delete pNewObj;

                Unmark();       // entry was moved - no use in leaving the old cell selected

                bRet = sal_True;
            }
        }
    }

    return bRet;
}

sal_Bool ScDBFunc::HasSelectionForDrillDown( sal_uInt16& rOrientation )
{
    sal_Bool bRet = sal_False;

    ScDPObject* pDPObj = GetViewData()->GetDocument()->GetDPAtCursor( GetViewData()->GetCurX(),
            							GetViewData()->GetCurY(), GetViewData()->GetTabNo() );
    if ( pDPObj )
    {
        ScStrCollection aEntries;
        long nSelectDimension = -1;
        GetSelectedMemberList( aEntries, nSelectDimension );

        if ( aEntries.GetCount() > 0 )
        {
            sal_Bool bIsDataLayout;
            String aDimName = pDPObj->GetDimName( nSelectDimension, bIsDataLayout );
            if ( !bIsDataLayout )
            {
                ScDPSaveData* pSaveData = pDPObj->GetSaveData();
            	ScDPSaveDimension* pDim = pSaveData->GetExistingDimensionByName( aDimName );
            	if ( pDim )
            	{
            	    sal_uInt16 nDimOrient = pDim->GetOrientation();
            	    ScDPSaveDimension* pInner = pSaveData->GetInnermostDimension( nDimOrient );
            	    if ( pDim == pInner )
            	    {
            	        rOrientation = nDimOrient;
            	        bRet = sal_True;
            	    }
            	}
            }
        }
    }

    return bRet;
}

void ScDBFunc::SetDataPilotDetails( sal_Bool bShow, const String* pNewDimensionName )
{
    ScDPObject* pDPObj = GetViewData()->GetDocument()->GetDPAtCursor( GetViewData()->GetCurX(),
            							GetViewData()->GetCurY(), GetViewData()->GetTabNo() );
    if ( pDPObj )
    {
        ScStrCollection aEntries;
        long nSelectDimension = -1;
        GetSelectedMemberList( aEntries, nSelectDimension );

        if ( aEntries.GetCount() > 0 )
        {
            sal_Bool bIsDataLayout;
            String aDimName = pDPObj->GetDimName( nSelectDimension, bIsDataLayout );
            if ( !bIsDataLayout )
            {
                ScDPSaveData aData( *pDPObj->GetSaveData() );
                ScDPSaveDimension* pDim = aData.GetDimensionByName( aDimName );

                if ( bShow && pNewDimensionName )
                {
                    //  add the new dimension with the same orientation, at the end

                    ScDPSaveDimension* pNewDim = aData.GetDimensionByName( *pNewDimensionName );
                    ScDPSaveDimension* pDuplicated = NULL;
                    if ( pNewDim->GetOrientation() == sheet::DataPilotFieldOrientation_DATA )
                    {
                        // Need to duplicate the dimension, create column/row in addition to data:
                        // The duplicated dimension inherits the existing settings, pNewDim is modified below.
                        pDuplicated = aData.DuplicateDimension( *pNewDimensionName );
                    }

                    sal_uInt16 nOrientation = pDim->GetOrientation();
                    pNewDim->SetOrientation( nOrientation );

                    long nPosition = LONG_MAX;
                    aData.SetPosition( pNewDim, nPosition );

                    ScDPSaveDimension* pDataLayout = aData.GetDataLayoutDimension();
                    if ( pDataLayout->GetOrientation() == nOrientation &&
                         aData.GetDataDimensionCount() <= 1 )
                    {
                        // If there is only one data dimension, the data layout dimension
                        // must still be the last one in its orientation.
                        aData.SetPosition( pDataLayout, nPosition );
                    }

                    if ( pDuplicated )
                    {
                        // The duplicated (data) dimension needs to be behind the original dimension
                        aData.SetPosition( pDuplicated, nPosition );
                    }

                    //  Hide details for all visible members (selected are changed below).
                    //! Use all members from source level instead (including non-visible)?

                    ScStrCollection aVisibleEntries;
                    pDPObj->GetMemberResultNames( aVisibleEntries, nSelectDimension );

                    sal_uInt16 nVisCount = aVisibleEntries.GetCount();
                    for (sal_uInt16 nVisPos=0; nVisPos<nVisCount; nVisPos++)
                    {
                        String aVisName = aVisibleEntries[nVisPos]->GetString();
                        ScDPSaveMember* pMember = pDim->GetMemberByName( aVisName );
                        pMember->SetShowDetails( sal_False );
                    }
                }

                sal_uInt16 nEntryCount = aEntries.GetCount();
                for (sal_uInt16 nEntry=0; nEntry<nEntryCount; nEntry++)
                {
                    String aEntryName = aEntries[nEntry]->GetString();
                    ScDPSaveMember* pMember = pDim->GetMemberByName( aEntryName );
                    pMember->SetShowDetails( bShow );
                }

                // apply changes
                ScDBDocFunc aFunc( *GetViewData()->GetDocShell() );
                ScDPObject* pNewObj = new ScDPObject( *pDPObj );
                pNewObj->SetSaveData( aData );
                aFunc.DataPilotUpdate( pDPObj, pNewObj, sal_True, sal_False );
                delete pNewObj;

                // unmark cell selection
                Unmark();
            }
        }
    }
}

void ScDBFunc::ShowDataPilotSourceData( ScDPObject& rDPObj, const Sequence<sheet::DataPilotFieldFilter>& rFilters )
{
    ScDocument* pDoc = GetViewData()->GetDocument();
    if (pDoc->GetDocumentShell()->IsReadOnly())
    {
        ErrorMessage(STR_READONLYERR);
        return;
    }

    Reference<sheet::XDimensionsSupplier> xDimSupplier = rDPObj.GetSource();
    Reference<container::XNameAccess> xDims = xDimSupplier->getDimensions();
    Reference<sheet::XDrillDownDataSupplier> xDDSupplier(xDimSupplier, UNO_QUERY);
    if (!xDDSupplier.is())
        return;

    Sequence< Sequence<Any> > aTabData = xDDSupplier->getDrillDownData(rFilters);
    sal_Int32 nRowSize = aTabData.getLength();
    if (nRowSize <= 1)
        // There is no data to show.  Bail out.
        return;

    sal_Int32 nColSize = aTabData[0].getLength();

    SCTAB nNewTab = GetViewData()->GetTabNo();

    auto_ptr<ScDocument> pInsDoc(new ScDocument(SCDOCMODE_CLIP));
    pInsDoc->ResetClip( pDoc, nNewTab );
    for (SCROW nRow = 0; nRow < nRowSize; ++nRow)
    {
        for (SCCOL nCol = 0; nCol < nColSize; ++nCol)
        {
            const Any& rAny = aTabData[nRow][nCol];
            rtl::OUString aStr;
            double fVal;
            if (rAny >>= aStr)
                pInsDoc->PutCell( ScAddress(nCol, nRow, nNewTab), new ScStringCell(String(aStr)) );
            else if (rAny >>= fVal)
                pInsDoc->SetValue(nCol, nRow, nNewTab, fVal);
        }
    }

    // set number format (important for dates)
    for (SCCOL nCol = 0; nCol < nColSize; ++nCol)
    {
        rtl::OUString aStr;
        if (!(aTabData[0][nCol] >>= aStr))
            continue;

        Reference<XPropertySet> xPropSet(xDims->getByName(aStr), UNO_QUERY);
        if (!xPropSet.is())
            continue;

        Any any = xPropSet->getPropertyValue( rtl::OUString::createFromAscii(SC_UNO_NUMBERFO) );
        sal_Int32 nNumFmt = 0;
        if (!(any >>= nNumFmt))
            continue;

        ScPatternAttr aPattern( pInsDoc->GetPool() );
        aPattern.GetItemSet().Put( SfxUInt32Item(ATTR_VALUE_FORMAT, static_cast<sal_uInt32>(nNumFmt)) );
        pInsDoc->ApplyPatternAreaTab(nCol, 1, nCol, nRowSize-1, nNewTab, aPattern);
    }

    SCCOL nEndCol = 0;
    SCROW nEndRow = 0;
    pInsDoc->GetCellArea( nNewTab, nEndCol, nEndRow );
    pInsDoc->SetClipArea( ScRange( 0, 0, nNewTab, nEndCol, nEndRow, nNewTab ) );

    ::svl::IUndoManager* pMgr = GetViewData()->GetDocShell()->GetUndoManager();
    String aUndo = ScGlobal::GetRscString( STR_UNDO_DOOUTLINE );
    pMgr->EnterListAction( aUndo, aUndo );

    String aNewTabName;
    pDoc->CreateValidTabName(aNewTabName);
    if ( InsertTable(aNewTabName, nNewTab) )
        PasteFromClip( IDF_ALL, pInsDoc.get() );

    pMgr->LeaveListAction();
}

//
//			DB-Operationen (Sortieren, Filtern, Teilergebnisse) wiederholen
//

void ScDBFunc::RepeatDB( sal_Bool bRecord )
{
	SCCOL nCurX = GetViewData()->GetCurX();
	SCROW nCurY = GetViewData()->GetCurY();
	SCTAB nTab = GetViewData()->GetTabNo();
	ScDocument* pDoc = GetViewData()->GetDocument();
	ScDBData* pDBData = GetDBData();
	if (bRecord && !pDoc->IsUndoEnabled())
		bRecord = sal_False;

	ScQueryParam aQueryParam;
	pDBData->GetQueryParam( aQueryParam );
	sal_Bool bQuery = aQueryParam.GetEntry(0).bDoQuery;

	ScSortParam aSortParam;
	pDBData->GetSortParam( aSortParam );
	sal_Bool bSort = aSortParam.bDoSort[0];

	ScSubTotalParam aSubTotalParam;
	pDBData->GetSubTotalParam( aSubTotalParam );
	sal_Bool bSubTotal = aSubTotalParam.bGroupActive[0] && !aSubTotalParam.bRemoveOnly;

	if ( bQuery || bSort || bSubTotal )
	{
		sal_Bool bQuerySize = sal_False;
		ScRange aOldQuery;
		ScRange aNewQuery;
		if (bQuery && !aQueryParam.bInplace)
		{
			ScDBData* pDest = pDoc->GetDBAtCursor( aQueryParam.nDestCol, aQueryParam.nDestRow,
													aQueryParam.nDestTab, sal_True );
			if (pDest && pDest->IsDoSize())
			{
				pDest->GetArea( aOldQuery );
				bQuerySize = sal_True;
			}
		}

		SCTAB nDummy;
		SCCOL nStartCol;
		SCROW nStartRow;
		SCCOL nEndCol;
		SCROW nEndRow;
		pDBData->GetArea( nDummy, nStartCol, nStartRow, nEndCol, nEndRow );

		//!		Undo nur benoetigte Daten ?

		ScDocument* pUndoDoc = NULL;
		ScOutlineTable* pUndoTab = NULL;
		ScRangeName* pUndoRange = NULL;
		ScDBCollection* pUndoDB = NULL;

		if (bRecord)
		{
			SCTAB nTabCount = pDoc->GetTableCount();
			pUndoDoc = new ScDocument( SCDOCMODE_UNDO );
			ScOutlineTable* pTable = pDoc->GetOutlineTable( nTab );
			if (pTable)
			{
				pUndoTab = new ScOutlineTable( *pTable );

				SCCOLROW nOutStartCol;							// Zeilen/Spaltenstatus
				SCCOLROW nOutStartRow;
				SCCOLROW nOutEndCol;
				SCCOLROW nOutEndRow;
				pTable->GetColArray()->GetRange( nOutStartCol, nOutEndCol );
				pTable->GetRowArray()->GetRange( nOutStartRow, nOutEndRow );

				pUndoDoc->InitUndo( pDoc, nTab, nTab, sal_True, sal_True );
				pDoc->CopyToDocument( static_cast<SCCOL>(nOutStartCol), 0, nTab, static_cast<SCCOL>(nOutEndCol), MAXROW, nTab, IDF_NONE, sal_False, pUndoDoc );
				pDoc->CopyToDocument( 0, nOutStartRow, nTab, MAXCOL, nOutEndRow, nTab, IDF_NONE, sal_False, pUndoDoc );
			}
			else
				pUndoDoc->InitUndo( pDoc, nTab, nTab, sal_False, sal_True );

			//	Datenbereich sichern - incl. Filter-Ergebnis
			pDoc->CopyToDocument( 0,nStartRow,nTab, MAXCOL,nEndRow,nTab, IDF_ALL, sal_False, pUndoDoc );

			//	alle Formeln wegen Referenzen
			pDoc->CopyToDocument( 0,0,0, MAXCOL,MAXROW,nTabCount-1, IDF_FORMULA, sal_False, pUndoDoc );

			//	DB- und andere Bereiche
			ScRangeName* pDocRange = pDoc->GetRangeName();
			if (pDocRange->GetCount())
				pUndoRange = new ScRangeName( *pDocRange );
			ScDBCollection* pDocDB = pDoc->GetDBCollection();
			if (pDocDB->GetCount())
				pUndoDB = new ScDBCollection( *pDocDB );
		}

		if (bSort && bSubTotal)
		{
			//	Sortieren ohne SubTotals

			aSubTotalParam.bRemoveOnly = sal_True;		// wird unten wieder zurueckgesetzt
			DoSubTotals( aSubTotalParam, sal_False );
		}

		if (bSort)
		{
			pDBData->GetSortParam( aSortParam );			// Bereich kann sich geaendert haben
			Sort( aSortParam, sal_False, sal_False);
		}
		if (bQuery)
		{
			pDBData->GetQueryParam( aQueryParam );			// Bereich kann sich geaendert haben
			ScRange aAdvSource;
			if (pDBData->GetAdvancedQuerySource(aAdvSource))
			{
				pDoc->CreateQueryParam(
					aAdvSource.aStart.Col(), aAdvSource.aStart.Row(),
					aAdvSource.aEnd.Col(), aAdvSource.aEnd.Row(),
					aAdvSource.aStart.Tab(), aQueryParam );
				Query( aQueryParam, &aAdvSource, sal_False );
			}				
			else
				Query( aQueryParam, NULL, sal_False );

			//	bei nicht-inplace kann die Tabelle umgestellt worden sein
			if ( !aQueryParam.bInplace && aQueryParam.nDestTab != nTab )
				SetTabNo( nTab );
		}
		if (bSubTotal)
		{
			pDBData->GetSubTotalParam( aSubTotalParam );	// Bereich kann sich geaendert haben
			aSubTotalParam.bRemoveOnly = sal_False;
			DoSubTotals( aSubTotalParam, sal_False );
		}

		if (bRecord)
		{
			SCTAB nDummyTab;
            SCCOL nDummyCol;
			SCROW nDummyRow, nNewEndRow;
			pDBData->GetArea( nDummyTab, nDummyCol,nDummyRow, nDummyCol,nNewEndRow );

			const ScRange* pOld = NULL;
			const ScRange* pNew = NULL;
			if (bQuerySize)
			{
				ScDBData* pDest = pDoc->GetDBAtCursor( aQueryParam.nDestCol, aQueryParam.nDestRow,
														aQueryParam.nDestTab, sal_True );
				if (pDest)
				{
					pDest->GetArea( aNewQuery );
					pOld = &aOldQuery;
					pNew = &aNewQuery;
				}
			}

			GetViewData()->GetDocShell()->GetUndoManager()->AddUndoAction(
				new ScUndoRepeatDB( GetViewData()->GetDocShell(), nTab,
										nStartCol, nStartRow, nEndCol, nEndRow,
										nNewEndRow,
										nCurX, nCurY,
										pUndoDoc, pUndoTab,
										pUndoRange, pUndoDB,
										pOld, pNew ) );
		}

		GetViewData()->GetDocShell()->PostPaint( 0,0,nTab, MAXCOL,MAXROW,nTab,
													PAINT_GRID | PAINT_LEFT | PAINT_TOP | PAINT_SIZE );
	}
	else		// "Keine Operationen auszufuehren"
		ErrorMessage(STR_MSSG_REPEATDB_0);
}




