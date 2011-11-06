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

#include "xmlconti.hxx"
#include "xmlimprt.hxx"
#include "global.hxx"
#include "document.hxx"

#include <xmloff/xmltkmap.hxx>
#include <xmloff/nmspmap.hxx>
#include <xmloff/xmlnmspe.hxx>
#include <xmloff/xmltoken.hxx>

using namespace xmloff::token;

//------------------------------------------------------------------

ScXMLContentContext::ScXMLContentContext( ScXMLImport& rImport,
									  sal_uInt16 nPrfx,
									  const ::rtl::OUString& rLName,
									  const ::com::sun::star::uno::Reference<
                                      ::com::sun::star::xml::sax::XAttributeList>& /* xAttrList */,
									  rtl::OUStringBuffer& sTempValue) :
	SvXMLImportContext( rImport, nPrfx, rLName ),
	sOUText(),
	sValue(sTempValue)
{
}

ScXMLContentContext::~ScXMLContentContext()
{
}

SvXMLImportContext *ScXMLContentContext::CreateChildContext( sal_uInt16 nPrefix,
											const ::rtl::OUString& rLName,
											const ::com::sun::star::uno::Reference<
									  	::com::sun::star::xml::sax::XAttributeList>& xAttrList )
{
	SvXMLImportContext *pContext = 0;

	if ((nPrefix == XML_NAMESPACE_TEXT) && IsXMLToken(rLName, XML_S))
	{
		sal_Int32 nRepeat(0);
		sal_Int16 nAttrCount = xAttrList.is() ? xAttrList->getLength() : 0;
		for( sal_Int16 i=0; i < nAttrCount; ++i )
		{
			const rtl::OUString& sAttrName(xAttrList->getNameByIndex( i ));
			const rtl::OUString& sAttrValue(xAttrList->getValueByIndex( i ));
			rtl::OUString aLocalName;
			sal_uInt16 nPrfx = GetScImport().GetNamespaceMap().GetKeyByAttrName(
												sAttrName, &aLocalName );
			if ((nPrfx == XML_NAMESPACE_TEXT) && IsXMLToken(aLocalName, XML_C))
				nRepeat = sAttrValue.toInt32();
		}
		if (nRepeat)
			for (sal_Int32 j = 0; j < nRepeat; ++j)
				sOUText.append(static_cast<sal_Unicode>(' '));
		else
			sOUText.append(static_cast<sal_Unicode>(' '));
	}

	if( !pContext )
		pContext = new SvXMLImportContext( GetImport(), nPrefix, rLName );

	return pContext;
}

void ScXMLContentContext::Characters( const ::rtl::OUString& rChars )
{
	sOUText.append(rChars);
}

void ScXMLContentContext::EndElement()
{
	sValue.append(sOUText);
}
