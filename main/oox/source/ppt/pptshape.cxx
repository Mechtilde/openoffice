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



#include "oox/ppt/pptshape.hxx"
#include "oox/core/xmlfilterbase.hxx"
#include "oox/drawingml/textbody.hxx"

#include <com/sun/star/container/XNamed.hpp>
#include <com/sun/star/beans/XMultiPropertySet.hpp>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>
#include <com/sun/star/drawing/HomogenMatrix3.hpp>
#include <com/sun/star/text/XText.hpp>
#include <basegfx/matrix/b2dhommatrix.hxx>
#include "oox/ppt/slidepersist.hxx"

using rtl::OUString;
using namespace ::oox::core;
using namespace ::oox::drawingml;
using namespace ::com::sun::star;
using namespace ::com::sun::star::awt;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::frame;
using namespace ::com::sun::star::text;
using namespace ::com::sun::star::drawing;

namespace oox { namespace ppt {

PPTShape::PPTShape( const oox::ppt::ShapeLocation eShapeLocation, const sal_Char* pServiceName )
: Shape( pServiceName )
, meShapeLocation( eShapeLocation )
, mbReferenced( sal_False )
{
}

PPTShape::~PPTShape()
{
}

void PPTShape::addShape(
        oox::core::XmlFilterBase& rFilterBase,
        const SlidePersist& rSlidePersist,
        const oox::drawingml::Theme* pTheme,
        const Reference< XShapes >& rxShapes,
        const awt::Rectangle* pShapeRect,
        ::oox::drawingml::ShapeIdMap* pShapeMap )
{
	// only placeholder from layout are being inserted
	if ( mnSubType && ( meShapeLocation == Master ) )
		return;
	try
    {
		rtl::OUString sServiceName( msServiceName );
		if( sServiceName.getLength() )
		{
			oox::drawingml::TextListStylePtr aMasterTextListStyle;
            Reference< lang::XMultiServiceFactory > xServiceFact( rFilterBase.getModel(), UNO_QUERY_THROW );
			sal_Bool bClearText = sal_False;

			if ( sServiceName != OUString::createFromAscii( "com.sun.star.drawing.GraphicObjectShape" ) )
			{
				switch( mnSubType )
				{
					case XML_ctrTitle :
					case XML_title :
					{
						const rtl::OUString sTitleShapeService( RTL_CONSTASCII_USTRINGPARAM( "com.sun.star.presentation.TitleTextShape" ) );
						sServiceName = sTitleShapeService;
						aMasterTextListStyle = rSlidePersist.getMasterPersist().get() ? rSlidePersist.getMasterPersist()->getTitleTextStyle() : rSlidePersist.getTitleTextStyle();
					}
					break;
					case XML_subTitle :
					{
						if ( ( meShapeLocation == Master ) || ( meShapeLocation == Layout ) )
							sServiceName = rtl::OUString();
						else {
							const rtl::OUString sTitleShapeService( RTL_CONSTASCII_USTRINGPARAM( "com.sun.star.presentation.SubtitleShape" ) );
							sServiceName = sTitleShapeService;
							aMasterTextListStyle = rSlidePersist.getMasterPersist().get() ? rSlidePersist.getMasterPersist()->getTitleTextStyle() : rSlidePersist.getTitleTextStyle();
						}
					}
					break;
   				    case XML_obj :
					{
						const rtl::OUString sOutlinerShapeService( RTL_CONSTASCII_USTRINGPARAM( "com.sun.star.presentation.OutlinerShape" ) );
						sServiceName = sOutlinerShapeService;
						aMasterTextListStyle = rSlidePersist.getMasterPersist().get() ? rSlidePersist.getMasterPersist()->getBodyTextStyle() : rSlidePersist.getBodyTextStyle();
					}
					break;
					case XML_body :
					{
						const rtl::OUString sNotesShapeService( RTL_CONSTASCII_USTRINGPARAM( "com.sun.star.presentation.NotesShape" ) );
						const rtl::OUString sOutlinerShapeService( RTL_CONSTASCII_USTRINGPARAM( "com.sun.star.presentation.OutlinerShape" ) );
						if ( rSlidePersist.isNotesPage() )
						{
							sServiceName = sNotesShapeService;
							aMasterTextListStyle = rSlidePersist.getMasterPersist().get() ? rSlidePersist.getMasterPersist()->getNotesTextStyle() : rSlidePersist.getNotesTextStyle();
						}
						else
						{
							sServiceName = sOutlinerShapeService;
							aMasterTextListStyle = rSlidePersist.getMasterPersist().get() ? rSlidePersist.getMasterPersist()->getBodyTextStyle() : rSlidePersist.getBodyTextStyle();
						}
					}
					break;
					case XML_dt :
					{
						const rtl::OUString sDateTimeShapeService( RTL_CONSTASCII_USTRINGPARAM( "com.sun.star.presentation.DateTimeShape" ) );
						sServiceName = sDateTimeShapeService;
						bClearText = sal_True;
					}
					break;
					case XML_hdr :
					{
						const rtl::OUString sHeaderShapeService( RTL_CONSTASCII_USTRINGPARAM( "com.sun.star.presentation.HeaderShape" ) );
						sServiceName = sHeaderShapeService;
						bClearText = sal_True;
					}
					break;
					case XML_ftr :
					{
						const rtl::OUString sFooterShapeService( RTL_CONSTASCII_USTRINGPARAM( "com.sun.star.presentation.FooterShape" ) );
						sServiceName = sFooterShapeService;
						bClearText = sal_True;
					}
					break;
					case XML_sldNum :
					{
						const rtl::OUString sSlideNumberShapeService( RTL_CONSTASCII_USTRINGPARAM( "com.sun.star.presentation.SlideNumberShape" ) );
						sServiceName = sSlideNumberShapeService;
						bClearText = sal_True;
					}
					break;
					case XML_sldImg :
					{
						const rtl::OUString sPageShapeService( RTL_CONSTASCII_USTRINGPARAM( "com.sun.star.presentation.PageShape" ) );
						sServiceName = sPageShapeService;
					}
					break;

					default:
					break;
				}
			}

/*
			// use placeholder index if possible
			if( mnSubType && getSubTypeIndex() && rSlidePersist.getMasterPersist().get() ) {
			    oox::drawingml::ShapePtr pPlaceholder = PPTShape::findPlaceholderByIndex( getSubTypeIndex(), rSlidePersist.getMasterPersist()->getShapes()->getChildren() );
			    if( pPlaceholder.get() && pPlaceholder->getTextBody() ) {
				TextListStylePtr pNewTextListStyle ( new TextListStyle() );

				pNewTextListStyle->apply( pPlaceholder->getTextBody()->getTextListStyle() );
				if( pPlaceholder->getMasterTextListStyle().get() )
				    pNewTextListStyle->apply( *pPlaceholder->getMasterTextListStyle() );
				
				aMasterTextListStyle = pNewTextListStyle;
			    }
			}
*/
			if ( sServiceName.getLength() )
			{
				if ( !aMasterTextListStyle.get() )
					aMasterTextListStyle = rSlidePersist.getMasterPersist().get() ? rSlidePersist.getMasterPersist()->getOtherTextStyle() : rSlidePersist.getOtherTextStyle();
				setMasterTextListStyle( aMasterTextListStyle );
 
				Reference< XShape > xShape( createAndInsert( rFilterBase, sServiceName, pTheme, rxShapes, pShapeRect, bClearText ) );
				if ( !rSlidePersist.isMasterPage() && rSlidePersist.getPage().is() && ( (sal_Int32)mnSubType == XML_title ) )
 				{
					try
					{
						rtl::OUString aTitleText;
						Reference< XTextRange > xText( xShape, UNO_QUERY_THROW );
						aTitleText = xText->getString();
						if ( aTitleText.getLength() && ( aTitleText.getLength() < 64 ) )	// just a magic value, but we don't want to set slide names which are too long
						{
							Reference< container::XNamed > xName( rSlidePersist.getPage(), UNO_QUERY_THROW );
							xName->setName( aTitleText );
						}
					}
					catch( uno::Exception& )
					{
                        
					}
				}
				if( pShapeMap && msId.getLength() )
				{
					(*pShapeMap)[ msId ] = shared_from_this();
				}

				// if this is a group shape, we have to add also each child shape
				Reference< XShapes > xShapes( xShape, UNO_QUERY );
				if ( xShapes.is() )
					addChildren( rFilterBase, *this, pTheme, xShapes, pShapeRect ? *pShapeRect : awt::Rectangle( maPosition.X, maPosition.Y, maSize.Width, maSize.Height ), pShapeMap );
            }
		}
	}
	catch( const Exception&  )
	{
	}
}

void PPTShape::applyShapeReference( const oox::drawingml::Shape& rReferencedShape )
{
	Shape::applyShapeReference( rReferencedShape );
}

oox::drawingml::ShapePtr PPTShape::findPlaceholder( const sal_Int32 nMasterPlaceholder, std::vector< oox::drawingml::ShapePtr >& rShapes )
{
	oox::drawingml::ShapePtr aShapePtr;
	std::vector< oox::drawingml::ShapePtr >::reverse_iterator aRevIter( rShapes.rbegin() );
	while( aRevIter != rShapes.rend() )
	{
		if ( (*aRevIter)->getSubType() == nMasterPlaceholder )
		{
			aShapePtr = *aRevIter;
			break;
		}
        std::vector< oox::drawingml::ShapePtr >& rChildren = (*aRevIter)->getChildren();
        aShapePtr = findPlaceholder( nMasterPlaceholder, rChildren );
		if ( aShapePtr.get() )
			break;
		aRevIter++;
	}
	return aShapePtr;
}

oox::drawingml::ShapePtr PPTShape::findPlaceholderByIndex( const sal_Int32 nIdx, std::vector< oox::drawingml::ShapePtr >& rShapes )
{
	oox::drawingml::ShapePtr aShapePtr;
	std::vector< oox::drawingml::ShapePtr >::reverse_iterator aRevIter( rShapes.rbegin() );
	while( aRevIter != rShapes.rend() )
	{
		if ( (*aRevIter)->getSubTypeIndex() == nIdx )
		{
			aShapePtr = *aRevIter;
			break;
		}
		std::vector< oox::drawingml::ShapePtr >& rChildren = (*aRevIter)->getChildren();
		aShapePtr = findPlaceholderByIndex( nIdx, rChildren );
		if ( aShapePtr.get() )
		    break;
		aRevIter++;
	}
	return aShapePtr;
}

// if nFirstPlaceholder can't be found, it will be searched for nSecondPlaceholder
oox::drawingml::ShapePtr PPTShape::findPlaceholder( sal_Int32 nFirstPlaceholder, sal_Int32 nSecondPlaceholder, std::vector< oox::drawingml::ShapePtr >& rShapes )
{
	oox::drawingml::ShapePtr pPlaceholder = findPlaceholder( nFirstPlaceholder, rShapes );
	return !nSecondPlaceholder || pPlaceholder.get() ? pPlaceholder : findPlaceholder( nSecondPlaceholder, rShapes );
}

} }
