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

#if STLPORT_VERSION<321
#include <stddef.h>
#else
#include <cstddef>
#endif
#include <cstdio>

#include <string.h>
#include <tools/mempool.hxx>
#include <tools/debug.hxx>

#include "token.hxx"
#include "tokenarray.hxx"
#include "compiler.hxx"
#include <formula/compiler.hrc>
#include "rechead.hxx"
#include "parclass.hxx"
#include "jumpmatrix.hxx"
#include "rangeseq.hxx"
#include "externalrefmgr.hxx"
#include "document.hxx"

using ::std::vector;

#include <com/sun/star/sheet/ComplexReference.hpp>
#include <com/sun/star/sheet/ExternalReference.hpp>
#include <com/sun/star/sheet/ReferenceFlags.hpp>

using namespace formula;
using namespace com::sun::star;

namespace
{
    void lcl_SingleRefToCalc( ScSingleRefData& rRef, const sheet::SingleReference& rAPI )
    {
        rRef.InitFlags();

        rRef.nCol    = static_cast<SCsCOL>(rAPI.Column);
        rRef.nRow    = static_cast<SCsROW>(rAPI.Row);
        rRef.nTab    = static_cast<SCsTAB>(rAPI.Sheet);
        rRef.nRelCol = static_cast<SCsCOL>(rAPI.RelativeColumn);
        rRef.nRelRow = static_cast<SCsROW>(rAPI.RelativeRow);
        rRef.nRelTab = static_cast<SCsTAB>(rAPI.RelativeSheet);

        rRef.SetColRel(     ( rAPI.Flags & sheet::ReferenceFlags::COLUMN_RELATIVE ) != 0 );
        rRef.SetRowRel(     ( rAPI.Flags & sheet::ReferenceFlags::ROW_RELATIVE    ) != 0 );
        rRef.SetTabRel(     ( rAPI.Flags & sheet::ReferenceFlags::SHEET_RELATIVE  ) != 0 );
        rRef.SetColDeleted( ( rAPI.Flags & sheet::ReferenceFlags::COLUMN_DELETED  ) != 0 );
        rRef.SetRowDeleted( ( rAPI.Flags & sheet::ReferenceFlags::ROW_DELETED     ) != 0 );
        rRef.SetTabDeleted( ( rAPI.Flags & sheet::ReferenceFlags::SHEET_DELETED   ) != 0 );
        rRef.SetFlag3D(     ( rAPI.Flags & sheet::ReferenceFlags::SHEET_3D        ) != 0 );
        rRef.SetRelName(    ( rAPI.Flags & sheet::ReferenceFlags::RELATIVE_NAME   ) != 0 );
    }
    
    void lcl_ExternalRefToCalc( ScSingleRefData& rRef, const sheet::SingleReference& rAPI )
    {
        rRef.InitFlags();

        rRef.nCol    = static_cast<SCsCOL>(rAPI.Column);
        rRef.nRow    = static_cast<SCsROW>(rAPI.Row);
        rRef.nTab    = 0;
        rRef.nRelCol = static_cast<SCsCOL>(rAPI.RelativeColumn);
        rRef.nRelRow = static_cast<SCsROW>(rAPI.RelativeRow);
        rRef.nRelTab = 0;

        rRef.SetColRel(     ( rAPI.Flags & sheet::ReferenceFlags::COLUMN_RELATIVE ) != 0 );
        rRef.SetRowRel(     ( rAPI.Flags & sheet::ReferenceFlags::ROW_RELATIVE    ) != 0 );
        rRef.SetTabRel(     false );    // sheet index must be absolute for external refs
        rRef.SetColDeleted( ( rAPI.Flags & sheet::ReferenceFlags::COLUMN_DELETED  ) != 0 );
        rRef.SetRowDeleted( ( rAPI.Flags & sheet::ReferenceFlags::ROW_DELETED     ) != 0 );
        rRef.SetTabDeleted( false );    // sheet must not be deleted for external refs
        rRef.SetFlag3D(     ( rAPI.Flags & sheet::ReferenceFlags::SHEET_3D        ) != 0 );
        rRef.SetRelName(    false );
    }
//
} // namespace
// 
// ImpTokenIterator wird je Interpreter angelegt, mehrfache auch durch
// SubCode via FormulaTokenIterator Push/Pop moeglich
IMPL_FIXEDMEMPOOL_NEWDEL( ImpTokenIterator, 32, 16 )

// Align MemPools on 4k boundaries - 64 bytes (4k is a MUST for OS/2)

// Since RawTokens are temporary for the compiler, don't align on 4k and waste memory.
// ScRawToken size is FixMembers + MAXSTRLEN + ~4 ~= 1036
IMPL_FIXEDMEMPOOL_NEWDEL( ScRawToken, 8, 4 )
// Some ScDoubleRawToken, FixMembers + sizeof(double) ~= 16
const sal_uInt16 nMemPoolDoubleRawToken = 0x0400 / sizeof(ScDoubleRawToken);
IMPL_FIXEDMEMPOOL_NEWDEL( ScDoubleRawToken, nMemPoolDoubleRawToken, nMemPoolDoubleRawToken )

// Need a whole bunch of ScSingleRefToken
const sal_uInt16 nMemPoolSingleRefToken = (0x4000 - 64) / sizeof(ScSingleRefToken);
IMPL_FIXEDMEMPOOL_NEWDEL( ScSingleRefToken, nMemPoolSingleRefToken, nMemPoolSingleRefToken )
// Need quite a lot of ScDoubleRefToken
const sal_uInt16 nMemPoolDoubleRefToken = (0x2000 - 64) / sizeof(ScDoubleRefToken);
IMPL_FIXEDMEMPOOL_NEWDEL( ScDoubleRefToken, nMemPoolDoubleRefToken, nMemPoolDoubleRefToken )

// --- helpers --------------------------------------------------------------

inline sal_Bool lcl_IsReference( OpCode eOp, StackVar eType )
{
    return
        (eOp == ocPush && (eType == svSingleRef || eType == svDoubleRef))
        || (eOp == ocColRowNameAuto && eType == svDoubleRef)
        || (eOp == ocColRowName && eType == svSingleRef)
        || (eOp == ocMatRef && eType == svSingleRef)
        ;
}


// --- class ScRawToken -----------------------------------------------------

xub_StrLen ScRawToken::GetStrLen( const sal_Unicode* pStr )
{
    if ( !pStr )
        return 0;
    register const sal_Unicode* p = pStr;
    while ( *p )
        p++;
    return sal::static_int_cast<xub_StrLen>( p - pStr );
}


void ScRawToken::SetOpCode( OpCode e )
{
    eOp   = e;
    switch (eOp)
    {
        case ocIf:
            eType = svJump;
            nJump[ 0 ] = 3; // If, Else, Behind
            break;
        case ocChose:
            eType = svJump;
            nJump[ 0 ] = MAXJUMPCOUNT+1;
            break;
        case ocMissing:
            eType = svMissing;
            break;
        case ocSep:
        case ocOpen:
        case ocClose:
        case ocArrayRowSep:
        case ocArrayColSep:
        case ocArrayOpen:
        case ocArrayClose:
            eType = svSep;
            break;
        default:
            eType = svByte;
            sbyte.cByte = 0;
            sbyte.bHasForceArray = ScParameterClassification::HasForceArray( eOp);
    }
    nRefCnt = 0;
}

void ScRawToken::SetString( const sal_Unicode* pStr )
{
    eOp   = ocPush;
    eType = svString;
    if ( pStr )
    {
        xub_StrLen nLen = GetStrLen( pStr ) + 1;
        if( nLen > MAXSTRLEN )
            nLen = MAXSTRLEN;
        memcpy( cStr, pStr, GetStrLenBytes( nLen ) );
        cStr[ nLen-1 ] = 0;
    }
    else
        cStr[0] = 0;
    nRefCnt = 0;
}

void ScRawToken::SetSingleReference( const ScSingleRefData& rRef )
{
    eOp       = ocPush;
    eType     = svSingleRef;
    aRef.Ref1 =
    aRef.Ref2 = rRef;
    nRefCnt   = 0;
}

void ScRawToken::SetDoubleReference( const ScComplexRefData& rRef )
{
    eOp   = ocPush;
    eType = svDoubleRef;
    aRef  = rRef;
    nRefCnt = 0;
}

void ScRawToken::SetDouble(double rVal)
{
    eOp   = ocPush;
    eType = svDouble;
    nValue = rVal;
    nRefCnt = 0;
}

void ScRawToken::SetName( sal_uInt16 n )
{
    eOp    = ocName;
    eType  = svIndex;
    nIndex = n;
    nRefCnt = 0;
}

void ScRawToken::SetExternalSingleRef( sal_uInt16 nFileId, const String& rTabName, const ScSingleRefData& rRef )
{
    eOp = ocExternalRef;
    eType = svExternalSingleRef;
    nRefCnt = 0;

    extref.nFileId = nFileId;
    extref.aRef.Ref1 = 
    extref.aRef.Ref2 = rRef;

    xub_StrLen n = rTabName.Len();
    memcpy(extref.cTabName, rTabName.GetBuffer(), n*sizeof(sal_Unicode));
    extref.cTabName[n] = 0;
}

void ScRawToken::SetExternalDoubleRef( sal_uInt16 nFileId, const String& rTabName, const ScComplexRefData& rRef )
{
    eOp = ocExternalRef;
    eType = svExternalDoubleRef;
    nRefCnt = 0;

    extref.nFileId = nFileId;
    extref.aRef = rRef;

    xub_StrLen n = rTabName.Len();
    memcpy(extref.cTabName, rTabName.GetBuffer(), n*sizeof(sal_Unicode));
    extref.cTabName[n] = 0;
}

void ScRawToken::SetExternalName( sal_uInt16 nFileId, const String& rName )
{
    eOp = ocExternalRef;
    eType = svExternalName;
    nRefCnt = 0;

    extname.nFileId = nFileId;

    xub_StrLen n = rName.Len();
    memcpy(extname.cName, rName.GetBuffer(), n*sizeof(sal_Unicode));
    extname.cName[n] = 0;
}

//UNUSED2008-05  void ScRawToken::SetInt(int rVal)
//UNUSED2008-05  {
//UNUSED2008-05      eOp   = ocPush;
//UNUSED2008-05      eType = svDouble;
//UNUSED2008-05      nValue = (double)rVal;
//UNUSED2008-05      nRefCnt = 0;
//UNUSED2008-05  
//UNUSED2008-05  }
//UNUSED2008-05  void ScRawToken::SetMatrix( ScMatrix* p )
//UNUSED2008-05  {
//UNUSED2008-05      eOp   = ocPush;
//UNUSED2008-05      eType = svMatrix;
//UNUSED2008-05      pMat  = p;
//UNUSED2008-05      nRefCnt = 0;
//UNUSED2008-05  }
//UNUSED2008-05  
//UNUSED2008-05  ScComplexRefData& ScRawToken::GetReference()
//UNUSED2008-05  {
//UNUSED2008-05      DBG_ASSERT( lcl_IsReference( eOp, GetType() ), "GetReference: no Ref" );
//UNUSED2008-05      return aRef;
//UNUSED2008-05  }
//UNUSED2008-05  
//UNUSED2008-05  void ScRawToken::SetReference( ScComplexRefData& rRef )
//UNUSED2008-05  {
//UNUSED2008-05      DBG_ASSERT( lcl_IsReference( eOp, GetType() ), "SetReference: no Ref" );
//UNUSED2008-05      aRef = rRef;
//UNUSED2008-05      if( GetType() == svSingleRef )
//UNUSED2008-05          aRef.Ref2 = aRef.Ref1;
//UNUSED2008-05  }

void ScRawToken::SetExternal( const sal_Unicode* pStr )
{
    eOp   = ocExternal;
    eType = svExternal;
    xub_StrLen nLen = GetStrLen( pStr ) + 1;
    if( nLen >= MAXSTRLEN )
        nLen = MAXSTRLEN-1;
    // Platz fuer Byte-Parameter lassen!
    memcpy( cStr+1, pStr, GetStrLenBytes( nLen ) );
    cStr[ nLen+1 ] = 0;
    nRefCnt = 0;
}

sal_uInt16 lcl_ScRawTokenOffset()
{
    // offset of sbyte in ScRawToken
    // offsetof(ScRawToken, sbyte) gives a warning with gcc, because ScRawToken is no POD

    ScRawToken aToken;
    return static_cast<sal_uInt16>( reinterpret_cast<char*>(&aToken.sbyte) - reinterpret_cast<char*>(&aToken) );
}

ScRawToken* ScRawToken::Clone() const
{
    ScRawToken* p;
    if ( eType == svDouble )
    {
        p = (ScRawToken*) new ScDoubleRawToken;
        p->eOp = eOp;
        p->eType = eType;
        p->nValue = nValue;
    }
    else
    {
        static sal_uInt16 nOffset = lcl_ScRawTokenOffset();     // offset of sbyte
        sal_uInt16 n = nOffset;

        if (eOp == ocExternalRef)
        {
            switch (eType)
            {
                case svExternalSingleRef:
                case svExternalDoubleRef: n += sizeof(extref); break;
                case svExternalName:      n += sizeof(extname); break;
                default:
                {
                    DBG_ERROR1( "unknown ScRawToken::Clone() external type %d", int(eType));
                }
            }
        }
        else
        {
            switch( eType )
            {
                case svSep:         break;
                case svByte:        n += sizeof(ScRawToken::sbyte); break;
                case svDouble:      n += sizeof(double); break;
                case svString:      n = sal::static_int_cast<sal_uInt16>( n + GetStrLenBytes( cStr ) + GetStrLenBytes( 1 ) ); break;
                case svSingleRef:
                case svDoubleRef:   n += sizeof(aRef); break;
                case svMatrix:      n += sizeof(ScMatrix*); break;
                case svIndex:       n += sizeof(sal_uInt16); break;
                case svJump:        n += nJump[ 0 ] * 2 + 2; break;
                case svExternal:    n = sal::static_int_cast<sal_uInt16>( n + GetStrLenBytes( cStr+1 ) + GetStrLenBytes( 2 ) ); break;
                default:
                {
                    DBG_ERROR1( "unknown ScRawToken::Clone() type %d", int(eType));
                }
            }
        }
        p = (ScRawToken*) new sal_uInt8[ n ];
        memcpy( p, this, n * sizeof(sal_uInt8) );
    }
    p->nRefCnt = 0;
    p->bRaw = sal_False;
    return p;
}


FormulaToken* ScRawToken::CreateToken() const
{
#ifdef DBG_UTIL
#define IF_NOT_OPCODE_ERROR(o,c) if (eOp!=o) DBG_ERROR1( #c "::ctor: OpCode %d lost, converted to " #o "; maybe inherit from FormulaToken instead!", int(eOp))
#else
#define IF_NOT_OPCODE_ERROR(o,c)
#endif
    switch ( GetType() )
    {
        case svByte :
            return new FormulaByteToken( eOp, sbyte.cByte, sbyte.bHasForceArray );
        case svDouble :
            IF_NOT_OPCODE_ERROR( ocPush, FormulaDoubleToken);
            return new FormulaDoubleToken( nValue );
        case svString :
            if (eOp == ocPush)
                return new FormulaStringToken( String( cStr ) );
            else
                return new FormulaStringOpToken( eOp, String( cStr ) );
        case svSingleRef :
            if (eOp == ocPush)
                return new ScSingleRefToken( aRef.Ref1 );
            else
                return new ScSingleRefToken( aRef.Ref1, eOp );
        case svDoubleRef :
            if (eOp == ocPush)
                return new ScDoubleRefToken( aRef );
            else
                return new ScDoubleRefToken( aRef, eOp );
        case svMatrix :
            IF_NOT_OPCODE_ERROR( ocPush, ScMatrixToken);
            return new ScMatrixToken( pMat );
        case svIndex :
            return new FormulaIndexToken( eOp, nIndex );
        case svExternalSingleRef:
            {
                String aTabName(extref.cTabName);
                return new ScExternalSingleRefToken(extref.nFileId, aTabName, extref.aRef.Ref1);
            }
        case svExternalDoubleRef:
            {
                String aTabName(extref.cTabName);
                return new ScExternalDoubleRefToken(extref.nFileId, aTabName, extref.aRef);
            }
        case svExternalName:
            {
                String aName(extname.cName);
                return new ScExternalNameToken( extname.nFileId, aName );
            }
        case svJump :
            return new FormulaJumpToken( eOp, (short*) nJump );
        case svExternal :
            return new FormulaExternalToken( eOp, sbyte.cByte, String( cStr+1 ) );
        case svFAP :
            return new FormulaFAPToken( eOp, sbyte.cByte, NULL );
        case svMissing :
            IF_NOT_OPCODE_ERROR( ocMissing, FormulaMissingToken);
            return new FormulaMissingToken;
        case svSep :
            return new FormulaToken( svSep,eOp );
        case svUnknown :
            return new FormulaUnknownToken( eOp );
        default:
            {
                DBG_ERROR1( "unknown ScRawToken::CreateToken() type %d", int(GetType()));
                return new FormulaUnknownToken( ocBad );
            }
    }
#undef IF_NOT_OPCODE_ERROR
}


void ScRawToken::Delete()
{
    if ( bRaw )
        delete this;                            // FixedMemPool ScRawToken
    else
    {   // created per Clone
        switch ( eType )
        {
            case svDouble :
                delete (ScDoubleRawToken*) this;    // FixedMemPool ScDoubleRawToken
            break;
            default:
                delete [] (sal_uInt8*) this;
        }
    }
}


// --- class ScToken --------------------------------------------------------

ScSingleRefData lcl_ScToken_InitSingleRef()
{
    ScSingleRefData aRef;
    aRef.InitAddress( ScAddress() );
    return aRef;
}

ScComplexRefData lcl_ScToken_InitDoubleRef()
{
    ScComplexRefData aRef;
    aRef.Ref1 = lcl_ScToken_InitSingleRef();
    aRef.Ref2 = aRef.Ref1;
    return aRef;
}

ScToken::~ScToken()
{
}

//  TextEqual: if same formula entered (for optimization in sort)
sal_Bool ScToken::TextEqual( const FormulaToken& _rToken ) const
{
    if ( eType == svSingleRef || eType == svDoubleRef )
    {
        //  in relative Refs only compare relative parts

        if ( eType != _rToken.GetType() || GetOpCode() != _rToken.GetOpCode() )
            return sal_False;

        const ScToken& rToken = static_cast<const ScToken&>(_rToken);
        ScComplexRefData aTemp1;
        if ( eType == svSingleRef )
        {
            aTemp1.Ref1 = GetSingleRef();
            aTemp1.Ref2 = aTemp1.Ref1;
        }
        else
            aTemp1 = GetDoubleRef();

        ScComplexRefData aTemp2;
        if ( rToken.eType == svSingleRef )
        {
            aTemp2.Ref1 = rToken.GetSingleRef();
            aTemp2.Ref2 = aTemp2.Ref1;
        }
        else
            aTemp2 = rToken.GetDoubleRef();

        ScAddress aPos;
        aTemp1.SmartRelAbs(aPos);
        aTemp2.SmartRelAbs(aPos);

        //  memcmp doesn't work because of the alignment byte after bFlags.
        //  After SmartRelAbs only absolute parts have to be compared.
        return aTemp1.Ref1.nCol   == aTemp2.Ref1.nCol   &&
               aTemp1.Ref1.nRow   == aTemp2.Ref1.nRow   &&
               aTemp1.Ref1.nTab   == aTemp2.Ref1.nTab   &&
               aTemp1.Ref1.bFlags == aTemp2.Ref1.bFlags &&
               aTemp1.Ref2.nCol   == aTemp2.Ref2.nCol   &&
               aTemp1.Ref2.nRow   == aTemp2.Ref2.nRow   &&
               aTemp1.Ref2.nTab   == aTemp2.Ref2.nTab   &&
               aTemp1.Ref2.bFlags == aTemp2.Ref2.bFlags;
    }
    else
        return *this == _rToken;     // else normal operator==
}


sal_Bool ScToken::Is3DRef() const
{
    switch ( eType )
    {
        case svDoubleRef :
            if ( GetSingleRef2().IsFlag3D() )
                return sal_True;
        //! fallthru
        case svSingleRef :
            if ( GetSingleRef().IsFlag3D() )
                return sal_True;
            break;
        case svExternalSingleRef:
        case svExternalDoubleRef:
            return sal_True;
        default:
        {
            // added to avoid warnings
        }
    }
    return sal_False;
}

// static
FormulaTokenRef ScToken::ExtendRangeReference( FormulaToken & rTok1, FormulaToken & rTok2,
        const ScAddress & rPos, bool bReuseDoubleRef )
{
    
    StackVar sv1, sv2;
    // Doing a RangeOp with RefList is probably utter nonsense, but Xcl
    // supports it, so do we.
    if (((sv1 = rTok1.GetType()) != svSingleRef && sv1 != svDoubleRef && sv1 != svRefList &&
			sv1 != svExternalSingleRef && sv1 != svExternalDoubleRef ) ||
        ((sv2 = rTok2.GetType()) != svSingleRef && sv2 != svDoubleRef && sv2 != svRefList))
        return NULL;

    ScToken *p1 = static_cast<ScToken*>(&rTok1);
    ScToken *p2 = static_cast<ScToken*>(&rTok2);

    ScTokenRef xRes;
    bool bExternal = (sv1 == svExternalSingleRef);
    if ((sv1 == svSingleRef || bExternal) && sv2 == svSingleRef)
    {
        // Range references like Sheet1.A1:A2 are generalized and built by
        // first creating a DoubleRef from the first SingleRef, effectively
        // generating Sheet1.A1:A1, and then extending that with A2 as if
        // Sheet1.A1:A1:A2 was encountered, so the mechanisms to adjust the
        // references apply as well.

        /* Given the current structure of external references an external
         * reference can only be extended if the second reference does not
         * point to a different sheet. 'file'#Sheet1.A1:A2 is ok,
         * 'file'#Sheet1.A1:Sheet2.A2 is not. Since we can't determine from a
         * svSingleRef whether the sheet would be different from the one given
         * in the external reference, we have to bail out if there is any sheet
         * specified. NOTE: Xcl does handle external 3D references as in
         * '[file]Sheet1:Sheet2'!A1:A2
         *
         * FIXME: For OOo syntax be smart and remember an external singleref
         * encountered and if followed by ocRange and singleref, create an
         * external singleref for the second singleref. Both could then be
         * merged here. For Xcl syntax already parse an external range
         * reference entirely, cumbersome. */

        const ScSingleRefData& rRef2 = p2->GetSingleRef();
        if (bExternal && rRef2.IsFlag3D())
            return NULL;

        ScComplexRefData aRef;
        aRef.Ref1 = aRef.Ref2 = p1->GetSingleRef();
        aRef.Ref2.SetFlag3D( false);
        aRef.Extend( rRef2, rPos);
        if (bExternal)
            xRes = new ScExternalDoubleRefToken( p1->GetIndex(), p1->GetString(), aRef);
        else
            xRes = new ScDoubleRefToken( aRef);
    }
    else
    {
        bExternal |= (sv1 == svExternalDoubleRef);
        const ScRefList* pRefList = NULL;
        if (sv1 == svDoubleRef)
        {
            xRes = (bReuseDoubleRef && p1->GetRef() == 1 ? p1 : static_cast<ScToken*>(p1->Clone()));
            sv1 = svUnknown;    // mark as handled
        }
        else if (sv2 == svDoubleRef)
        {
            xRes = (bReuseDoubleRef && p2->GetRef() == 1 ? p2 : static_cast<ScToken*>(p2->Clone()));
            sv2 = svUnknown;    // mark as handled
        }
        else if (sv1 == svRefList)
            pRefList = p1->GetRefList();
        else if (sv2 == svRefList)
            pRefList = p2->GetRefList();
        if (pRefList)
        {
            if (!pRefList->size())
                return NULL;
            if (bExternal)
                return NULL;    // external reference list not possible
            xRes = new ScDoubleRefToken( (*pRefList)[0] );
        }
        if (!xRes)
            return NULL;    // shouldn't happen..
        StackVar sv[2] = { sv1, sv2 };
        ScToken* pt[2] = { p1, p2 };
        ScComplexRefData& rRef = xRes->GetDoubleRef();
        for (size_t i=0; i<2; ++i)
        {
            switch (sv[i])
            {
                case svSingleRef:
                    rRef.Extend( pt[i]->GetSingleRef(), rPos);
                    break;
                case svDoubleRef:
                    rRef.Extend( pt[i]->GetDoubleRef(), rPos);
                    break;
                case svRefList:
                    {
                        const ScRefList* p = pt[i]->GetRefList();
                        if (!p->size())
                            return NULL;
                        ScRefList::const_iterator it( p->begin());
                        ScRefList::const_iterator end( p->end());
                        for ( ; it != end; ++it)
                        {
                            rRef.Extend( *it, rPos);
                        }
                    }
                    break;
                case svExternalSingleRef:
                    if (rRef.Ref1.IsFlag3D() || rRef.Ref2.IsFlag3D())
                        return NULL;    // no other sheets with external refs
                    else
                        rRef.Extend( pt[i]->GetSingleRef(), rPos);
                    break;
                case svExternalDoubleRef:
                    if (rRef.Ref1.IsFlag3D() || rRef.Ref2.IsFlag3D())
                        return NULL;    // no other sheets with external refs
                    else
                        rRef.Extend( pt[i]->GetDoubleRef(), rPos);
                    break;
                default:
                    ;   // nothing, prevent compiler warning
            }
        }
    }
    return FormulaTokenRef(xRes.get());
}

const ScSingleRefData& ScToken::GetSingleRef() const
{
    DBG_ERRORFILE( "ScToken::GetSingleRef: virtual dummy called" );
    static ScSingleRefData aDummySingleRef = lcl_ScToken_InitSingleRef();
    return aDummySingleRef;
}

ScSingleRefData& ScToken::GetSingleRef()
{
    DBG_ERRORFILE( "ScToken::GetSingleRef: virtual dummy called" );
    static ScSingleRefData aDummySingleRef = lcl_ScToken_InitSingleRef();
    return aDummySingleRef;
}

const ScComplexRefData& ScToken::GetDoubleRef() const
{
    DBG_ERRORFILE( "ScToken::GetDoubleRef: virtual dummy called" );
    static ScComplexRefData aDummyDoubleRef = lcl_ScToken_InitDoubleRef();
    return aDummyDoubleRef;
}

ScComplexRefData& ScToken::GetDoubleRef()
{
    DBG_ERRORFILE( "ScToken::GetDoubleRef: virtual dummy called" );
    static ScComplexRefData aDummyDoubleRef = lcl_ScToken_InitDoubleRef();
    return aDummyDoubleRef;
}

const ScSingleRefData& ScToken::GetSingleRef2() const
{
    DBG_ERRORFILE( "ScToken::GetSingleRef2: virtual dummy called" );
    static ScSingleRefData aDummySingleRef = lcl_ScToken_InitSingleRef();
    return aDummySingleRef;
}

ScSingleRefData& ScToken::GetSingleRef2()
{
    DBG_ERRORFILE( "ScToken::GetSingleRef2: virtual dummy called" );
    static ScSingleRefData aDummySingleRef = lcl_ScToken_InitSingleRef();
    return aDummySingleRef;
}

void ScToken::CalcAbsIfRel( const ScAddress& /* rPos */ )
{
    DBG_ERRORFILE( "ScToken::CalcAbsIfRel: virtual dummy called" );
}

void ScToken::CalcRelFromAbs( const ScAddress& /* rPos */ )
{
    DBG_ERRORFILE( "ScToken::CalcRelFromAbs: virtual dummy called" );
}

const ScMatrix* ScToken::GetMatrix() const
{
    DBG_ERRORFILE( "ScToken::GetMatrix: virtual dummy called" );
    return NULL;
}

ScMatrix* ScToken::GetMatrix()
{
    DBG_ERRORFILE( "ScToken::GetMatrix: virtual dummy called" );
    return NULL;
}


ScJumpMatrix* ScToken::GetJumpMatrix() const
{
    DBG_ERRORFILE( "ScToken::GetJumpMatrix: virtual dummy called" );
    return NULL;
}
const ScRefList* ScToken::GetRefList() const
{
    DBG_ERRORFILE( "ScToken::GetRefList: virtual dummy called" );
    return NULL;
}

ScRefList* ScToken::GetRefList()
{
    DBG_ERRORFILE( "ScToken::GetRefList: virtual dummy called" );
    return NULL;
}
// ==========================================================================
// real implementations of virtual functions
// --------------------------------------------------------------------------




const ScSingleRefData&    ScSingleRefToken::GetSingleRef() const  { return aSingleRef; }
ScSingleRefData&          ScSingleRefToken::GetSingleRef()        { return aSingleRef; }
void                    ScSingleRefToken::CalcAbsIfRel( const ScAddress& rPos )
                            { aSingleRef.CalcAbsIfRel( rPos ); }
void                    ScSingleRefToken::CalcRelFromAbs( const ScAddress& rPos )
                            { aSingleRef.CalcRelFromAbs( rPos ); }
sal_Bool ScSingleRefToken::operator==( const FormulaToken& r ) const
{
    return FormulaToken::operator==( r ) && aSingleRef == static_cast<const ScToken&>(r).GetSingleRef();
}


const ScSingleRefData&    ScDoubleRefToken::GetSingleRef() const  { return aDoubleRef.Ref1; }
ScSingleRefData&          ScDoubleRefToken::GetSingleRef()        { return aDoubleRef.Ref1; }
const ScComplexRefData&     ScDoubleRefToken::GetDoubleRef() const  { return aDoubleRef; }
ScComplexRefData&           ScDoubleRefToken::GetDoubleRef()        { return aDoubleRef; }
const ScSingleRefData&    ScDoubleRefToken::GetSingleRef2() const { return aDoubleRef.Ref2; }
ScSingleRefData&          ScDoubleRefToken::GetSingleRef2()       { return aDoubleRef.Ref2; }
void                    ScDoubleRefToken::CalcAbsIfRel( const ScAddress& rPos )
                            { aDoubleRef.CalcAbsIfRel( rPos ); }
void                    ScDoubleRefToken::CalcRelFromAbs( const ScAddress& rPos )
                            { aDoubleRef.CalcRelFromAbs( rPos ); }
sal_Bool ScDoubleRefToken::operator==( const FormulaToken& r ) const
{
    return FormulaToken::operator==( r ) && aDoubleRef == static_cast<const ScToken&>(r).GetDoubleRef();
}


const ScRefList*        ScRefListToken::GetRefList() const  { return &aRefList; }
      ScRefList*        ScRefListToken::GetRefList()        { return &aRefList; }
void                    ScRefListToken::CalcAbsIfRel( const ScAddress& rPos )
{
    for (ScRefList::iterator it( aRefList.begin()); it != aRefList.end(); ++it)
        (*it).CalcAbsIfRel( rPos);
}
void                    ScRefListToken::CalcRelFromAbs( const ScAddress& rPos )
{
    for (ScRefList::iterator it( aRefList.begin()); it != aRefList.end(); ++it)
        (*it).CalcRelFromAbs( rPos);
}
sal_Bool ScRefListToken::operator==( const FormulaToken& r ) const
{
    return FormulaToken::operator==( r ) && &aRefList == static_cast<const ScToken&>(r).GetRefList();
}


const ScMatrix* ScMatrixToken::GetMatrix() const        { return pMatrix; }
ScMatrix*       ScMatrixToken::GetMatrix()              { return pMatrix; }
sal_Bool ScMatrixToken::operator==( const FormulaToken& r ) const
{
    return FormulaToken::operator==( r ) && pMatrix == static_cast<const ScToken&>(r).GetMatrix();
}

// ============================================================================

ScExternalSingleRefToken::ScExternalSingleRefToken( sal_uInt16 nFileId, const String& rTabName, const ScSingleRefData& r ) :
    ScToken( svExternalSingleRef, ocExternalRef),
    mnFileId(nFileId),
    maTabName(rTabName),
    maSingleRef(r)
{
}

ScExternalSingleRefToken::ScExternalSingleRefToken( const ScExternalSingleRefToken& r ) :
    ScToken(r), 
    mnFileId(r.mnFileId),
    maTabName(r.maTabName),
    maSingleRef(r.maSingleRef)
{
}

ScExternalSingleRefToken::~ScExternalSingleRefToken()
{
}

sal_uInt16 ScExternalSingleRefToken::GetIndex() const
{
    return mnFileId;
}

const String& ScExternalSingleRefToken::GetString() const
{
    return maTabName;
}

const ScSingleRefData& ScExternalSingleRefToken::GetSingleRef() const
{
    return maSingleRef;
}

ScSingleRefData& ScExternalSingleRefToken::GetSingleRef()
{
    return maSingleRef;
}

void ScExternalSingleRefToken::CalcAbsIfRel( const ScAddress& rPos )
{
    maSingleRef.CalcAbsIfRel( rPos );
}

void ScExternalSingleRefToken::CalcRelFromAbs( const ScAddress& rPos )
{
    maSingleRef.CalcRelFromAbs( rPos );
}

sal_Bool ScExternalSingleRefToken::operator ==( const FormulaToken& r ) const
{
    if (!FormulaToken::operator==(r))
        return false;

    if (mnFileId != r.GetIndex())
        return false;

    if (maTabName != r.GetString())
        return false;

    return maSingleRef == static_cast<const ScToken&>(r).GetSingleRef();
}

// ============================================================================

ScExternalDoubleRefToken::ScExternalDoubleRefToken( sal_uInt16 nFileId, const String& rTabName, const ScComplexRefData& r ) :
    ScToken( svExternalDoubleRef, ocExternalRef),
    mnFileId(nFileId),
    maTabName(rTabName),
    maDoubleRef(r)
{
}

ScExternalDoubleRefToken::ScExternalDoubleRefToken( const ScExternalDoubleRefToken& r ) :
    ScToken(r), 
    mnFileId(r.mnFileId),
    maTabName(r.maTabName),
    maDoubleRef(r.maDoubleRef)
{
}

ScExternalDoubleRefToken::ScExternalDoubleRefToken( const ScExternalSingleRefToken& r ) :
    ScToken( svExternalDoubleRef, ocExternalRef),
    mnFileId( r.GetIndex()),
    maTabName( r.GetString())
{
    maDoubleRef.Ref1 = maDoubleRef.Ref2 = r.GetSingleRef();
}

ScExternalDoubleRefToken::~ScExternalDoubleRefToken()
{
}

sal_uInt16 ScExternalDoubleRefToken::GetIndex() const
{
    return mnFileId;
}

const String& ScExternalDoubleRefToken::GetString() const
{
    return maTabName;
}

const ScSingleRefData& ScExternalDoubleRefToken::GetSingleRef() const
{
    return maDoubleRef.Ref1;
}

ScSingleRefData& ScExternalDoubleRefToken::GetSingleRef()
{
    return maDoubleRef.Ref1;
}

const ScSingleRefData& ScExternalDoubleRefToken::GetSingleRef2() const
{
    return maDoubleRef.Ref2;
}

ScSingleRefData& ScExternalDoubleRefToken::GetSingleRef2()
{
    return maDoubleRef.Ref2;
}

const ScComplexRefData& ScExternalDoubleRefToken::GetDoubleRef() const
{
    return maDoubleRef;
}

ScComplexRefData& ScExternalDoubleRefToken::GetDoubleRef()
{
    return maDoubleRef;
}

void ScExternalDoubleRefToken::CalcAbsIfRel( const ScAddress& rPos )
{
    maDoubleRef.CalcAbsIfRel( rPos );
}

void ScExternalDoubleRefToken::CalcRelFromAbs( const ScAddress& rPos )
{
    maDoubleRef.CalcRelFromAbs( rPos );
}

sal_Bool ScExternalDoubleRefToken::operator ==( const FormulaToken& r ) const
{
    if (!ScToken::operator==(r))
        return false;

    if (mnFileId != r.GetIndex())
        return false;

    if (maTabName != r.GetString())
        return false;

    return maDoubleRef == static_cast<const ScToken&>(r).GetDoubleRef();
}

// ============================================================================

ScExternalNameToken::ScExternalNameToken( sal_uInt16 nFileId, const String& rName ) :
    ScToken( svExternalName, ocExternalRef),
    mnFileId(nFileId),
    maName(rName)
{
}

ScExternalNameToken::ScExternalNameToken( const ScExternalNameToken& r ) :
    ScToken(r),
    mnFileId(r.mnFileId),
    maName(r.maName)
{
}

ScExternalNameToken::~ScExternalNameToken() {}

sal_uInt16 ScExternalNameToken::GetIndex() const
{
    return mnFileId;
}

const String& ScExternalNameToken::GetString() const
{
    return maName;
}

sal_Bool ScExternalNameToken::operator==( const FormulaToken& r ) const
{
    if ( !FormulaToken::operator==(r) )
        return false;

    if (mnFileId != r.GetIndex())
        return false;

    xub_StrLen nLen = maName.Len();
    const String& rName = r.GetString();
    if (nLen != rName.Len())
        return false;

    const sal_Unicode* p1 = maName.GetBuffer();
    const sal_Unicode* p2 = rName.GetBuffer();
    for (xub_StrLen j = 0; j < nLen; ++j)
    {
        if (p1[j] != p2[j])
            return false;
    }
    return true;
}

// ============================================================================

ScJumpMatrix* ScJumpMatrixToken::GetJumpMatrix() const  { return pJumpMatrix; }
sal_Bool ScJumpMatrixToken::operator==( const FormulaToken& r ) const
{
    return FormulaToken::operator==( r ) && pJumpMatrix == static_cast<const ScToken&>(r).GetJumpMatrix();
}
ScJumpMatrixToken::~ScJumpMatrixToken()
{
    delete pJumpMatrix;
}

double          ScEmptyCellToken::GetDouble() const     { return 0.0; }
const String &  ScEmptyCellToken::GetString() const     
{ 
    static  String              aDummyString;
    return aDummyString; 
}
sal_Bool ScEmptyCellToken::operator==( const FormulaToken& r ) const
{
    return FormulaToken::operator==( r ) &&
        bInherited == static_cast< const ScEmptyCellToken & >(r).IsInherited() &&
        bDisplayedAsString == static_cast< const ScEmptyCellToken & >(r).IsDisplayedAsString();
}


double          ScMatrixCellResultToken::GetDouble() const  { return xUpperLeft->GetDouble(); }
const String &  ScMatrixCellResultToken::GetString() const  { return xUpperLeft->GetString(); }
const ScMatrix* ScMatrixCellResultToken::GetMatrix() const  { return xMatrix; }
// Non-const GetMatrix() is private and unused but must be implemented to
// satisfy vtable linkage.
ScMatrix* ScMatrixCellResultToken::GetMatrix()
{
    return const_cast<ScMatrix*>(xMatrix.operator->());
}
sal_Bool ScMatrixCellResultToken::operator==( const FormulaToken& r ) const
{
    return FormulaToken::operator==( r ) &&
        xUpperLeft == static_cast<const ScMatrixCellResultToken &>(r).xUpperLeft &&
        xMatrix == static_cast<const ScMatrixCellResultToken &>(r).xMatrix;
}


sal_Bool ScMatrixFormulaCellToken::operator==( const FormulaToken& r ) const
{
    const ScMatrixFormulaCellToken* p = dynamic_cast<const ScMatrixFormulaCellToken*>(&r);
    return p && ScMatrixCellResultToken::operator==( r ) &&
        nCols == p->nCols && nRows == p->nRows;
}
void ScMatrixFormulaCellToken::Assign( const formula::FormulaToken& r )
{
    if (this == &r)
        return;
    const ScMatrixCellResultToken* p = dynamic_cast<const ScMatrixCellResultToken*>(&r);
    if (p)
        ScMatrixCellResultToken::Assign( *p);
    else
    {
        DBG_ASSERT( r.GetType() != svMatrix, "ScMatrixFormulaCellToken::operator=: assigning ScMatrixToken to ScMatrixFormulaCellToken is not proper, use ScMatrixCellResultToken instead");
        if (r.GetType() == svMatrix)
        {
            xUpperLeft = NULL;
            xMatrix = static_cast<const ScToken&>(r).GetMatrix();
        }
        else
        {
            xUpperLeft = &r;
            xMatrix = NULL;
        }
    }
}
void ScMatrixFormulaCellToken::SetUpperLeftDouble( double f )
{
    switch (GetUpperLeftType())
    {
        case svDouble:
            const_cast<FormulaToken*>(xUpperLeft.get())->GetDoubleAsReference() = f;
            break;
        case svUnknown:
            if (!xUpperLeft)
            {
                xUpperLeft = new FormulaDoubleToken( f);
                break;
            }
            // fall thru
        default:
            {
                DBG_ERRORFILE("ScMatrixFormulaCellToken::SetUpperLeftDouble: not modifying unhandled token type");
            }
    }
}


double          ScHybridCellToken::GetDouble() const    { return fDouble; }
const String &  ScHybridCellToken::GetString() const    { return aString; }
sal_Bool ScHybridCellToken::operator==( const FormulaToken& r ) const
{
    return FormulaToken::operator==( r ) &&
        fDouble == r.GetDouble() && aString == r.GetString() &&
        aFormula == static_cast<const ScHybridCellToken &>(r).GetFormula();
}




//////////////////////////////////////////////////////////////////////////

bool ScTokenArray::AddFormulaToken(const com::sun::star::sheet::FormulaToken& _aToken,formula::ExternalReferenceHelper* _pRef)
{
    bool bError = FormulaTokenArray::AddFormulaToken(_aToken,_pRef);
    if ( bError )
    {
        bError = false;
        const OpCode eOpCode = static_cast<OpCode>(_aToken.OpCode);      //! assuming equal values for the moment

        const uno::TypeClass eClass = _aToken.Data.getValueTypeClass();
        switch ( eClass )
        {
            case uno::TypeClass_STRUCT:
                {
                    uno::Type aType = _aToken.Data.getValueType();
                    if ( aType.equals( cppu::UnoType<sheet::SingleReference>::get() ) )
                    {
                        ScSingleRefData aSingleRef;
                        sheet::SingleReference aApiRef;
                        _aToken.Data >>= aApiRef;
                        lcl_SingleRefToCalc( aSingleRef, aApiRef );
                        if ( eOpCode == ocPush )
                            AddSingleReference( aSingleRef );
                        else if ( eOpCode == ocColRowName )
                            AddColRowName( aSingleRef );
                        else
                            bError = true;
                    }
                    else if ( aType.equals( cppu::UnoType<sheet::ComplexReference>::get() ) )
                    {
                        ScComplexRefData aComplRef;
                        sheet::ComplexReference aApiRef;
                        _aToken.Data >>= aApiRef;
                        lcl_SingleRefToCalc( aComplRef.Ref1, aApiRef.Reference1 );
                        lcl_SingleRefToCalc( aComplRef.Ref2, aApiRef.Reference2 );

                        if ( eOpCode == ocPush )
                            AddDoubleReference( aComplRef );
                        else
                            bError = true;
                    }
                    else if ( aType.equals( cppu::UnoType<sheet::ExternalReference>::get() ) )
                    {
                        sheet::ExternalReference aApiExtRef;
                        if( (eOpCode == ocPush) && (_aToken.Data >>= aApiExtRef) && (0 <= aApiExtRef.Index) && (aApiExtRef.Index <= SAL_MAX_UINT16) )
                        {
                            sal_uInt16 nFileId = static_cast< sal_uInt16 >( aApiExtRef.Index );
                            sheet::SingleReference aApiSRef;
                            sheet::ComplexReference aApiCRef;
                            ::rtl::OUString aName;
                            if( aApiExtRef.Reference >>= aApiSRef )
                            {
                                // try to resolve cache index to sheet name
                                size_t nCacheId = static_cast< size_t >( aApiSRef.Sheet );
                                String aTabName = _pRef->getCacheTableName( nFileId, nCacheId );
                                if( aTabName.Len() > 0 )
                                {
                                    ScSingleRefData aSingleRef;
                                    // convert column/row settings, set sheet index to absolute
                                    lcl_ExternalRefToCalc( aSingleRef, aApiSRef );
                                    AddExternalSingleReference( nFileId, aTabName, aSingleRef );
                                }
                                else
                                    bError = true;
                            }
                            else if( aApiExtRef.Reference >>= aApiCRef )
                            {
                                // try to resolve cache index to sheet name.
                                size_t nCacheId = static_cast< size_t >( aApiCRef.Reference1.Sheet );
                                String aTabName = _pRef->getCacheTableName( nFileId, nCacheId );
                                if( aTabName.Len() > 0 )
                                {
                                    ScComplexRefData aComplRef;
                                    // convert column/row settings, set sheet index to absolute
                                    lcl_ExternalRefToCalc( aComplRef.Ref1, aApiCRef.Reference1 );
                                    lcl_ExternalRefToCalc( aComplRef.Ref2, aApiCRef.Reference2 );
                                    // NOTE: This assumes that cached sheets are in consecutive order!
                                    aComplRef.Ref2.nTab = aComplRef.Ref1.nTab + static_cast<SCsTAB>(aApiCRef.Reference2.Sheet - aApiCRef.Reference1.Sheet);
                                    AddExternalDoubleReference( nFileId, aTabName, aComplRef );
                                }
                                else
                                    bError = true;
                            }
                            else if( aApiExtRef.Reference >>= aName )
                            {
                                if( aName.getLength() > 0 )
                                    AddExternalName( nFileId, aName );
                                else
                                    bError = true;
                            }
                            else
                                bError = true;
                        }
                        else
                            bError = true;
                    }
                    else
                        bError = true;      // unknown struct
                }
                break;
            case uno::TypeClass_SEQUENCE:
                {
                    if ( eOpCode != ocPush )
                        bError = true;      // not an inline array
                    else if (!_aToken.Data.getValueType().equals( getCppuType(
                                    (uno::Sequence< uno::Sequence< uno::Any > > *)0)))
                        bError = true;      // unexpected sequence type
                    else
                    {
                        ScMatrixRef xMat = ScSequenceToMatrix::CreateMixedMatrix( _aToken.Data);
                        if (xMat)
                            AddMatrix( xMat);
                        else
                            bError = true;
                    }
                }
                break;
            default:
                bError = true;
        }
    }
    return bError;
}
sal_Bool ScTokenArray::ImplGetReference( ScRange& rRange, sal_Bool bValidOnly ) const
{
    sal_Bool bIs = sal_False;
    if ( pCode && nLen == 1 )
    {
        const FormulaToken* pToken = pCode[0];
        if ( pToken )
        {
            if ( pToken->GetType() == svSingleRef )
            {
                const ScSingleRefData& rRef = ((const ScSingleRefToken*)pToken)->GetSingleRef();
                rRange.aStart = rRange.aEnd = ScAddress( rRef.nCol, rRef.nRow, rRef.nTab );
                bIs = !bValidOnly || !rRef.IsDeleted();
            }
            else if ( pToken->GetType() == svDoubleRef )
            {
                const ScComplexRefData& rCompl = ((const ScDoubleRefToken*)pToken)->GetDoubleRef();
                const ScSingleRefData& rRef1 = rCompl.Ref1;
                const ScSingleRefData& rRef2 = rCompl.Ref2;
                rRange.aStart = ScAddress( rRef1.nCol, rRef1.nRow, rRef1.nTab );
                rRange.aEnd   = ScAddress( rRef2.nCol, rRef2.nRow, rRef2.nTab );
                bIs = !bValidOnly || (!rRef1.IsDeleted() && !rRef2.IsDeleted());
            }
        }
    }
    return bIs;
}

sal_Bool ScTokenArray::IsReference( ScRange& rRange ) const
{
    return ImplGetReference( rRange, sal_False );
}

sal_Bool ScTokenArray::IsValidReference( ScRange& rRange ) const
{
    return ImplGetReference( rRange, sal_True );
}

////////////////////////////////////////////////////////////////////////////

ScTokenArray::ScTokenArray() 
{
}

ScTokenArray::ScTokenArray( const ScTokenArray& rArr ) : FormulaTokenArray(rArr)
{
}

ScTokenArray::~ScTokenArray()
{
}



ScTokenArray& ScTokenArray::operator=( const ScTokenArray& rArr )
{
    Clear();
    Assign( rArr );
    return *this;
}

ScTokenArray* ScTokenArray::Clone() const
{
    ScTokenArray* p = new ScTokenArray();
    p->nLen = nLen;
    p->nRPN = nRPN;
    p->nRefs = nRefs;
    p->nMode = nMode;
    p->nError = nError;
    p->bHyperLink = bHyperLink;
    FormulaToken** pp;
    if( nLen )
    {
        pp = p->pCode = new FormulaToken*[ nLen ];
        memcpy( pp, pCode, nLen * sizeof( ScToken* ) );
        for( sal_uInt16 i = 0; i < nLen; i++, pp++ )
        {
            *pp = (*pp)->Clone();
            (*pp)->IncRef();
        }
    }
    if( nRPN )
    {
        pp = p->pRPN = new FormulaToken*[ nRPN ];
        memcpy( pp, pRPN, nRPN * sizeof( ScToken* ) );
        for( sal_uInt16 i = 0; i < nRPN; i++, pp++ )
        {
            FormulaToken* t = *pp;
            if( t->GetRef() > 1 )
            {
                FormulaToken** p2 = pCode;
                sal_uInt16 nIdx = 0xFFFF;
                for( sal_uInt16 j = 0; j < nLen; j++, p2++ )
                {
                    if( *p2 == t )
                    {
                        nIdx = j; break;
                    }
                }
                if( nIdx == 0xFFFF )
                    *pp = t->Clone();
                else
                    *pp = p->pCode[ nIdx ];
            }
            else
                *pp = t->Clone();
            (*pp)->IncRef();
        }
    }
    return p;
}

FormulaToken* ScTokenArray::AddRawToken( const ScRawToken& r )
{
    return Add( r.CreateToken() );
}

// Utility function to ensure that there is strict alternation of values and
// seperators.
static bool
checkArraySep( bool & bPrevWasSep, bool bNewVal )
{
    bool bResult = (bPrevWasSep == bNewVal);
    bPrevWasSep = bNewVal;
    return bResult;
}

FormulaToken* ScTokenArray::MergeArray( )
{
    int nCol = -1, nRow = 0;
    int i, nPrevRowSep = -1, nStart = 0;
    bool bPrevWasSep = false; // top of stack is ocArrayClose
    FormulaToken* t;
    bool bNumeric = false;  // numeric value encountered in current element

    // (1) Iterate from the end to the start to find matrix dims
    // and do basic validation.
    for ( i = nLen ; i-- > nStart ; )
    {
        t = pCode[i];
        switch ( t->GetOpCode() )
        {
            case ocPush :
                if( checkArraySep( bPrevWasSep, false ) )
                {
                    return NULL;
                }

                // no references or nested arrays
                if ( t->GetType() != svDouble  && t->GetType() != svString )
                {
                    return NULL;
                }
                bNumeric = (t->GetType() == svDouble);
            break;

            case ocMissing :
            case ocTrue :
            case ocFalse :
                if( checkArraySep( bPrevWasSep, false ) )
                {
                    return NULL;
                }
                bNumeric = false;
            break;

            case ocArrayColSep :
            case ocSep :
                if( checkArraySep( bPrevWasSep, true ) )
                {
                    return NULL;
                }
                bNumeric = false;
            break;

            case ocArrayClose :
                // not possible with the , but check just in case
                // something changes in the future
                if( i != (nLen-1))
                {
                    return NULL;
                }

                if( checkArraySep( bPrevWasSep, true ) )
                {
                    return NULL;
                }

                nPrevRowSep = i;
                bNumeric = false;
            break;

            case ocArrayOpen :
                nStart = i; // stop iteration
                // fall through to ArrayRowSep

            case ocArrayRowSep :
                if( checkArraySep( bPrevWasSep, true ) )
                {
                    return NULL;
                }

                if( nPrevRowSep < 0 ||              // missing ocArrayClose
                    ((nPrevRowSep - i) % 2) == 1)   // no complex elements
                {
                    return NULL;
                }

                if( nCol < 0 )
                {
                    nCol = (nPrevRowSep - i) / 2;
                }
                else if( (nPrevRowSep - i)/2 != nCol)   // irregular array
                {
                    return NULL;
                }

                nPrevRowSep = i;
                nRow++;
                bNumeric = false;
            break;

            case ocNegSub :
            case ocAdd :
                // negation or unary plus must precede numeric value
                if( !bNumeric )
                {
                    return NULL;
                }
                --nPrevRowSep;      // shorten this row by 1
                bNumeric = false;   // one level only, no --42
            break;

            case ocSpaces :
                // ignore spaces
                --nPrevRowSep;      // shorten this row by 1
            break;

            default :
                // no functions or operators
                return NULL;
        }
    }
    if( nCol <= 0 || nRow <= 0 )
        return NULL;

    // fprintf (stderr, "Array (cols = %d, rows = %d)\n", nCol, nRow );

    int nSign = 1;
    ScMatrix* pArray = new ScMatrix( nCol, nRow );
    for ( i = nStart, nCol = 0, nRow = 0 ; i < nLen ; i++ )
    {
        t = pCode[i];

        switch ( t->GetOpCode() )
        {
            case ocPush :
                if ( t->GetType() == svDouble )
                {
                    pArray->PutDouble( t->GetDouble() * nSign, nCol, nRow );
                    nSign = 1;
                }
                else if ( t->GetType() == svString )
                {
                    pArray->PutString( t->GetString(), nCol, nRow );
                }
            break;

            case ocMissing :
                pArray->PutEmpty( nCol, nRow );
            break;

            case ocTrue :
                pArray->PutBoolean( true, nCol, nRow );
            break;

            case ocFalse :
                pArray->PutBoolean( false, nCol, nRow );
            break;

            case ocArrayColSep :
            case ocSep :
                nCol++;
            break;

            case ocArrayRowSep :
                nRow++; nCol = 0;
            break;

            case ocNegSub :
                nSign = -nSign;
            break;

            default :
                break;
        }
        pCode[i] = NULL;
        t->DecRef();
    }
    nLen = sal_uInt16( nStart );
    return AddMatrix( pArray );
}


FormulaToken* ScTokenArray::MergeRangeReference( const ScAddress & rPos )
{
    if (!pCode || !nLen)
        return NULL;
    sal_uInt16 nIdx = nLen;
    FormulaToken *p1, *p2, *p3;      // ref, ocRange, ref
    // The actual types are checked in ExtendRangeReference().
    if (((p3 = PeekPrev(nIdx)) != 0) &&
            (((p2 = PeekPrev(nIdx)) != 0) && p2->GetOpCode() == ocRange) &&
            ((p1 = PeekPrev(nIdx)) != 0))
    {
        FormulaTokenRef p = ScToken::ExtendRangeReference( *p1, *p3, rPos, true);
        if (p)
        {
            p->IncRef();
            p1->DecRef();
            p2->DecRef();
            p3->DecRef();
            nLen -= 2;
            pCode[ nLen-1 ] = p;
            nRefs--;
        }
    }
    return pCode[ nLen-1 ];
}

FormulaToken* ScTokenArray::AddOpCode( OpCode e )
{
    ScRawToken t;
    t.SetOpCode( e );
    return AddRawToken( t );
}

FormulaToken* ScTokenArray::AddSingleReference( const ScSingleRefData& rRef )
{
    return Add( new ScSingleRefToken( rRef ) );
}

FormulaToken* ScTokenArray::AddMatrixSingleReference( const ScSingleRefData& rRef )
{
    return Add( new ScSingleRefToken( rRef, ocMatRef ) );
}

FormulaToken* ScTokenArray::AddDoubleReference( const ScComplexRefData& rRef )
{
    return Add( new ScDoubleRefToken( rRef ) );
}

FormulaToken* ScTokenArray::AddMatrix( ScMatrix* p )
{
    return Add( new ScMatrixToken( p ) );
}

FormulaToken* ScTokenArray::AddExternalName( sal_uInt16 nFileId, const String& rName )
{
    return Add( new ScExternalNameToken(nFileId, rName) );
}

FormulaToken* ScTokenArray::AddExternalSingleReference( sal_uInt16 nFileId, const String& rTabName, const ScSingleRefData& rRef )
{
    return Add( new ScExternalSingleRefToken(nFileId, rTabName, rRef) );
}

FormulaToken* ScTokenArray::AddExternalDoubleReference( sal_uInt16 nFileId, const String& rTabName, const ScComplexRefData& rRef )
{
    return Add( new ScExternalDoubleRefToken(nFileId, rTabName, rRef) );
}

FormulaToken* ScTokenArray::AddColRowName( const ScSingleRefData& rRef )
{
    return Add( new ScSingleRefToken( rRef, ocColRowName ) );
}

sal_Bool ScTokenArray::GetAdjacentExtendOfOuterFuncRefs( SCCOLROW& nExtend,
        const ScAddress& rPos, ScDirection eDir )
{
    SCCOL nCol = 0;
    SCROW nRow = 0;
    switch ( eDir )
    {
        case DIR_BOTTOM :
            if ( rPos.Row() < MAXROW )
                nRow = (nExtend = rPos.Row()) + 1;
            else
                return sal_False;
        break;
        case DIR_RIGHT :
            if ( rPos.Col() < MAXCOL )
                nCol = static_cast<SCCOL>(nExtend = rPos.Col()) + 1;
            else
                return sal_False;
        break;
        case DIR_TOP :
            if ( rPos.Row() > 0 )
                nRow = (nExtend = rPos.Row()) - 1;
            else
                return sal_False;
        break;
        case DIR_LEFT :
            if ( rPos.Col() > 0 )
                nCol = static_cast<SCCOL>(nExtend = rPos.Col()) - 1;
            else
                return sal_False;
        break;
        default:
            DBG_ERRORFILE( "unknown Direction" );
            return sal_False;
    }
    if ( pRPN && nRPN )
    {
        FormulaToken* t = pRPN[nRPN-1];
        if ( t->GetType() == svByte )
        {
            sal_uInt8 nParamCount = t->GetByte();
            if ( nParamCount && nRPN > nParamCount )
            {
                sal_Bool bRet = sal_False;
                sal_uInt16 nParam = nRPN - nParamCount - 1;
                for ( ; nParam < nRPN-1; nParam++ )
                {
                    FormulaToken* p = pRPN[nParam];
                    switch ( p->GetType() )
                    {
                        case svSingleRef :
                        {
                            ScSingleRefData& rRef = static_cast<ScToken*>(p)->GetSingleRef();
                            rRef.CalcAbsIfRel( rPos );
                            switch ( eDir )
                            {
                                case DIR_BOTTOM :
                                    if ( rRef.nRow == nRow
                                            && rRef.nRow > nExtend )
                                    {
                                        nExtend = rRef.nRow;
                                        bRet = sal_True;
                                    }
                                break;
                                case DIR_RIGHT :
                                    if ( rRef.nCol == nCol
                                            && static_cast<SCCOLROW>(rRef.nCol)
                                            > nExtend )
                                    {
                                        nExtend = rRef.nCol;
                                        bRet = sal_True;
                                    }
                                break;
                                case DIR_TOP :
                                    if ( rRef.nRow == nRow
                                            && rRef.nRow < nExtend )
                                    {
                                        nExtend = rRef.nRow;
                                        bRet = sal_True;
                                    }
                                break;
                                case DIR_LEFT :
                                    if ( rRef.nCol == nCol
                                            && static_cast<SCCOLROW>(rRef.nCol)
                                            < nExtend )
                                    {
                                        nExtend = rRef.nCol;
                                        bRet = sal_True;
                                    }
                                break;
                            }
                        }
                        break;
                        case svDoubleRef :
                        {
                            ScComplexRefData& rRef = static_cast<ScToken*>(p)->GetDoubleRef();
                            rRef.CalcAbsIfRel( rPos );
                            switch ( eDir )
                            {
                                case DIR_BOTTOM :
                                    if ( rRef.Ref1.nRow == nRow
                                            && rRef.Ref2.nRow > nExtend )
                                    {
                                        nExtend = rRef.Ref2.nRow;
                                        bRet = sal_True;
                                    }
                                break;
                                case DIR_RIGHT :
                                    if ( rRef.Ref1.nCol == nCol &&
                                            static_cast<SCCOLROW>(rRef.Ref2.nCol)
                                            > nExtend )
                                    {
                                        nExtend = rRef.Ref2.nCol;
                                        bRet = sal_True;
                                    }
                                break;
                                case DIR_TOP :
                                    if ( rRef.Ref2.nRow == nRow
                                            && rRef.Ref1.nRow < nExtend )
                                    {
                                        nExtend = rRef.Ref1.nRow;
                                        bRet = sal_True;
                                    }
                                break;
                                case DIR_LEFT :
                                    if ( rRef.Ref2.nCol == nCol &&
                                            static_cast<SCCOLROW>(rRef.Ref1.nCol)
                                            < nExtend )
                                    {
                                        nExtend = rRef.Ref1.nCol;
                                        bRet = sal_True;
                                    }
                                break;
                            }
                        }
                        break;
                        default:
                        {
                            // added to avoid warnings
                        }
                    } // switch
                } // for
                return bRet;
            }
        }
    }
    return sal_False;
}


void ScTokenArray::ReadjustRelative3DReferences( const ScAddress& rOldPos,
        const ScAddress& rNewPos )
{
    for ( sal_uInt16 j=0; j<nLen; ++j )
    {
        switch ( pCode[j]->GetType() )
        {
            case svDoubleRef :
            {
                ScSingleRefData& rRef2 = static_cast<ScToken*>(pCode[j])->GetSingleRef2();
                // Also adjust if the reference is of the form Sheet1.A2:A3
                if ( rRef2.IsFlag3D() || static_cast<ScToken*>(pCode[j])->GetSingleRef().IsFlag3D() )
                {
                    rRef2.CalcAbsIfRel( rOldPos );
                    rRef2.CalcRelFromAbs( rNewPos );
                }
            }
            //! fallthru
            case svSingleRef :
            {
                ScSingleRefData& rRef1 = static_cast<ScToken*>(pCode[j])->GetSingleRef();
                if ( rRef1.IsFlag3D() )
                {
                    rRef1.CalcAbsIfRel( rOldPos );
                    rRef1.CalcRelFromAbs( rNewPos );
                }
            }
            break;
            case svExternalDoubleRef:
            {
                ScSingleRefData& rRef2 = static_cast<ScToken*>(pCode[j])->GetSingleRef2();
                rRef2.CalcAbsIfRel( rOldPos );
                rRef2.CalcRelFromAbs( rNewPos );
            }
            //! fallthru
            case svExternalSingleRef:
            {
                ScSingleRefData& rRef1 = static_cast<ScToken*>(pCode[j])->GetSingleRef();
                rRef1.CalcAbsIfRel( rOldPos );
                rRef1.CalcRelFromAbs( rNewPos );
            }
            break;
            default:
            {
                // added to avoid warnings
            }
        }
    }
}


