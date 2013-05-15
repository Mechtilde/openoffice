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
#include "precompiled_sw.hxx"


#include <hintids.hxx>
#include <doc.hxx>
#include <IDocumentUndoRedo.hxx>
#include <editsh.hxx>
#include <cntfrm.hxx>
#include <pam.hxx>
#include <swundo.hxx>		// fuer die UndoIds
#include <edimp.hxx>
#include <IMark.hxx>
#include <docary.hxx>
#include <SwRewriter.hxx>
#include <globals.hrc>

#include <comcore.hrc>
#include <list>

/************************************************************
 * Loeschen
 ************************************************************/

void SwEditShell::DeleteSel( SwPaM& rPam, sal_Bool* pUndo )
{
	// nur bei Selektion
	if( !rPam.HasMark() || *rPam.GetPoint() == *rPam.GetMark())
		return;

	// besteht eine Selection in einer Tabelle ?
	// dann nur den Inhalt der selektierten Boxen loeschen
	// jetzt gibt es 2 Faelle die beachtet werden muessen:
	//	1. Point und Mark stehen in einer Box, Selection normal loeschen
	//	2. Point und Mark stehen in unterschiedlichen Boxen, alle
	// selektierten Boxen suchen in den Inhalt loeschen

	//Comment:If the point is outside of a table and the mark point is in the a table cell,
	//			should go throw the following code
	if( (rPam.GetNode()->FindTableNode() || rPam.GetNode(sal_False)->FindTableNode()) &&
		rPam.GetNode()->StartOfSectionNode() !=
		rPam.GetNode(sal_False)->StartOfSectionNode() )
	{
		// in Tabellen das Undo gruppieren
		if( pUndo && !*pUndo )
        {
            GetDoc()->GetIDocumentUndoRedo().StartUndo( UNDO_START, NULL );
			*pUndo = sal_True;
		}
		SwPaM aDelPam( *rPam.Start() );
		const SwPosition* pEndSelPos = rPam.End();
		do {
			aDelPam.SetMark();
			SwNode* pNd = aDelPam.GetNode();
			//Comment:If the point is outside of table, select the table start node as the end node of current selection node
			const SwNode& rEndNd = !rPam.GetNode()->FindTableNode() && !pNd->FindTableNode()?
						*(SwNode*)(rPam.GetNode(sal_False)->FindTableNode())
						:
						*pNd->EndOfSectionNode();
			if( pEndSelPos->nNode.GetIndex() <= rEndNd.GetIndex() )
			{
				*aDelPam.GetPoint() = *pEndSelPos;
				pEndSelPos = 0;		// Pointer als Flag missbrauchen
			}
			else
			{
				// dann ans Ende der Section
				aDelPam.GetPoint()->nNode = rEndNd;
				aDelPam.Move( fnMoveBackward, fnGoCntnt );
			}
				// geschuetze Boxen ueberspringen !
			//For i117395, in some situation, the node would be hidden or invisible, which makes the frame of it unavailable
			//So verify it before use it.
			SwCntntFrm* pFrm = NULL;
			if( !pNd->IsCntntNode() ||
				!((pFrm=((SwCntntNode*)pNd)->getLayoutFrm( GetLayout() ))!=NULL && pFrm->IsProtected()) )
			{
				// alles loeschen
				GetDoc()->DeleteAndJoin( aDelPam );
				SaveTblBoxCntnt( aDelPam.GetPoint() );
			}

			if( !pEndSelPos )				// am Ende der Selection
				break;
			aDelPam.DeleteMark();
			aDelPam.Move( fnMoveForward, fnGoCntnt );	// naechste Box
		} while( pEndSelPos );
	}
	else
	{
			// alles loeschen		
		GetDoc()->DeleteAndJoin( rPam );
		SaveTblBoxCntnt( rPam.GetPoint() );
	}

	// Selection wird nicht mehr benoetigt.
	rPam.DeleteMark();
}


long SwEditShell::Delete()
{
	SET_CURR_SHELL( this );
	long nRet = 0;
	if( !HasReadonlySel() )
	{
		StartAllAction();

		sal_Bool bUndo = GetCrsr()->GetNext() != GetCrsr();
		if( bUndo )     // mehr als eine Selection ?
        {
            SwRewriter aRewriter;
            aRewriter.AddRule(UNDO_ARG1, String(SW_RES(STR_MULTISEL)));

            GetDoc()->GetIDocumentUndoRedo().StartUndo(UNDO_DELETE, &aRewriter);
        }

		FOREACHPAM_START(this)
			DeleteSel( *PCURCRSR, &bUndo );
		FOREACHPAM_END()

		// falls eine Undo-Klammerung, dann hier beenden
		if( bUndo )
        {
            GetDoc()->GetIDocumentUndoRedo().EndUndo(UNDO_END, 0);
        }
		EndAllAction();
		nRet = 1;
	}
	return nRet;
}

long SwEditShell::Copy( SwEditShell* pDestShell )
{
	if( !pDestShell )
		pDestShell = this;

	SET_CURR_SHELL( pDestShell );

	// List of insert positions for smart insert of block selections
	std::list< boost::shared_ptr<SwPosition> > aInsertList;

	// Fill list of insert positions
	{
		SwPosition * pPos = 0;
		boost::shared_ptr<SwPosition> pInsertPos;
		sal_uInt16 nMove = 0;
		FOREACHPAM_START(this)

			if( !pPos )
			{
				if( pDestShell == this )
				{
					// First cursor represents the target position!!
					PCURCRSR->DeleteMark();
					pPos = (SwPosition*)PCURCRSR->GetPoint();
					continue;
				}
				else
					pPos = pDestShell->GetCrsr()->GetPoint();
			}
			if( IsBlockMode() )
			{	// In block mode different insert positions will be calculated
				// by simulated cursor movements from the given first insert position
				if( nMove )
				{
					SwCursor aCrsr( *pPos, 0, false);
					if( aCrsr.UpDown( sal_False, nMove, 0, 0 ) )
					{
						pInsertPos.reset( new SwPosition( *aCrsr.GetPoint() ) );
						aInsertList.push_back( pInsertPos );
					}
				}
				else
					pInsertPos.reset( new SwPosition( *pPos ) );
				++nMove;
			}
			SwPosition *pTmp = IsBlockMode() ? pInsertPos.get() : pPos;
			// Check if a selection would be copied into itself
			if( pDestShell->GetDoc() == GetDoc() && 
				*PCURCRSR->Start() <= *pTmp && *pTmp < *PCURCRSR->End() )
				return sal_False;
		FOREACHPAM_END()
	}

	pDestShell->StartAllAction();
	SwPosition *pPos = 0;
	sal_Bool bRet = sal_False;
	sal_Bool bFirstMove = sal_True;
	SwNodeIndex aSttNdIdx( pDestShell->GetDoc()->GetNodes() );
	xub_StrLen nSttCntIdx = 0;
	// For block selection this list is filled with the insert positions
	std::list< boost::shared_ptr<SwPosition> >::iterator pNextInsert = aInsertList.begin();

    pDestShell->GetDoc()->GetIDocumentUndoRedo().StartUndo( UNDO_START, NULL );
	FOREACHPAM_START(this)

		if( !pPos )
		{
			if( pDestShell == this )
			{
				// First cursor represents the target position!!
				PCURCRSR->DeleteMark();
				pPos = (SwPosition*)PCURCRSR->GetPoint();
				continue;
			}
			else
				pPos = pDestShell->GetCrsr()->GetPoint();
		}
		if( !bFirstMove )
		{
			if( pNextInsert != aInsertList.end() )
			{
				pPos = pNextInsert->get();
				++pNextInsert;
			}
			else if( IsBlockMode() )
				GetDoc()->SplitNode( *pPos, false );
		}

		// nur bei Selektion (nicht Textnodes haben Selection,
		// aber Point/GetMark sind gleich
		if( !PCURCRSR->HasMark() || *PCURCRSR->GetPoint() == *PCURCRSR->GetMark() )
			continue;

		if( bFirstMove )
		{
			// Anfangs-Position vom neuen Bereich merken
			aSttNdIdx = pPos->nNode.GetIndex()-1;
			nSttCntIdx = pPos->nContent.GetIndex();
			bFirstMove = sal_False;
		}

        const bool bSuccess( GetDoc()->CopyRange( *PCURCRSR, *pPos, false ) );
        if (!bSuccess)
			continue;

        SwPaM aInsertPaM(*pPos, SwPosition(aSttNdIdx));
        pDestShell->GetDoc()->MakeUniqueNumRules(aInsertPaM);

		bRet = sal_True;
	FOREACHPAM_END()


	// Maybe nothing has been moved?
	if( !bFirstMove )
	{
		SwPaM* pCrsr = pDestShell->GetCrsr();
		pCrsr->SetMark();
		pCrsr->GetPoint()->nNode = aSttNdIdx.GetIndex()+1;
		pCrsr->GetPoint()->nContent.Assign( pCrsr->GetCntntNode(),nSttCntIdx);
		pCrsr->Exchange();
	}
	else
	{
		// falls beim Move der Cursor "gewandert" ist, so setze hier auch
		// seinen GetMark um, damit dieser nie in den Wald zeigt.
		pDestShell->GetCrsr()->SetMark();
		pDestShell->GetCrsr()->DeleteMark();
	}
#if OSL_DEBUG_LEVEL > 1
// pruefe ob die Indizies auch in den richtigen Nodes angemeldet sind
{
	SwPaM* pCmp = (SwPaM*)pDestShell->GetCrsr();        // sicher den Pointer auf Cursor
	do {
		ASSERT( pCmp->GetPoint()->nContent.GetIdxReg()
					== pCmp->GetCntntNode(), "Point im falschen Node" );
		ASSERT( pCmp->GetMark()->nContent.GetIdxReg()
					== pCmp->GetCntntNode(sal_False), "Mark im falschen Node" );
		sal_Bool bTst = *pCmp->GetPoint() == *pCmp->GetMark();
        (void) bTst;
	} while( pDestShell->GetCrsr() != ( pCmp = (SwPaM*)pCmp->GetNext() ) );
}
#endif

	// Undo-Klammerung hier beenden
    pDestShell->GetDoc()->GetIDocumentUndoRedo().EndUndo( UNDO_END, NULL );
	pDestShell->EndAllAction();

	pDestShell->SaveTblBoxCntnt( pDestShell->GetCrsr()->GetPoint() );

	return (long)bRet;
}


	// Ersetz einen selektierten Bereich in einem TextNode mit dem
	// String. Ist fuers Suchen&Ersetzen gedacht.
	// bRegExpRplc - ersetze Tabs (\\t) und setze den gefundenen String
	//				 ein ( nicht \& )
	// 				z.B.: Fnd: "zzz", Repl: "xx\t\\t..&..\&"
	//						--> "xx\t<Tab>..zzz..&"
sal_Bool SwEditShell::Replace( const String& rNewStr, sal_Bool bRegExpRplc )
{
	SET_CURR_SHELL( this );

	sal_Bool bRet = sal_False;
	if( !HasReadonlySel() )
	{
		StartAllAction();
        GetDoc()->GetIDocumentUndoRedo().StartUndo(UNDO_EMPTY, NULL);

		FOREACHPAM_START(this)
			if( PCURCRSR->HasMark() && *PCURCRSR->GetMark() != *PCURCRSR->GetPoint() )
			{
                bRet = GetDoc()->ReplaceRange( *PCURCRSR, rNewStr, bRegExpRplc )
                    || bRet;
				SaveTblBoxCntnt( PCURCRSR->GetPoint() );
			}
		FOREACHPAM_END()

		// Undo-Klammerung hier beenden
        GetDoc()->GetIDocumentUndoRedo().EndUndo(UNDO_EMPTY, NULL);
		EndAllAction();
	}
	return bRet;
}


	// Special-Methode fuer JOE's- Wizzards
sal_Bool SwEditShell::DelFullPara()
{
	sal_Bool bRet = sal_False;
	if( !IsTableMode() )
	{
		SwPaM* pCrsr = GetCrsr();
		// keine Mehrfach-Selection
		if( pCrsr->GetNext() == pCrsr && !HasReadonlySel() )
		{
			SET_CURR_SHELL( this );
			StartAllAction();
			bRet = GetDoc()->DelFullPara( *pCrsr );
			EndAllAction();
		}
	}
	return bRet;
}



