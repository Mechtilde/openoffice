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

#include <svx/svdoutl.hxx>
#include <svx/svdmodel.hxx>
#include <svx/svdpage.hxx>
#include <svx/svdocapt.hxx>
#include <sfx2/printer.hxx>
#include <unotools/pathoptions.hxx>
#include <svl/itempool.hxx>
#include <vcl/svapp.hxx>

#include "notemark.hxx"
#include "document.hxx"
#include "postit.hxx"

#define SC_NOTEMARK_TIME	800
#define SC_NOTEMARK_SHORT	70

// -----------------------------------------------------------------------

ScNoteMarker::ScNoteMarker( Window* pWin, Window* pRight, Window* pBottom, Window* pDiagonal,
							ScDocument* pD, ScAddress aPos, const String& rUser,
							const MapMode& rMap, sal_Bool bLeftEdge, sal_Bool bForce, sal_Bool bKeyboard ) :
	pWindow( pWin ),
	pRightWin( pRight ),
	pBottomWin( pBottom ),
	pDiagWin( pDiagonal ),
	pDoc( pD ),
	aDocPos( aPos ),
	aUserText( rUser ),
	aMapMode( rMap ),
	bLeft( bLeftEdge ),
	bByKeyboard( bKeyboard ),
	pModel( NULL ),
	pObject( NULL ),
	bVisible( sal_False )
{
    Size aSizePixel = pWindow->GetOutputSizePixel();
    if( pRightWin )
        aSizePixel.Width() += pRightWin->GetOutputSizePixel().Width();
    if( pBottomWin )
        aSizePixel.Height() += pBottomWin->GetOutputSizePixel().Height();
    Rectangle aVisPixel( Point( 0, 0 ), aSizePixel );
    aVisRect = pWindow->PixelToLogic( aVisPixel, aMapMode );

	aTimer.SetTimeoutHdl( LINK( this, ScNoteMarker, TimeHdl ) );
	aTimer.SetTimeout( bForce ? SC_NOTEMARK_SHORT : SC_NOTEMARK_TIME );
	aTimer.Start();
}

ScNoteMarker::~ScNoteMarker()
{
	InvalidateWin();

	delete pModel;
}

IMPL_LINK( ScNoteMarker, TimeHdl, Timer*, EMPTYARG )
{
	if (!bVisible)
	{
		SvtPathOptions aPathOpt;
		String aPath = aPathOpt.GetPalettePath();
		pModel = new SdrModel(aPath);
		pModel->SetScaleUnit(MAP_100TH_MM);
		SfxItemPool& rPool = pModel->GetItemPool();
		rPool.SetDefaultMetric(SFX_MAPUNIT_100TH_MM);
		rPool.FreezeIdRanges();

        OutputDevice* pPrinter = pDoc->GetRefDevice();
		if (pPrinter)
		{
			//	Am Outliner des Draw-Model ist auch der Drucker als RefDevice gesetzt,
			//	und es soll einheitlich aussehen.
			Outliner& rOutliner = pModel->GetDrawOutliner();
			rOutliner.SetRefDevice(pPrinter);
		}

		if( SdrPage* pPage = pModel->AllocPage( sal_False ) )
        {
            pObject = ScNoteUtil::CreateTempCaption( *pDoc, aDocPos, *pPage, aUserText, aVisRect, bLeft );
            if( pObject )
                aRect = pObject->GetCurrentBoundRect();

            // #39351# Page einfuegen damit das Model sie kennt und auch deleted
            pModel->InsertPage( pPage );

        }
        bVisible = sal_True;
	}

	Draw();
	return 0;
}

void lcl_DrawWin( SdrObject* pObject, Window* pWindow, const MapMode& rMap )
{
	MapMode aOld = pWindow->GetMapMode();
	pWindow->SetMapMode( rMap );

	sal_uLong nOldDrawMode = pWindow->GetDrawMode();
	if ( Application::GetSettings().GetStyleSettings().GetHighContrastMode() )
	{
		pWindow->SetDrawMode( nOldDrawMode | DRAWMODE_SETTINGSLINE | DRAWMODE_SETTINGSFILL |
							DRAWMODE_SETTINGSTEXT | DRAWMODE_SETTINGSGRADIENT );
	}

	pObject->SingleObjectPainter( *pWindow ); // #110094#-17

	pWindow->SetDrawMode( nOldDrawMode );
	pWindow->SetMapMode( aOld );
}

MapMode lcl_MoveMapMode( const MapMode& rMap, const Size& rMove )
{
	MapMode aNew = rMap;
	Point aOrigin = aNew.GetOrigin();
	aOrigin.X() -= rMove.Width();
	aOrigin.Y() -= rMove.Height();
	aNew.SetOrigin(aOrigin);
	return aNew;
}

void ScNoteMarker::Draw()
{
	if ( pObject && bVisible )
	{
		lcl_DrawWin( pObject, pWindow, aMapMode );

		if ( pRightWin || pBottomWin )
		{
			Size aWinSize = pWindow->PixelToLogic( pWindow->GetOutputSizePixel(), aMapMode );
			if ( pRightWin )
				lcl_DrawWin( pObject, pRightWin,
								lcl_MoveMapMode( aMapMode, Size( aWinSize.Width(), 0 ) ) );
			if ( pBottomWin )
				lcl_DrawWin( pObject, pBottomWin,
								lcl_MoveMapMode( aMapMode, Size( 0, aWinSize.Height() ) ) );
			if ( pDiagWin )
				lcl_DrawWin( pObject, pDiagWin, lcl_MoveMapMode( aMapMode, aWinSize ) );
		}
	}
}

void ScNoteMarker::InvalidateWin()
{
	if (bVisible)
	{
		pWindow->Invalidate( pWindow->LogicToLogic(aRect, aMapMode, pWindow->GetMapMode()) );

		if ( pRightWin || pBottomWin )
		{
			Size aWinSize = pWindow->PixelToLogic( pWindow->GetOutputSizePixel(), aMapMode );
			if ( pRightWin )
				pRightWin->Invalidate( pRightWin->LogicToLogic(aRect,
										lcl_MoveMapMode( aMapMode, Size( aWinSize.Width(), 0 ) ),
										pRightWin->GetMapMode()) );
			if ( pBottomWin )
				pBottomWin->Invalidate( pBottomWin->LogicToLogic(aRect,
										lcl_MoveMapMode( aMapMode, Size( 0, aWinSize.Height() ) ),
										pBottomWin->GetMapMode()) );
			if ( pDiagWin )
				pDiagWin->Invalidate( pDiagWin->LogicToLogic(aRect,
										lcl_MoveMapMode( aMapMode, aWinSize ),
										pDiagWin->GetMapMode()) );
		}
	}
}




