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

#include <svx/imapdlg.hxx>
#include <sfx2/viewfrm.hxx>


sal_uInt16 ScIMapChildWindowId()
{
	return SvxIMapDlgChildWindow::GetChildWindowId();
}

SvxIMapDlg* ScGetIMapDlg()
{
	//!	pass view frame here and in SVXIMAPDLG()

	SfxViewFrame* pViewFrm = SfxViewFrame::Current();
	if( pViewFrm && pViewFrm->HasChildWindow( SvxIMapDlgChildWindow::GetChildWindowId() ) )
		return SVXIMAPDLG();
	else
		return NULL;
}

void ScIMapDlgSet( const Graphic& rGraphic, const ImageMap* pImageMap,
					const TargetList* pTargetList, void* pEditingObj )
{
	SvxIMapDlgChildWindow::UpdateIMapDlg( rGraphic, pImageMap, pTargetList, pEditingObj );
}

const void* ScIMapDlgGetObj( SvxIMapDlg* pDlg )
{
	if ( pDlg )
		return pDlg->GetEditingObject();
	else
		return NULL;
}

const ImageMap&	ScIMapDlgGetMap( SvxIMapDlg* pDlg )
{
	return pDlg->GetImageMap();
}




