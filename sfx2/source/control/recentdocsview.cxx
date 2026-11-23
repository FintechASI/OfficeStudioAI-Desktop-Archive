/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <sal/log.hxx>
#include <basegfx/polygon/b2dpolygon.hxx>
#include <basegfx/polygon/b2dpolygontools.hxx>
#include <cmath>
#include <comphelper/DirectoryHelper.hxx>
#include <recentdocsview.hxx>
#include <sfx2/sfxresid.hxx>
#include <unotools/historyoptions.hxx>
#include <vcl/event.hxx>
#include <vcl/ptrstyle.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/rendercontext/DrawTextFlags.hxx>
#include <vcl/svapp.hxx>
#include <tools/urlobj.hxx>
#include <com/sun/star/frame/XDispatch.hpp>
#include <sfx2/strings.hrc>
#include <bitmaps.hlst>
#include "recentdocsviewitem.hxx"
#include <sfx2/app.hxx>

#include <officecfg/Office/Common.hxx>
#include <svtools/confirmationdlg.hxx>

#include <map>

using namespace ::com::sun::star;
using namespace com::sun::star::uno;

namespace {

/// Set (larger) font for the Welcome message.
void SetMessageFont(vcl::RenderContext& rRenderContext)
{
    vcl::Font aFont(rRenderContext.GetFont());
    aFont.SetFontHeight(aFont.GetFontHeight() * 1.3);
    rRenderContext.SetFont(aFont);
}

}

namespace sfx2
{

constexpr tools::Long gnTextHeight = 30;
constexpr tools::Long gnItemPadding = 5;

RecentDocsView::RecentDocsView(std::unique_ptr<weld::ScrolledWindow> xWindow)
    : ThumbnailView(std::move(xWindow))
    , mnFileTypes(ApplicationType::TYPE_NONE)
    , mnLastMouseDownItem(THUMBNAILVIEW_ITEM_NOTFOUND)
    , maWelcomeLine1(SfxResId(STR_WELCOME_LINE1))
    , maWelcomeLine2(SfxResId(STR_WELCOME_LINE2))
    , mpLoadRecentFile(nullptr)
    , m_nExecuteHdlId(nullptr)
    , mbHasAnyHistory(false)
{
    mbAllowMultiSelection = false;
    AbsoluteScreenPixelRectangle aScreen = Application::GetScreenPosSizePixel(Application::GetDisplayBuiltInScreen());
    mnItemMaxSize = std::min(aScreen.GetWidth(),aScreen.GetHeight()) > 800 ? 256 : 192;

    setItemMaxTextLength( 30 );
    setItemDimensions( mnItemMaxSize, mnItemMaxSize, gnTextHeight, gnItemPadding );

    mfHighlightTransparence = 0.25;

    UpdateColors(Application::GetSettings().GetStyleSettings());
}

RecentDocsView::~RecentDocsView()
{
    Application::RemoveUserEvent(m_nExecuteHdlId);
    m_nExecuteHdlId = nullptr;
    if (mpLoadRecentFile)
    {
        mpLoadRecentFile->pView = nullptr;
        mpLoadRecentFile = nullptr;
    }
}

void RecentDocsView::UpdateColors(const StyleSettings& rSettings)
{
    ThumbnailView::UpdateColors(rSettings);

    maFillColor = Color(0xFF, 0xFF, 0xFF);
    maTextColor = Color(0x0F, 0x1F, 0x3D);

    maHighlightColor = Color(0xDB, 0xEA, 0xFE);
    maHighlightTextColor = Color(0x0F, 0x1F, 0x3D);
}

bool RecentDocsView::typeMatchesExtension(ApplicationType type, std::u16string_view rExt)
{
    bool bRet = false;

    if (rExt == u"odt" || rExt == u"fodt" || rExt == u"doc" || rExt == u"docx" ||
        rExt == u"rtf" || rExt == u"txt" || rExt == u"odm" || rExt == u"otm")
    {
        bRet = static_cast<bool>(type & ApplicationType::TYPE_WRITER);
    }
    else if (rExt == u"ods" || rExt == u"fods" || rExt == u"xls" || rExt == u"xlsx")
    {
        bRet = static_cast<bool>(type & ApplicationType::TYPE_CALC);
    }
    else if (rExt == u"odp" || rExt == u"fodp" || rExt == u"pps" || rExt == u"ppt" ||
            rExt == u"pptx")
    {
        bRet = static_cast<bool>(type & ApplicationType::TYPE_IMPRESS);
    }
    else if (rExt == u"odg" || rExt == u"fodg")
    {
        bRet = static_cast<bool>(type & ApplicationType::TYPE_DRAW);
    }
    else if (rExt == u"odb")
    {
        bRet = static_cast<bool>(type & ApplicationType::TYPE_DATABASE);
    }
    else if (rExt == u"odf")
    {
        bRet = static_cast<bool>(type & ApplicationType::TYPE_MATH);
    }
    else
    {
        bRet = static_cast<bool>(type & ApplicationType::TYPE_OTHER);
    }

    return bRet;
}

bool RecentDocsView::isAcceptedFile(const INetURLObject& rURL) const
{
    const OUString aExt = rURL.getExtension();
    return (mnFileTypes & ApplicationType::TYPE_WRITER   && typeMatchesExtension(ApplicationType::TYPE_WRITER,  aExt)) ||
           (mnFileTypes & ApplicationType::TYPE_CALC     && typeMatchesExtension(ApplicationType::TYPE_CALC,    aExt)) ||
           (mnFileTypes & ApplicationType::TYPE_IMPRESS  && typeMatchesExtension(ApplicationType::TYPE_IMPRESS, aExt)) ||
           (mnFileTypes & ApplicationType::TYPE_DRAW     && typeMatchesExtension(ApplicationType::TYPE_DRAW,    aExt)) ||
           (mnFileTypes & ApplicationType::TYPE_DATABASE && typeMatchesExtension(ApplicationType::TYPE_DATABASE,aExt)) ||
           (mnFileTypes & ApplicationType::TYPE_MATH     && typeMatchesExtension(ApplicationType::TYPE_MATH,    aExt)) ||
           (mnFileTypes & ApplicationType::TYPE_OTHER    && typeMatchesExtension(ApplicationType::TYPE_OTHER,   aExt));
}

void RecentDocsView::insertItem(const OUString& rURL, const OUString& rTitle,
                                const OUString& rThumbnail, bool isReadOnly, bool isPinned,
                                sal_uInt16 nId)
{
    AppendItem(std::make_unique<RecentDocsViewItem>(*this, rURL, rTitle, rThumbnail, nId,
                                                    mnItemMaxSize, isReadOnly, isPinned));
}

void RecentDocsView::Reload()
{
    Clear();

    std::vector< SvtHistoryOptions::HistoryItem > aHistoryList = SvtHistoryOptions::GetList( EHistoryType::PickList );
    mbHasAnyHistory = !aHistoryList.empty();
    for ( size_t i = 0; i < aHistoryList.size(); i++ )
    {
        const SvtHistoryOptions::HistoryItem& rRecentEntry = aHistoryList[i];

        OUString aURL = rRecentEntry.sURL;
        const INetURLObject aURLObj(aURL);

        if ((mnFileTypes != ApplicationType::TYPE_NONE) &&
            (!isAcceptedFile(aURLObj)))
            continue;

        //Remove extension from url's last segment and use it as title
        const OUString aTitle = aURLObj.GetBase(); //DecodeMechanism::WithCharset

        insertItem(aURL, aTitle, rRecentEntry.sThumbnail, rRecentEntry.isReadOnly,
                   rRecentEntry.isPinned, i + 1);
    }

    CalculateItemPositions();
    Invalidate();
}

void RecentDocsView::setFilter(ApplicationType aFilter)
{
    mnFileTypes = aFilter;
    Reload();
}

void RecentDocsView::clearUnavailableFiles()
{
    if (ConfirmationDlg::Query<officecfg::Office::Common::Misc::QueryClearUnavailableDocuments>(
            Application::GetDefDialogParent(), SfxResId(STR_QUERY_CLR_UNAVAILABLE_DOCS_TITLE),
            SfxResId(STR_QUERY_CLR_UNAVAILABLE_DOCS_TEXT),
            SfxResId(STR_QUERY_CLR_UNAVAILABLE_DOCS_QUESTION)))
    {
        std::vector< SvtHistoryOptions::HistoryItem > aHistoryList = SvtHistoryOptions::GetList( EHistoryType::PickList );
        for ( size_t i = 0; i < aHistoryList.size(); i++ )
        {
            const SvtHistoryOptions::HistoryItem& rPickListEntry = aHistoryList[i];
            if ( !comphelper::DirectoryHelper::fileExists(rPickListEntry.sURL) ){
                SvtHistoryOptions::DeleteItem(EHistoryType::PickList,rPickListEntry.sURL, false);
            }
        }
        Reload();
    }
}

bool RecentDocsView::MouseButtonDown( const MouseEvent& rMEvt )
{
    if (rMEvt.IsLeft())
    {
        mnLastMouseDownItem = ImplGetItem(rMEvt.GetPosPixel());

        // ignore to avoid stuff done in ThumbnailView; we don't do selections etc.
        return true;
    }

    return ThumbnailView::MouseButtonDown(rMEvt);
}

bool RecentDocsView::MouseButtonUp(const MouseEvent& rMEvt)
{
    if (rMEvt.IsLeft())
    {
        if( rMEvt.GetClicks() > 1 )
            return true;

        size_t nPos = ImplGetItem(rMEvt.GetPosPixel());
        ThumbnailViewItem* pItem = ImplGetItem(nPos);

        if (pItem && nPos == mnLastMouseDownItem)
        {
            pItem->MouseButtonUp(rMEvt);

            ThumbnailViewItem* pNewItem = ImplGetItem(nPos);
            if(pNewItem)
                pNewItem->setHighlight(true);
        }

        mnLastMouseDownItem = THUMBNAILVIEW_ITEM_NOTFOUND;

        if (pItem)
            return true;
    }
    return ThumbnailView::MouseButtonUp(rMEvt);
}

void RecentDocsView::OnItemDblClicked(ThumbnailViewItem *pItem)
{
    RecentDocsViewItem* pRecentItem = dynamic_cast< RecentDocsViewItem* >(pItem);
    if (pRecentItem)
        pRecentItem->OpenDocument();
}

void RecentDocsView::Paint(vcl::RenderContext& rRenderContext, const tools::Rectangle &aRect)
{
    ThumbnailView::Paint(rRenderContext, aRect);

    if (!mItemList.empty())
        return;

    auto popIt = rRenderContext.ScopedPush(vcl::PushFlags::ALL);
    rRenderContext.SetAntialiasing(AntialiasingFlags::Enable);

    const Size& rSize = GetOutputSizePixel();
    const double fS = std::max(1.0, static_cast<double>(rRenderContext.GetDPIScaleFactor()));
    auto sx = [fS](tools::Long nPx) -> tools::Long {
        return static_cast<tools::Long>(std::lround(static_cast<double>(nPx) * fS));
    };

    const OUString sHeading = mbHasAnyHistory
        ? u"No matching recent files"_ustr
        : u"No recent files yet"_ustr;

    OUString sBody;
    if (!mbHasAnyHistory)
    {
        sBody = u"Documents you create or open will appear here for quick access."_ustr;
    }
    else if (mnFileTypes == ApplicationType::TYPE_WRITER)
    {
        sBody = u"No Docs files appear in your recent history. Select All Documents in Filter to view everything."_ustr;
    }
    else if (mnFileTypes == ApplicationType::TYPE_CALC)
    {
        sBody = u"No Spreadsheets files appear in your recent history. Select All Documents in Filter to view everything."_ustr;
    }
    else if (mnFileTypes == ApplicationType::TYPE_IMPRESS)
    {
        sBody = u"No SlideShows files appear in your recent history. Select All Documents in Filter to view everything."_ustr;
    }
    else if (mnFileTypes == ApplicationType::TYPE_DRAW)
    {
        sBody = u"No Design files appear in your recent history. Select All Documents in Filter to view everything."_ustr;
    }
    else if (mnFileTypes == ApplicationType::TYPE_DATABASE)
    {
        sBody = u"No Database files appear in your recent history. Select All Documents in Filter to view everything."_ustr;
    }
    else if (mnFileTypes == ApplicationType::TYPE_MATH)
    {
        sBody = u"No Math files appear in your recent history. Select All Documents in Filter to view everything."_ustr;
    }
    else
    {
        sBody = u"No recent files match the active filter. Select All Documents in Filter to view everything."_ustr;
    }

    const tools::Long nCardW = std::max<tools::Long>(180, std::min<tools::Long>(sx(440), rSize.Width() - sx(48)));
    const tools::Long nCardH = std::max<tools::Long>(120, std::min<tools::Long>(sx(214), rSize.Height() - sx(32)));
    const tools::Long nCardX = (rSize.Width() - nCardW) / 2;
    const tools::Long nCardY = (rSize.Height() - nCardH) / 2;

    if (rSize.Width() >= sx(240) && rSize.Height() >= sx(150))
    {
        const double w = static_cast<double>(nCardW);
        const double h = static_cast<double>(nCardH);
        const double rad = 10.0 * fS;
        const basegfx::B2DPolygon aCardPoly = basegfx::utils::createPolygonFromRect(
            basegfx::B2DRectangle(nCardX, nCardY, nCardX + nCardW, nCardY + nCardH),
            std::min(1.0, (2.0 * rad) / std::max(1.0, w)),
            std::min(1.0, (2.0 * rad) / std::max(1.0, h)));
        rRenderContext.SetLineColor();
        rRenderContext.SetFillColor(Color(0xF8, 0xFA, 0xFC));
        rRenderContext.DrawPolygon(aCardPoly);
        rRenderContext.SetFillColor();
        rRenderContext.SetLineColor(Color(0xDC, 0xE3, 0xED));
        rRenderContext.DrawPolyLine(aCardPoly, 1.2 * fS, basegfx::B2DLineJoin::Round, css::drawing::LineCap_ROUND);
    }

    const double fCx = static_cast<double>(rSize.Width()) * 0.5;
    const double fBadgeCy = static_cast<double>(nCardY + sx(52));
    const double fBadgeR = 24.0 * fS;
    const basegfx::B2DPolygon aBadge = basegfx::utils::createPolygonFromCircle(basegfx::B2DPoint(fCx, fBadgeCy), fBadgeR);
    rRenderContext.SetLineColor();
    rRenderContext.SetFillColor(Color(0xED, 0xF1, 0xF6));
    rRenderContext.DrawPolygon(aBadge);

    const Color aIconCol = mbHasAnyHistory ? Color(0x1A, 0x63, 0xD8) : Color(0x10, 0x23, 0x42);
    rRenderContext.SetFillColor();
    rRenderContext.SetLineColor(aIconCol);
    if (!mbHasAnyHistory)
    {
        const basegfx::B2DPolygon aRing = basegfx::utils::createPolygonFromCircle(basegfx::B2DPoint(fCx, fBadgeCy), 10.5 * fS);
        rRenderContext.DrawPolyLine(aRing, 1.8 * fS, basegfx::B2DLineJoin::Round);
        basegfx::B2DPolygon aHands;
        aHands.append(basegfx::B2DPoint(fCx, fBadgeCy - 5.5 * fS));
        aHands.append(basegfx::B2DPoint(fCx, fBadgeCy));
        aHands.append(basegfx::B2DPoint(fCx + 4.5 * fS, fBadgeCy + 2.5 * fS));
        rRenderContext.DrawPolyLine(aHands, 1.8 * fS, basegfx::B2DLineJoin::Round, css::drawing::LineCap_ROUND);
    }
    else
    {
        basegfx::B2DPolygon aFunnel;
        aFunnel.append(basegfx::B2DPoint(fCx - 9.0 * fS, fBadgeCy - 6.5 * fS));
        aFunnel.append(basegfx::B2DPoint(fCx + 9.0 * fS, fBadgeCy - 6.5 * fS));
        aFunnel.append(basegfx::B2DPoint(fCx + 2.5 * fS, fBadgeCy + 1.0 * fS));
        aFunnel.append(basegfx::B2DPoint(fCx + 2.5 * fS, fBadgeCy + 8.0 * fS));
        aFunnel.append(basegfx::B2DPoint(fCx - 2.5 * fS, fBadgeCy + 6.0 * fS));
        aFunnel.append(basegfx::B2DPoint(fCx - 2.5 * fS, fBadgeCy + 1.0 * fS));
        aFunnel.setClosed(true);
        rRenderContext.DrawPolyLine(aFunnel, 1.7 * fS, basegfx::B2DLineJoin::Round, css::drawing::LineCap_ROUND);
    }

    vcl::Font aFont(Application::GetSettings().GetStyleSettings().GetLabelFont());
    aFont.SetFamilyName(u"Segoe UI"_ustr);
    aFont.SetTransparent(true);

    aFont.SetFontHeight(sx(16));
    aFont.SetWeight(WEIGHT_BOLD);
    rRenderContext.SetFont(aFont);
    rRenderContext.SetTextColor(Color(0x0F, 0x1F, 0x3D));
    rRenderContext.DrawText(tools::Rectangle(Point(nCardX + sx(24), nCardY + sx(88)), Size(std::max<tools::Long>(60, nCardW - sx(48)), sx(30))),
                            sHeading, DrawTextFlags::Center | DrawTextFlags::VCenter | DrawTextFlags::EndEllipsis);

    aFont.SetFontHeight(sx(13));
    aFont.SetWeight(WEIGHT_NORMAL);
    rRenderContext.SetFont(aFont);
    rRenderContext.SetTextColor(Color(0x55, 0x65, 0x7D));
    rRenderContext.DrawText(tools::Rectangle(Point(nCardX + sx(28), nCardY + sx(122)), Size(std::max<tools::Long>(60, nCardW - sx(56)), std::max<tools::Long>(24, nCardH - sx(138)))),
                            sBody, DrawTextFlags::MultiLine | DrawTextFlags::WordBreak | DrawTextFlags::Center);
}

void RecentDocsView::LoseFocus()
{
    deselectItems();

    ThumbnailView::LoseFocus();
}

void RecentDocsView::Clear()
{
    Invalidate();
    ThumbnailView::Clear();
}

void RecentDocsView::PostLoadRecentUsedFile(LoadRecentFile* pLoadRecentFile)
{
    assert(!mpLoadRecentFile);
    mpLoadRecentFile = pLoadRecentFile;
    m_nExecuteHdlId = Application::PostUserEvent(LINK(this, RecentDocsView, ExecuteHdl_Impl), pLoadRecentFile);
}

void RecentDocsView::DispatchedLoadRecentUsedFile()
{
    mpLoadRecentFile = nullptr;
}

IMPL_LINK( RecentDocsView, ExecuteHdl_Impl, void*, p, void )
{
    m_nExecuteHdlId = nullptr;
    LoadRecentFile* pLoadRecentFile = static_cast<LoadRecentFile*>(p);
    try
    {
        // Asynchronous execution as this can lead to our own destruction!
        // Framework can recycle our current frame and the layout manager disposes all user interface
        // elements if a component gets detached from its frame!
        pLoadRecentFile->xDispatch->dispatch( pLoadRecentFile->aTargetURL, pLoadRecentFile->aArgSeq );
    }
    catch ( const Exception& )
    {
    }

    if (pLoadRecentFile->pView)
    {
        pLoadRecentFile->pView->DispatchedLoadRecentUsedFile();
        pLoadRecentFile->pView->SetPointer(PointerStyle::Arrow);
        pLoadRecentFile->pView->Enable();
    }

    delete pLoadRecentFile;
}

} // namespace sfx2

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
