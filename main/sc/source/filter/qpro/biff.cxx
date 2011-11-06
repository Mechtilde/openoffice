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

#include <sal/config.h>
#include <stdio.h>
#include <sfx2/docfile.hxx>

#include "global.hxx"
#include "scerrors.hxx"
#include "docpool.hxx"
#include "patattr.hxx"
#include "filter.hxx"
#include "document.hxx"
#include "cell.hxx"
#include "biff.hxx"

ScBiffReader::ScBiffReader( SfxMedium & rMedium ) :
	mnId(0),
	mnLength(0),
	mnOffset(0)
{
	mpStream = rMedium.GetInStream();
	if( mpStream )
	{
        mpStream->SetBufferSize( 65535 );
		mpStream->SetStreamCharSet( RTL_TEXTENCODING_MS_1252 );
	}
}

ScBiffReader::~ScBiffReader()
{
	if( mpStream )
		mpStream->SetBufferSize( 0 );
}

bool ScBiffReader::nextRecord()
{
	if( !recordsLeft() )
		return false;

	if( IsEndOfFile() )
		return false;

	sal_uInt32 nPos = mpStream->Tell();
	if( nPos != mnOffset + mnLength )
		mpStream->Seek( mnOffset + mnLength );

	mnLength = mnId = 0;
	*mpStream >> mnId >> mnLength;

	mnOffset = mpStream->Tell();
#ifdef DEBUG
	fprintf( stderr, "Read record 0x%x length 0x%x at offset 0x%x\n",
        (unsigned)mnId, (unsigned)mnLength, (unsigned)mnOffset );

#if 1  // rather verbose
	int len = mnLength;
	while (len > 0) {
		int i, chunk = len < 16 ? len : 16;
		unsigned char data[16];
		mpStream->Read( data, chunk );

		for (i = 0; i < chunk; i++)
			fprintf( stderr, "%.2x ", data[i] );
		fprintf( stderr, "| " );
		for (i = 0; i < chunk; i++)
			fprintf( stderr, "%c", data[i] < 127 && data[i] > 30 ? data[i] : '.' );
		fprintf( stderr, "\n" );

		len -= chunk;
	}
	mpStream->Seek( mnOffset );
#endif
#endif
	return true;
}

