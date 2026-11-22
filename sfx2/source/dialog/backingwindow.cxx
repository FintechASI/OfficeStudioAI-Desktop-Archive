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
 *   except in compliance with the License. You may obtain a copy of the
 *   License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include "backingwindow.hxx"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

#include <osl/file.hxx>
#include <osl/time.h>

#include <basegfx/polygon/b2dpolygon.hxx>
#include <basegfx/polygon/b2dpolygontools.hxx>

#include <vcl/event.hxx>
#include <vcl/help.hxx>
#include <vcl/metaactiontypes.hxx>
#include <vcl/ptrstyle.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/rendercontext/DrawTextFlags.hxx>
#include <vcl/settings.hxx>
#include <vcl/svapp.hxx>
#include <vcl/syswin.hxx>
#include <vcl/weld/Builder.hxx>
#include <vcl/weld/Menu.hxx>

#include <unotools/historyoptions.hxx>
#include <unotools/moduleoptions.hxx>
#include <unotools/cmdoptions.hxx>
#include <unotools/configmgr.hxx>
#include <svtools/openfiledroptargetlistener.hxx>
#include <svtools/colorcfg.hxx>
#include <svtools/langhelp.hxx>
#include <templateviewitem.hxx>
#include <tools/urlobj.hxx>

#include <comphelper/processfactory.hxx>
#include <comphelper/propertysequence.hxx>
#include <comphelper/propertyvalue.hxx>
#include <sfx2/app.hxx>
#include <officecfg/Office/Common.hxx>

#include <i18nlangtag/languagetag.hxx>
#include <comphelper/diagnose_ex.hxx>

#include <com/sun/star/configuration/theDefaultProvider.hpp>
#include <com/sun/star/container/XNameAccess.hpp>
#include <com/sun/star/datatransfer/dnd/XDropTarget.hpp>
#include <com/sun/star/document/MacroExecMode.hpp>
#include <com/sun/star/document/UpdateDocMode.hpp>
#include <com/sun/star/frame/Desktop.hpp>
#include <com/sun/star/lang/XMultiServiceFactory.hpp>
#include <com/sun/star/system/SystemShellExecute.hpp>
#include <com/sun/star/system/SystemShellExecuteFlags.hpp>
#include <com/sun/star/util/URLTransformer.hpp>
#include <com/sun/star/task/InteractionHandler.hpp>

#include <sfx2/viewfrm.hxx>
#include <vcl/commandinfoprovider.hxx>
#include <sfx2/styfitem.hxx>
#include <sfx2/objsh.hxx>
#include <sfx2/tplpitem.hxx>

#include <svl/itemset.hxx>
#include <sfx2/dispatch.hxx>
#include <sfx2/sfxsids.hrc>
#include <sfx2/strings.hrc>
#include <sfx2/sfxresid.hxx>
#include <bitmaps.hlst>
#include <rtl/bootstrap.hxx>
#include <vcl/virdev.hxx>
#include <vcl/graph.hxx>
#include <vcl/graphicfilter.hxx>

#include <config_folders.h>

using namespace ::com::sun::star;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::frame;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::document;

namespace
{
// Approved OfficeStudio AI palette matching OfficeStudioAI-StartCenter.png
constexpr Color COL_NAV_BG(0xED, 0xF1, 0xF6);
constexpr Color COL_NAV_HOVER(0xE1, 0xE7, 0xF0);
constexpr Color COL_NAVY(0x10, 0x23, 0x42);
constexpr Color COL_NAVY_DARK(0x0A, 0x1A, 0x33);
constexpr Color COL_RED_BRAND(0xDA, 0x18, 0x23);
constexpr Color COL_BORDER(0xDC, 0xE3, 0xED);
constexpr Color COL_BORDER_HOVER(0xB4, 0xC2, 0xD6);
constexpr Color COL_TEXT_PRIMARY(0x0F, 0x1F, 0x3D);
constexpr Color COL_TEXT_MUTED(0x55, 0x65, 0x7D);
constexpr Color COL_TEXT_SUBTLE(0x6E, 0x7D, 0x93);
constexpr Color COL_LINK_BLUE(0x1A, 0x63, 0xD8);
constexpr Color COL_SURFACE_WHITE(0xFF, 0xFF, 0xFF);
constexpr Color COL_SURFACE_SUBTLE(0xF8, 0xFA, 0xFC);
constexpr Color COL_SURFACE_HOVER(0xF3, 0xF7, 0xFC);
constexpr Color COL_FOOTER_BG(0xF2, 0xF5, 0xF9);

constexpr Color COL_CARD_DOCS(0x1A, 0x73, 0xE8);
constexpr Color COL_CARD_SHEET(0x16, 0xA3, 0x4A);
constexpr Color COL_CARD_SLIDE(0xEA, 0x58, 0x0C);
constexpr Color COL_CARD_FLOW(0x7C, 0x3A, 0xED);

vcl::Font MakeUiFont(int nHeightPx, FontWeight eWeight = WEIGHT_NORMAL)
{
    vcl::Font aFont(Application::GetSettings().GetStyleSettings().GetLabelFont());
    aFont.SetFamilyName(u"Segoe UI"_ustr);
    aFont.SetFontHeight(nHeightPx);
    aFont.SetWeight(eWeight);
    aFont.SetTransparent(true);
    return aFont;
}

void DrawStrokePoly(vcl::RenderContext& rRenderContext, const std::vector<basegfx::B2DPoint>& rPts,
                    bool bClosed, Color aColor, double fWidth)
{
    basegfx::B2DPolygon aPoly;
    for (const auto& pt : rPts)
        aPoly.append(pt);
    aPoly.setClosed(bClosed);
    rRenderContext.SetFillColor();
    rRenderContext.SetLineColor(aColor);
    rRenderContext.DrawPolyLine(aPoly, fWidth, basegfx::B2DLineJoin::Round, css::drawing::LineCap_ROUND);
}

void FillSolidRect(vcl::RenderContext& rRenderContext, const tools::Rectangle& rRect, Color aFill)
{
    rRenderContext.SetLineColor();
    rRenderContext.SetFillColor(aFill);
    rRenderContext.DrawRect(rRect);
}

basegfx::B2DPolygon MakeRoundedRectPoly(const tools::Rectangle& rRect, double fRadiusPx)
{
    const double w = std::max<double>(1.0, rRect.GetWidth());
    const double h = std::max<double>(1.0, rRect.GetHeight());
    const double rx = std::min(1.0, (2.0 * fRadiusPx) / w);
    const double ry = std::min(1.0, (2.0 * fRadiusPx) / h);
    return basegfx::utils::createPolygonFromRect(
        basegfx::B2DRectangle(rRect.Left(), rRect.Top(), rRect.Right(), rRect.Bottom()), rx, ry);
}

void DrawRoundedBox(vcl::RenderContext& rRenderContext, const tools::Rectangle& rRect, double fRadiusPx,
                    Color aFill, Color aStroke, double fStrokeWidth = 1.0)
{
    const basegfx::B2DPolygon aPoly = MakeRoundedRectPoly(rRect, fRadiusPx);
    if (aFill != COL_TRANSPARENT)
    {
        rRenderContext.SetLineColor();
        rRenderContext.SetFillColor(aFill);
        rRenderContext.DrawPolygon(aPoly);
    }
    if (aStroke != COL_TRANSPARENT && fStrokeWidth > 0.0)
    {
        rRenderContext.SetFillColor();
        rRenderContext.SetLineColor(aStroke);
        rRenderContext.DrawPolyLine(aPoly, fStrokeWidth, basegfx::B2DLineJoin::Round, css::drawing::LineCap_ROUND);
    }
}

void DrawCircleStroke(vcl::RenderContext& rRenderContext, double fCx, double fCy, double fRadius,
                      Color aStroke, double fStrokeWidth = 1.6)
{
    basegfx::B2DPolygon aCircle = basegfx::utils::createPolygonFromCircle(basegfx::B2DPoint(fCx, fCy), fRadius);
    rRenderContext.SetFillColor();
    rRenderContext.SetLineColor(aStroke);
    rRenderContext.DrawPolyLine(aCircle, fStrokeWidth, basegfx::B2DLineJoin::Round);
}

void DrawMiniDocIcon(vcl::RenderContext& rRenderContext, int nX, int nY, int nSize, const OUString& rExt)
{
    const OUString aExt = rExt.toAsciiLowerCase();
    int nKind = 0; // 0=other, 1=docs, 2=sheet, 3=slides, 4=flows
    if (aExt == u"odt" || aExt == u"fodt" || aExt == u"doc" || aExt == u"docx" || aExt == u"rtf" || aExt == u"txt" || aExt == u"ott")
        nKind = 1;
    else if (aExt == u"ods" || aExt == u"fods" || aExt == u"xls" || aExt == u"xlsx" || aExt == u"csv" || aExt == u"ots")
        nKind = 2;
    else if (aExt == u"odp" || aExt == u"fodp" || aExt == u"ppt" || aExt == u"pptx" || aExt == u"pps" || aExt == u"otp")
        nKind = 3;
    else if (aExt == u"odg" || aExt == u"fodg" || aExt == u"otg" || aExt == u"vsd" || aExt == u"vsdx")
        nKind = 4;

    Color aColor = COL_TEXT_MUTED;
    if (nKind == 1) aColor = COL_CARD_DOCS;
    else if (nKind == 2) aColor = COL_CARD_SHEET;
    else if (nKind == 3) aColor = COL_CARD_SLIDE;
    else if (nKind == 4) aColor = COL_CARD_FLOW;

    const double x = nX;
    const double y = nY;
    const double w = nSize;
    const double h = nSize;

    if (nKind == 2)
    {
        DrawRoundedBox(rRenderContext, tools::Rectangle(Point(nX + 1, nY + 2), Size(nSize - 2, nSize - 4)), 2.5, COL_SURFACE_WHITE, aColor, 1.5);
        DrawStrokePoly(rRenderContext, { { x + 2, y + h * 0.42 }, { x + w - 2, y + h * 0.42 } }, false, aColor, 1.3);
        DrawStrokePoly(rRenderContext, { { x + 2, y + h * 0.66 }, { x + w - 2, y + h * 0.66 } }, false, aColor, 1.3);
        DrawStrokePoly(rRenderContext, { { x + w * 0.42, y + 3 }, { x + w * 0.42, y + h - 3 } }, false, aColor, 1.3);
        DrawStrokePoly(rRenderContext, { { x + w * 0.68, y + 3 }, { x + w * 0.68, y + h - 3 } }, false, aColor, 1.3);
    }
    else if (nKind == 3)
    {
        DrawRoundedBox(rRenderContext, tools::Rectangle(Point(nX + 1, nY + 2), Size(nSize - 2, nSize - 7)), 2.5, COL_SURFACE_WHITE, aColor, 1.5);
        DrawStrokePoly(rRenderContext, { { x + w * 0.5, y + h - 5 }, { x + w * 0.3, y + h - 1 } }, false, aColor, 1.5);
        DrawStrokePoly(rRenderContext, { { x + w * 0.5, y + h - 5 }, { x + w * 0.7, y + h - 1 } }, false, aColor, 1.5);
        basegfx::B2DPolygon aTri;
        aTri.append(basegfx::B2DPoint(x + w * 0.42, y + h * 0.28));
        aTri.append(basegfx::B2DPoint(x + w * 0.42, y + h * 0.54));
        aTri.append(basegfx::B2DPoint(x + w * 0.66, y + h * 0.41));
        aTri.setClosed(true);
        rRenderContext.SetLineColor();
        rRenderContext.SetFillColor(aColor);
        rRenderContext.DrawPolygon(aTri);
    }
    else if (nKind == 4)
    {
        DrawRoundedBox(rRenderContext, tools::Rectangle(Point(nX + nSize / 2 - 4, nY + 2), Size(8, 5)), 1.5, COL_SURFACE_WHITE, aColor, 1.4);
        DrawRoundedBox(rRenderContext, tools::Rectangle(Point(nX + 2, nY + nSize - 8), Size(7, 5)), 1.5, COL_SURFACE_WHITE, aColor, 1.4);
        DrawRoundedBox(rRenderContext, tools::Rectangle(Point(nX + nSize - 9, nY + nSize - 8), Size(7, 5)), 1.5, COL_SURFACE_WHITE, aColor, 1.4);
        DrawStrokePoly(rRenderContext, { { x + w * 0.5, y + 7 }, { x + w * 0.5, y + h * 0.55 } }, false, aColor, 1.3);
        DrawStrokePoly(rRenderContext, { { x + 5.5, y + h - 8 }, { x + 5.5, y + h * 0.55 }, { x + w - 5.5, y + h * 0.55 }, { x + w - 5.5, y + h - 8 } }, false, aColor, 1.3);
    }
    else
    {
        const double fold = w * 0.28;
        DrawStrokePoly(rRenderContext, {
            { x + 3, y + 1 },
            { x + w - 3 - fold, y + 1 },
            { x + w - 3, y + 1 + fold },
            { x + w - 3, y + h - 1 },
            { x + 3, y + h - 1 }
        }, true, aColor, 1.6);
        DrawStrokePoly(rRenderContext, { { x + 6.5, y + h * 0.46 }, { x + w - 6.5, y + h * 0.46 } }, false, aColor, 1.5);
        DrawStrokePoly(rRenderContext, { { x + 6.5, y + h * 0.64 }, { x + w - 6.5, y + h * 0.64 } }, false, aColor, 1.5);
        DrawStrokePoly(rRenderContext, { { x + 6.5, y + h * 0.80 }, { x + w * 0.62, y + h * 0.80 } }, false, aColor, 1.5);
    }
}

OUString FormatModifyTime(const OUString& rURL)
{
    INetURLObject aURLObj(rURL);
    if (aURLObj.GetProtocol() != INetProtocol::File)
        return u"—"_ustr;

    osl::DirectoryItem aItem;
    if (osl::DirectoryItem::get(rURL, aItem) != osl::FileBase::E_None)
        return u"—"_ustr;

    osl::FileStatus aStatus(osl_FileStatus_Mask_ModifyTime);
    if (aItem.getFileStatus(aStatus) != osl::FileBase::E_None || !aStatus.isValid(osl_FileStatus_Mask_ModifyTime))
        return u"—"_ustr;

    TimeValue aFileUtc = aStatus.getModifyTime();
    TimeValue aFileLocal{};
    oslDateTime aFileDt{};
    if (!osl_getLocalTimeFromSystemTime(&aFileUtc, &aFileLocal) ||
        !osl_getDateTimeFromTimeValue(&aFileLocal, &aFileDt))
        return u"—"_ustr;

    TimeValue aNowUtc{};
    TimeValue aNowLocal{};
    oslDateTime aNowDt{};
    osl_getSystemTime(&aNowUtc);
    osl_getLocalTimeFromSystemTime(&aNowUtc, &aNowLocal);
    osl_getDateTimeFromTimeValue(&aNowLocal, &aNowDt);

    const sal_uInt16 nHour24 = aFileDt.Hours;
    const sal_uInt16 nHour12 = (nHour24 % 12 == 0) ? 12 : (nHour24 % 12);
    const OUString sAmPm = (nHour24 >= 12) ? u"PM"_ustr : u"AM"_ustr;
    OUString sMin = OUString::number(aFileDt.Minutes);
    if (aFileDt.Minutes < 10)
        sMin = u"0"_ustr + sMin;
    const OUString sTime = OUString::number(nHour12) + u":" + sMin + u" " + sAmPm;

    if (aFileDt.Year == aNowDt.Year && aFileDt.Month == aNowDt.Month && aFileDt.Day == aNowDt.Day)
        return u"Today, "_ustr + sTime;

    static const std::array<OUString, 12> aMonths = {
        u"Jan"_ustr, u"Feb"_ustr, u"Mar"_ustr, u"Apr"_ustr, u"May"_ustr, u"Jun"_ustr,
        u"Jul"_ustr, u"Aug"_ustr, u"Sep"_ustr, u"Oct"_ustr, u"Nov"_ustr, u"Dec"_ustr
    };
    OUString sMonth = (aFileDt.Month >= 1 && aFileDt.Month <= 12) ? aMonths[aFileDt.Month - 1] : u""_ustr;
    return sMonth + u" " + OUString::number(aFileDt.Day) + u", " + OUString::number(aFileDt.Year) + u", " + sTime;
}

} // namespace

// ---------------------------------------------------------------------------
// BrandImage: top header banner across the right main pane
// ---------------------------------------------------------------------------
class BrandImage final : public weld::CustomWidgetController
{
private:
    BackingWindow& mrOwner;
    Bitmap maLogoBitmap;
    tools::Long mnLoadedWidth = 0;

    void EnsureLogoLoaded(int nTargetLogoWidth)
    {
        if (nTargetLogoWidth <= 0)
            return;
        if (maLogoBitmap.IsEmpty() || std::abs(mnLoadedWidth - nTargetLogoWidth) > 20)
        {
            SfxApplication::loadBrandSvg(u"shell/logo", maLogoBitmap, nTargetLogoWidth);
            mnLoadedWidth = nTargetLogoWidth;
        }
    }

public:
    explicit BrandImage(BackingWindow& rOwner)
        : mrOwner(rOwner)
    {
    }

    virtual void SetDrawingArea(weld::DrawingArea* pDrawingArea) override
    {
        weld::CustomWidgetController::SetDrawingArea(pDrawingArea);
        pDrawingArea->get_ref_device().SetBackground(Wallpaper(COL_SURFACE_WHITE));
        pDrawingArea->set_size_request(-1, 88);
        SetPointer(PointerStyle::RefHand);
    }

    virtual void Resize() override
    {
        EnsureLogoLoaded(248);
        weld::CustomWidgetController::Resize();
    }

    virtual bool MouseButtonUp(const MouseEvent& rMEvt) override
    {
        if (rMEvt.IsLeft())
            mrOwner.triggerAbout();
        return true;
    }

    virtual void Paint(vcl::RenderContext& rRenderContext, const tools::Rectangle&) override
    {
        auto popIt = rRenderContext.ScopedPush(vcl::PushFlags::ALL);
        rRenderContext.SetAntialiasing(AntialiasingFlags::Enable);

        const Size aSize = GetOutputSizePixel();
        const tools::Long w = aSize.Width();
        const tools::Long h = aSize.Height();

        FillSolidRect(rRenderContext, tools::Rectangle(Point(0, 0), aSize), COL_SURFACE_WHITE);

        // Top-right sweeping curved corner accents (red outer wave + navy inner corner)
        {
            basegfx::B2DPolygon aRedWave;
            aRedWave.append(basegfx::B2DPoint(std::max<double>(w * 0.72, w - 270.0), h - 1));
            aRedWave.appendBezierSegment(
                basegfx::B2DPoint(std::max<double>(w * 0.85, w - 145.0), h * 0.72),
                basegfx::B2DPoint(std::max<double>(w * 0.94, w - 58.0), h * 0.36),
                basegfx::B2DPoint(w, 0));
            aRedWave.append(basegfx::B2DPoint(w, h * 0.62));
            aRedWave.appendBezierSegment(
                basegfx::B2DPoint(std::max<double>(w * 0.93, w - 68.0), h * 0.78),
                basegfx::B2DPoint(std::max<double>(w * 0.85, w - 150.0), h - 1),
                basegfx::B2DPoint(std::max<double>(w * 0.72, w - 270.0), h - 1));
            aRedWave.setClosed(true);
            rRenderContext.SetLineColor();
            rRenderContext.SetFillColor(COL_RED_BRAND);
            rRenderContext.DrawPolygon(aRedWave);

            basegfx::B2DPolygon aNavyWave;
            aNavyWave.append(basegfx::B2DPoint(std::max<double>(w * 0.78, w - 210.0), h - 1));
            aNavyWave.appendBezierSegment(
                basegfx::B2DPoint(std::max<double>(w * 0.88, w - 110.0), h * 0.68),
                basegfx::B2DPoint(std::max<double>(w * 0.95, w - 42.0), h * 0.32),
                basegfx::B2DPoint(w, 0));
            aNavyWave.append(basegfx::B2DPoint(w, h * 0.40));
            aNavyWave.appendBezierSegment(
                basegfx::B2DPoint(std::max<double>(w * 0.96, w - 34.0), h * 0.56),
                basegfx::B2DPoint(std::max<double>(w * 0.89, w - 102.0), h * 0.82),
                basegfx::B2DPoint(std::max<double>(w * 0.81, w - 180.0), h - 1));
            aNavyWave.setClosed(true);
            rRenderContext.SetFillColor(COL_NAVY_DARK);
            rRenderContext.DrawPolygon(aNavyWave);
        }

        // Approved OfficeStudio AI wordmark logo on the left
        EnsureLogoLoaded(248);
        tools::Long nDivX = 28;
        if (!maLogoBitmap.IsEmpty())
        {
            const Size aImg = maLogoBitmap.GetSizePixel();
            const tools::Long nDrawH = 34;
            const tools::Long nDrawW = aImg.Height() > 0
                ? std::max<tools::Long>(160, (nDrawH * aImg.Width()) / aImg.Height())
                : 248;
            const tools::Long nLogoX = 28;
            const tools::Long nLogoY = (h - nDrawH) / 2;
            rRenderContext.DrawBitmap(Point(nLogoX, nLogoY), Size(nDrawW, nDrawH), maLogoBitmap);
            nDivX = nLogoX + nDrawW + 22;
        }

        // Vertical divider + muted tagline wrapped cleanly in 2 lines
        const tools::Long nRightLimit = std::max<tools::Long>(nDivX + 20, w - 205);
        if (nRightLimit - nDivX > 120)
        {
            DrawStrokePoly(rRenderContext, { { static_cast<double>(nDivX), 22.0 }, { static_cast<double>(nDivX), h - 22.0 } },
                           false, Color(0xD5, 0xDC, 0xE6), 1.0);

            rRenderContext.SetFont(MakeUiFont(13, WEIGHT_NORMAL));
            rRenderContext.SetTextColor(COL_TEXT_MUTED);
            const tools::Long nTagW = std::min<tools::Long>(340, nRightLimit - (nDivX + 20));
            tools::Rectangle aTagRect(Point(nDivX + 20, 16), Size(nTagW, h - 32));
            rRenderContext.DrawText(aTagRect,
                u"A modern, powerful office suite for documents, spreadsheets, presentations, and diagrams."_ustr,
                DrawTextFlags::Left | DrawTextFlags::VCenter | DrawTextFlags::WordBreak | DrawTextFlags::MultiLine);
        }

        DrawStrokePoly(rRenderContext, { { 0.0, static_cast<double>(h - 1) }, { static_cast<double>(w), static_cast<double>(h - 1) } },
                       false, COL_BORDER, 1.0);
    }
};

// ---------------------------------------------------------------------------
// SidebarNavWidget: left vertical navigation rail (#EDF1F6, active navy pill)
// ---------------------------------------------------------------------------
class SidebarNavWidget final : public weld::CustomWidgetController
{
private:
    BackingWindow& mrOwner;
    int mnHoveredIdx = -1; // 0:Home, 1:Recent, 2:Templates, 3:Open, 4:Remote, 5:Help
    int mnFocusedIdx = 0;
    bool mbRemoteEnabled = true;

    struct NavHit
    {
        int nId;
        tools::Rectangle aRect;
    };

    std::vector<NavHit> GetHitRects() const
    {
        const Size aSz = GetOutputSizePixel();
        const tools::Long w = aSz.Width();
        const tools::Long h = aSz.Height();
        std::vector<NavHit> aOut;
        const tools::Long nPadX = 14;
        const tools::Long nItemW = std::max<tools::Long>(60, w - 2 * nPadX);
        const tools::Long nItemH = 40;
        const tools::Long nGap = 6;
        tools::Long y = 20;
        for (int i = 0; i < 4; ++i)
        {
            aOut.push_back({ i, tools::Rectangle(Point(nPadX, y), Size(nItemW, nItemH)) });
            y += nItemH + nGap;
        }
        if (h > 290)
        {
            tools::Long uy = h - 42;
            aOut.push_back({ 5, tools::Rectangle(Point(nPadX, uy), Size(nItemW, 30)) });
            if (mbRemoteEnabled && h > 330)
                aOut.push_back({ 4, tools::Rectangle(Point(nPadX, uy - 34), Size(nItemW, 30)) });
        }
        return aOut;
    }

    int HitTest(const Point& rPos) const
    {
        for (const auto& item : GetHitRects())
        {
            if (item.aRect.Contains(rPos))
                return item.nId;
        }
        return -1;
    }

    void ActivateId(int nId)
    {
        if (nId == 0)
            mrOwner.activateMode(StartCenterMode::Home);
        else if (nId == 1)
            mrOwner.activateMode(StartCenterMode::Recent);
        else if (nId == 2)
            mrOwner.activateMode(StartCenterMode::Templates);
        else if (nId == 3)
            mrOwner.triggerOpen();
        else if (nId == 4)
            mrOwner.triggerOpenRemote();
        else if (nId == 5)
            mrOwner.triggerHelp();
    }

    void DrawNavIcon(vcl::RenderContext& rRenderContext, int nId, const tools::Rectangle& rRect, Color aCol) const
    {
        const double cx = rRect.Left() + 24.0;
        const double cy = rRect.Top() + rRect.GetHeight() / 2.0;
        if (nId == 0) // House
        {
            DrawStrokePoly(rRenderContext, {
                { cx - 7, cy - 1 }, { cx, cy - 7 }, { cx + 7, cy - 1 }
            }, false, aCol, 1.8);
            DrawStrokePoly(rRenderContext, {
                { cx - 5, cy - 2 }, { cx - 5, cy + 6 }, { cx + 5, cy + 6 }, { cx + 5, cy - 2 }
            }, false, aCol, 1.8);
        }
        else if (nId == 1) // Clock
        {
            DrawCircleStroke(rRenderContext, cx, cy, 7.0, aCol, 1.7);
            DrawStrokePoly(rRenderContext, { { cx, cy - 4 }, { cx, cy }, { cx + 3.5, cy + 2 } }, false, aCol, 1.7);
        }
        else if (nId == 2) // Layers / Templates
        {
            DrawStrokePoly(rRenderContext, { { cx, cy - 7 }, { cx + 7, cy - 3.5 }, { cx, cy }, { cx - 7, cy - 3.5 } }, true, aCol, 1.6);
            DrawStrokePoly(rRenderContext, { { cx - 7, cy + 0.5 }, { cx, cy + 4 }, { cx + 7, cy + 0.5 } }, false, aCol, 1.6);
            DrawStrokePoly(rRenderContext, { { cx - 7, cy + 4.5 }, { cx, cy + 8 }, { cx + 7, cy + 4.5 } }, false, aCol, 1.6);
        }
        else if (nId == 3) // Open folder
        {
            DrawStrokePoly(rRenderContext, {
                { cx - 7, cy - 6 }, { cx - 2, cy - 6 }, { cx, cy - 4 },
                { cx + 7, cy - 4 }, { cx + 7, cy + 6 }, { cx - 7, cy + 6 }
            }, true, aCol, 1.7);
        }
        else if (nId == 4) // Globe / Remote
        {
            DrawCircleStroke(rRenderContext, cx, cy, 6.0, aCol, 1.5);
            DrawStrokePoly(rRenderContext, { { cx - 6, cy }, { cx + 6, cy } }, false, aCol, 1.4);
            DrawStrokePoly(rRenderContext, { { cx, cy - 6 }, { cx, cy + 6 } }, false, aCol, 1.3);
        }
        else if (nId == 5) // Help circle
        {
            DrawCircleStroke(rRenderContext, cx, cy, 6.0, aCol, 1.5);
            DrawStrokePoly(rRenderContext, { { cx - 2, cy - 2 }, { cx, cy - 3.5 }, { cx + 2, cy - 2 }, { cx, cy + 0.5 } }, false, aCol, 1.4);
            FillSolidRect(rRenderContext, tools::Rectangle(Point(static_cast<tools::Long>(cx), static_cast<tools::Long>(cy + 3)), Size(2, 2)), aCol);
        }
    }

public:
    StartCenterMode meActiveMode = StartCenterMode::Home;

    explicit SidebarNavWidget(BackingWindow& rOwner)
        : mrOwner(rOwner)
    {
    }

    void SetRemoteEnabled(bool bEnabled)
    {
        mbRemoteEnabled = bEnabled;
        Invalidate();
    }

    virtual void SetDrawingArea(weld::DrawingArea* pDrawingArea) override
    {
        weld::CustomWidgetController::SetDrawingArea(pDrawingArea);
        pDrawingArea->get_ref_device().SetBackground(Wallpaper(COL_NAV_BG));
        SetPointer(PointerStyle::RefHand);
    }

    virtual bool MouseMove(const MouseEvent& rMEvt) override
    {
        int nHit = rMEvt.IsLeaveWindow() ? -1 : HitTest(rMEvt.GetPosPixel());
        if (nHit != mnHoveredIdx)
        {
            mnHoveredIdx = nHit;
            Invalidate();
        }
        SetPointer(nHit >= 0 ? PointerStyle::RefHand : PointerStyle::Arrow);
        return false;
    }

    virtual bool MouseButtonUp(const MouseEvent& rMEvt) override
    {
        if (rMEvt.IsLeft())
        {
            int nHit = HitTest(rMEvt.GetPosPixel());
            if (nHit >= 0)
            {
                mnFocusedIdx = nHit;
                ActivateId(nHit);
                return true;
            }
        }
        return false;
    }

    virtual bool KeyInput(const KeyEvent& rKEvt) override
    {
        sal_uInt16 nCode = rKEvt.GetKeyCode().GetCode();
        if (nCode == KEY_DOWN)
        {
            mnFocusedIdx = (mnFocusedIdx + 1) % 4;
            Invalidate();
            return true;
        }
        if (nCode == KEY_UP)
        {
            mnFocusedIdx = (mnFocusedIdx + 3) % 4;
            Invalidate();
            return true;
        }
        if (nCode == KEY_RETURN || nCode == KEY_SPACE)
        {
            ActivateId(mnFocusedIdx);
            return true;
        }
        return false;
    }

    virtual void Paint(vcl::RenderContext& rRenderContext, const tools::Rectangle&) override
    {
        auto popIt = rRenderContext.ScopedPush(vcl::PushFlags::ALL);
        rRenderContext.SetAntialiasing(AntialiasingFlags::Enable);
        const Size aSz = GetOutputSizePixel();

        FillSolidRect(rRenderContext, tools::Rectangle(Point(0, 0), aSz), COL_NAV_BG);
        DrawStrokePoly(rRenderContext, { { static_cast<double>(aSz.Width() - 1), 0.0 }, { static_cast<double>(aSz.Width() - 1), static_cast<double>(aSz.Height()) } },
                       false, COL_BORDER, 1.0);

        const int nActiveId = meActiveMode == StartCenterMode::Home ? 0
                            : (meActiveMode == StartCenterMode::Recent ? 1 : 2);

        for (const auto& item : GetHitRects())
        {
            const bool bActive = (item.nId == nActiveId);
            const bool bHover = (item.nId == mnHoveredIdx);
            if (bActive)
                DrawRoundedBox(rRenderContext, item.aRect, 8.0, COL_NAVY, COL_TRANSPARENT, 0.0);
            else if (bHover)
                DrawRoundedBox(rRenderContext, item.aRect, 8.0, COL_NAV_HOVER, COL_TRANSPARENT, 0.0);

            if (HasFocus() && item.nId == mnFocusedIdx && !bActive)
                DrawRoundedBox(rRenderContext, item.aRect, 8.0, COL_TRANSPARENT, COL_BORDER_HOVER, 1.2);

            Color aFg = bActive ? COL_SURFACE_WHITE : (item.nId >= 4 ? COL_TEXT_MUTED : COL_TEXT_PRIMARY);
            DrawNavIcon(rRenderContext, item.nId, item.aRect, aFg);

            OUString sLabel;
            if (item.nId == 0) sLabel = u"Home"_ustr;
            else if (item.nId == 1) sLabel = u"Recent"_ustr;
            else if (item.nId == 2) sLabel = u"Templates"_ustr;
            else if (item.nId == 3) sLabel = u"Open"_ustr;
            else if (item.nId == 4) sLabel = u"Remote Files"_ustr;
            else sLabel = u"Help"_ustr;

            const int nFontH = item.nId >= 4 ? 13 : 14;
            rRenderContext.SetFont(MakeUiFont(nFontH, bActive ? WEIGHT_SEMIBOLD : WEIGHT_NORMAL));
            rRenderContext.SetTextColor(aFg);
            tools::Rectangle aTxtRect(item.aRect.Left() + 44, item.aRect.Top(), item.aRect.Right() - 8, item.aRect.Bottom());
            rRenderContext.DrawText(aTxtRect, sLabel, DrawTextFlags::Left | DrawTextFlags::VCenter);
        }
    }
};

// ---------------------------------------------------------------------------
// CreateCardsWidget: "Create New" heading + 4 color-accented app cards
// ---------------------------------------------------------------------------
class CreateCardsWidget final : public weld::CustomWidgetController
{
private:
    BackingWindow& mrOwner;
    int mnHoveredCard = -1;
    int mnFocusedCard = 0;
    std::array<bool, 4> maEnabled = { true, true, true, true };

    tools::Rectangle GetCardRect(int nIndex) const
    {
        const Size aSz = GetOutputSizePixel();
        const tools::Long nPadX = 28;
        const tools::Long nGap = 14;
        const tools::Long nAvailW = std::max<tools::Long>(320, aSz.Width() - 2 * nPadX - 3 * nGap);
        const tools::Long nCardW = nAvailW / 4;
        const tools::Long nTop = 48;
        const tools::Long nCardH = std::max<tools::Long>(110, aSz.Height() - nTop - 14);
        return tools::Rectangle(Point(nPadX + nIndex * (nCardW + nGap), nTop), Size(nCardW, nCardH));
    }

    int HitTest(const Point& rPos) const
    {
        for (int i = 0; i < 4; ++i)
        {
            if (GetCardRect(i).Contains(rPos))
                return i;
        }
        return -1;
    }

    void DrawCardIcon(vcl::RenderContext& rRenderContext, int nIdx, int nCx, int nCy, Color aAccent) const
    {
        const double x = nCx;
        const double y = nCy;
        if (nIdx == 0) // Docs
        {
            const double w = 30, h = 38, fold = 9;
            DrawStrokePoly(rRenderContext, {
                { x - w / 2, y - h / 2 },
                { x + w / 2 - fold, y - h / 2 },
                { x + w / 2, y - h / 2 + fold },
                { x + w / 2, y + h / 2 },
                { x - w / 2, y + h / 2 }
            }, true, aAccent, 2.5);
            DrawStrokePoly(rRenderContext, { { x + w / 2 - fold, y - h / 2 }, { x + w / 2 - fold, y - h / 2 + fold }, { x + w / 2, y - h / 2 + fold } }, false, aAccent, 2.0);
            DrawStrokePoly(rRenderContext, { { x - 8, y - 3 }, { x + 8, y - 3 } }, false, aAccent, 2.3);
            DrawStrokePoly(rRenderContext, { { x - 8, y + 4 }, { x + 8, y + 4 } }, false, aAccent, 2.3);
            DrawStrokePoly(rRenderContext, { { x - 8, y + 11 }, { x + 2, y + 11 } }, false, aAccent, 2.3);
        }
        else if (nIdx == 1) // Spreadsheet
        {
            DrawRoundedBox(rRenderContext, tools::Rectangle(Point(nCx - 17, nCy - 17), Size(34, 34)), 5.0, COL_SURFACE_WHITE, aAccent, 2.4);
            DrawStrokePoly(rRenderContext, { { x - 17, y - 6 }, { x + 17, y - 6 } }, false, aAccent, 2.2);
            DrawStrokePoly(rRenderContext, { { x - 17, y + 5 }, { x + 17, y + 5 } }, false, aAccent, 2.2);
            DrawStrokePoly(rRenderContext, { { x - 6, y - 17 }, { x - 6, y + 17 } }, false, aAccent, 2.2);
            DrawStrokePoly(rRenderContext, { { x + 6, y - 17 }, { x + 6, y + 17 } }, false, aAccent, 2.2);
        }
        else if (nIdx == 2) // SlideShows
        {
            DrawRoundedBox(rRenderContext, tools::Rectangle(Point(nCx - 19, nCy - 14), Size(38, 26)), 5.0, COL_SURFACE_WHITE, aAccent, 2.5);
            DrawStrokePoly(rRenderContext, { { x, y - 18 }, { x, y - 14 } }, false, aAccent, 2.4);
            DrawStrokePoly(rRenderContext, { { x - 8, y + 19 }, { x, y + 12 }, { x + 8, y + 19 } }, false, aAccent, 2.4);
            basegfx::B2DPolygon aPlay;
            aPlay.append(basegfx::B2DPoint(x - 4, y - 7));
            aPlay.append(basegfx::B2DPoint(x - 4, y + 5));
            aPlay.append(basegfx::B2DPoint(x + 7, y - 1));
            aPlay.setClosed(true);
            rRenderContext.SetLineColor();
            rRenderContext.SetFillColor(aAccent);
            rRenderContext.DrawPolygon(aPlay);
        }
        else // FlowCharts
        {
            DrawRoundedBox(rRenderContext, tools::Rectangle(Point(nCx - 8, nCy - 18), Size(16, 11)), 3.0, COL_SURFACE_WHITE, aAccent, 2.3);
            DrawRoundedBox(rRenderContext, tools::Rectangle(Point(nCx - 20, nCy + 6), Size(16, 11)), 3.0, COL_SURFACE_WHITE, aAccent, 2.3);
            DrawRoundedBox(rRenderContext, tools::Rectangle(Point(nCx + 4, nCy + 6), Size(16, 11)), 3.0, COL_SURFACE_WHITE, aAccent, 2.3);
            DrawStrokePoly(rRenderContext, { { x, y - 7 }, { x, y - 1 } }, false, aAccent, 2.3);
            DrawStrokePoly(rRenderContext, { { x - 12, y + 6 }, { x - 12, y - 1 }, { x + 12, y - 1 }, { x + 12, y + 6 } }, false, aAccent, 2.3);
        }
    }

public:
    explicit CreateCardsWidget(BackingWindow& rOwner)
        : mrOwner(rOwner)
    {
    }

    void SetModuleStates(bool bDocs, bool bCalc, bool bImpress, bool bDraw)
    {
        maEnabled = { bDocs, bCalc, bImpress, bDraw };
        Invalidate();
    }

    virtual void SetDrawingArea(weld::DrawingArea* pDrawingArea) override
    {
        weld::CustomWidgetController::SetDrawingArea(pDrawingArea);
        pDrawingArea->get_ref_device().SetBackground(Wallpaper(COL_SURFACE_WHITE));
        pDrawingArea->set_size_request(-1, 196);
    }

    virtual bool MouseMove(const MouseEvent& rMEvt) override
    {
        int nHit = rMEvt.IsLeaveWindow() ? -1 : HitTest(rMEvt.GetPosPixel());
        if (nHit >= 0 && !maEnabled[nHit])
            nHit = -1;
        if (nHit != mnHoveredCard)
        {
            mnHoveredCard = nHit;
            Invalidate();
        }
        SetPointer(nHit >= 0 ? PointerStyle::RefHand : PointerStyle::Arrow);
        return false;
    }

    virtual bool MouseButtonUp(const MouseEvent& rMEvt) override
    {
        if (rMEvt.IsLeft())
        {
            int nHit = HitTest(rMEvt.GetPosPixel());
            if (nHit >= 0 && maEnabled[nHit])
            {
                mnFocusedCard = nHit;
                mrOwner.triggerCreateApp(nHit);
                return true;
            }
        }
        return false;
    }

    virtual bool KeyInput(const KeyEvent& rKEvt) override
    {
        sal_uInt16 nCode = rKEvt.GetKeyCode().GetCode();
        if (nCode == KEY_RIGHT)
        {
            mnFocusedCard = (mnFocusedCard + 1) % 4;
            Invalidate();
            return true;
        }
        if (nCode == KEY_LEFT)
        {
            mnFocusedCard = (mnFocusedCard + 3) % 4;
            Invalidate();
            return true;
        }
        if ((nCode == KEY_RETURN || nCode == KEY_SPACE) && maEnabled[mnFocusedCard])
        {
            mrOwner.triggerCreateApp(mnFocusedCard);
            return true;
        }
        return false;
    }

    virtual void Paint(vcl::RenderContext& rRenderContext, const tools::Rectangle&) override
    {
        auto popIt = rRenderContext.ScopedPush(vcl::PushFlags::ALL);
        rRenderContext.SetAntialiasing(AntialiasingFlags::Enable);
        const Size aSz = GetOutputSizePixel();

        FillSolidRect(rRenderContext, tools::Rectangle(Point(0, 0), aSz), COL_SURFACE_WHITE);

        // Section heading
        rRenderContext.SetFont(MakeUiFont(17, WEIGHT_BOLD));
        rRenderContext.SetTextColor(COL_TEXT_PRIMARY);
        rRenderContext.DrawText(tools::Rectangle(Point(28, 14), Size(300, 26)), u"Create New"_ustr,
                                DrawTextFlags::Left | DrawTextFlags::VCenter);

        static const std::array<Color, 4> aAccents = { COL_CARD_DOCS, COL_CARD_SHEET, COL_CARD_SLIDE, COL_CARD_FLOW };
        static const std::array<OUString, 4> aTitles = { u"Docs"_ustr, u"Spreadsheet"_ustr, u"SlideShows"_ustr, u"FlowCharts"_ustr };
        static const std::array<OUString, 4> aSubs = {
            u"Create text documents"_ustr,
            u"Work with data and tables"_ustr,
            u"Create presentations"_ustr,
            u"Design diagrams and flows"_ustr
        };

        for (int i = 0; i < 4; ++i)
        {
            const tools::Rectangle aCard = GetCardRect(i);
            const bool bHover = (i == mnHoveredCard);
            const bool bFocus = HasFocus() && (i == mnFocusedCard);

            // Subtle drop elevation under card + crisp rounded white card
            tools::Rectangle aShadow(Point(aCard.Left() + 1, aCard.Top() + 2), Size(aCard.GetWidth(), aCard.GetHeight()));
            DrawRoundedBox(rRenderContext, aShadow, 8.0, Color(0xEC, 0xF0, 0xF5), COL_TRANSPARENT, 0.0);

            DrawRoundedBox(rRenderContext, aCard, 8.0,
                           bHover ? COL_SURFACE_SUBTLE : COL_SURFACE_WHITE,
                           (bHover || bFocus) ? COL_BORDER_HOVER : COL_BORDER, 1.2);

            // Crisp 3.5px colored bottom accent strip inside card
            tools::Rectangle aBar(Point(aCard.Left() + 4, aCard.Bottom() - 4), Size(std::max<tools::Long>(10, aCard.GetWidth() - 8), 4));
            DrawRoundedBox(rRenderContext, aBar, 2.0, maEnabled[i] ? aAccents[i] : COL_TEXT_SUBTLE, COL_TRANSPARENT, 0.0);

            const int nCx = static_cast<int>((aCard.Left() + aCard.Right()) / 2);
            const int nCy = static_cast<int>(aCard.Top() + aCard.GetHeight() * 0.34);
            DrawCardIcon(rRenderContext, i, nCx, nCy, maEnabled[i] ? aAccents[i] : COL_TEXT_SUBTLE);

            rRenderContext.SetFont(MakeUiFont(15, WEIGHT_BOLD));
            rRenderContext.SetTextColor(maEnabled[i] ? COL_TEXT_PRIMARY : COL_TEXT_SUBTLE);
            rRenderContext.DrawText(
                tools::Rectangle(Point(aCard.Left() + 8, aCard.Top() + static_cast<tools::Long>(aCard.GetHeight() * 0.57)),
                                 Size(aCard.GetWidth() - 16, 22)),
                aTitles[i], DrawTextFlags::Center | DrawTextFlags::VCenter | DrawTextFlags::EndEllipsis);

            rRenderContext.SetFont(MakeUiFont(12, WEIGHT_NORMAL));
            rRenderContext.SetTextColor(COL_TEXT_MUTED);
            rRenderContext.DrawText(
                tools::Rectangle(Point(aCard.Left() + 8, aCard.Top() + static_cast<tools::Long>(aCard.GetHeight() * 0.73)),
                                 Size(aCard.GetWidth() - 16, 20)),
                aSubs[i], DrawTextFlags::Center | DrawTextFlags::VCenter | DrawTextFlags::EndEllipsis);
        }
    }
};

// ---------------------------------------------------------------------------
// RecentHeaderWidget: "Recent Files" title + "View All >" link
// ---------------------------------------------------------------------------
class RecentHeaderWidget final : public weld::CustomWidgetController
{
private:
    BackingWindow& mrOwner;
    bool mbHoverLink = false;

    tools::Rectangle GetLinkRect() const
    {
        const Size aSz = GetOutputSizePixel();
        return tools::Rectangle(Point(std::max<tools::Long>(180, aSz.Width() - 115), 6), Size(88, aSz.Height() - 10));
    }

public:
    explicit RecentHeaderWidget(BackingWindow& rOwner)
        : mrOwner(rOwner)
    {
    }

    virtual void SetDrawingArea(weld::DrawingArea* pDrawingArea) override
    {
        weld::CustomWidgetController::SetDrawingArea(pDrawingArea);
        pDrawingArea->get_ref_device().SetBackground(Wallpaper(COL_SURFACE_WHITE));
        pDrawingArea->set_size_request(-1, 38);
    }

    virtual bool MouseMove(const MouseEvent& rMEvt) override
    {
        bool bHover = !rMEvt.IsLeaveWindow() && GetLinkRect().Contains(rMEvt.GetPosPixel());
        if (bHover != mbHoverLink)
        {
            mbHoverLink = bHover;
            Invalidate();
        }
        SetPointer(bHover ? PointerStyle::RefHand : PointerStyle::Arrow);
        return false;
    }

    virtual bool MouseButtonUp(const MouseEvent& rMEvt) override
    {
        if (rMEvt.IsLeft() && GetLinkRect().Contains(rMEvt.GetPosPixel()))
        {
            mrOwner.activateMode(StartCenterMode::Recent);
            return true;
        }
        return false;
    }

    virtual bool KeyInput(const KeyEvent& rKEvt) override
    {
        sal_uInt16 nCode = rKEvt.GetKeyCode().GetCode();
        if (nCode == KEY_RETURN || nCode == KEY_SPACE)
        {
            mrOwner.activateMode(StartCenterMode::Recent);
            return true;
        }
        return false;
    }

    virtual void Paint(vcl::RenderContext& rRenderContext, const tools::Rectangle&) override
    {
        auto popIt = rRenderContext.ScopedPush(vcl::PushFlags::ALL);
        const Size aSz = GetOutputSizePixel();
        FillSolidRect(rRenderContext, tools::Rectangle(Point(0, 0), aSz), COL_SURFACE_WHITE);

        rRenderContext.SetFont(MakeUiFont(17, WEIGHT_BOLD));
        rRenderContext.SetTextColor(COL_TEXT_PRIMARY);
        rRenderContext.DrawText(tools::Rectangle(Point(28, 4), Size(260, aSz.Height() - 6)), u"Recent Files"_ustr,
                                DrawTextFlags::Left | DrawTextFlags::VCenter);

        rRenderContext.SetFont(MakeUiFont(13, mbHoverLink ? WEIGHT_BOLD : WEIGHT_SEMIBOLD));
        rRenderContext.SetTextColor(COL_LINK_BLUE);
        rRenderContext.DrawText(GetLinkRect(), u"View All >"_ustr,
                                DrawTextFlags::Right | DrawTextFlags::VCenter);
    }
};

// ---------------------------------------------------------------------------
// HomeRecentListWidget: clean Name | Location | Last Modified recent files table
// ---------------------------------------------------------------------------
class HomeRecentListWidget final : public weld::CustomWidgetController
{
private:
    struct RecentRow
    {
        OUString sURL;
        OUString sName;
        OUString sExt;
        OUString sLocation;
        OUString sModified;
        bool bReadOnly = false;
    };

    BackingWindow& mrOwner;
    std::vector<RecentRow> maRows;
    int mnHoveredRow = -1;
    bool mbHoveredRemove = false;
    int mnFocusedRow = 0;

    static constexpr tools::Long gnHeaderH = 34;
    static constexpr tools::Long gnRowH = 44;

    tools::Rectangle GetCardRect() const
    {
        const Size aSz = GetOutputSizePixel();
        const tools::Long nW = std::max<tools::Long>(200, aSz.Width() - 56);
        const tools::Long nMaxH = std::max<tools::Long>(90, aSz.Height() - 18);
        const size_t nFit = static_cast<size_t>(std::max<tools::Long>(1, (nMaxH - gnHeaderH) / gnRowH));
        const size_t nVisibleRows = maRows.empty() ? 1 : std::min(maRows.size(), nFit);
        const tools::Long nH = maRows.empty() ? std::min<tools::Long>(nMaxH, 92)
                                              : std::min<tools::Long>(nMaxH, gnHeaderH + static_cast<tools::Long>(nVisibleRows) * gnRowH);
        return tools::Rectangle(Point(28, 4), Size(nW, nH));
    }

    size_t VisibleRowCount() const
    {
        if (maRows.empty())
            return 0;
        const tools::Rectangle aCard = GetCardRect();
        return static_cast<size_t>(std::max<tools::Long>(0, (aCard.GetHeight() - gnHeaderH) / gnRowH));
    }

    tools::Rectangle GetRowRect(size_t i) const
    {
        const tools::Rectangle aCard = GetCardRect();
        return tools::Rectangle(
            Point(aCard.Left() + 1, aCard.Top() + gnHeaderH + static_cast<tools::Long>(i) * gnRowH),
            Size(aCard.GetWidth() - 2, gnRowH));
    }

    tools::Rectangle GetRemoveRect(size_t i) const
    {
        const tools::Rectangle aRow = GetRowRect(i);
        return tools::Rectangle(Point(aRow.Right() - 34, aRow.Top() + 6), Size(28, aRow.GetHeight() - 12));
    }

public:
    explicit HomeRecentListWidget(BackingWindow& rOwner)
        : mrOwner(rOwner)
    {
    }

    void Reload()
    {
        maRows.clear();
        const std::vector<SvtHistoryOptions::HistoryItem> aList = SvtHistoryOptions::GetList(EHistoryType::PickList);
        for (const auto& item : aList)
        {
            INetURLObject aURLObj(item.sURL);
            OUString sName = aURLObj.GetLastName(INetURLObject::DecodeMechanism::WithCharset);
            if (sName.isEmpty())
                sName = item.sTitle;

            OUString sLoc;
            if (aURLObj.GetProtocol() == INetProtocol::File)
            {
                INetURLObject aParent(aURLObj);
                aParent.removeSegment();
                sLoc = aParent.getFSysPath(FSysStyle::Detect);
                if (sLoc.getLength() > 3 && (sLoc.endsWith(u"\\") || sLoc.endsWith(u"/")))
                    sLoc = sLoc.copy(0, sLoc.getLength() - 1);
            }
            else
            {
                sLoc = aURLObj.GetHost();
                if (sLoc.isEmpty())
                    sLoc = item.sURL;
            }

            RecentRow aRow;
            aRow.sURL = item.sURL;
            aRow.sName = sName;
            aRow.sExt = aURLObj.getExtension();
            aRow.sLocation = sLoc;
            aRow.sModified = FormatModifyTime(item.sURL);
            aRow.bReadOnly = item.isReadOnly;
            maRows.push_back(std::move(aRow));
            if (maRows.size() >= 8)
                break;
        }
        mnHoveredRow = -1;
        mbHoveredRemove = false;
        mnFocusedRow = 0;
        Invalidate();
    }

    virtual void SetDrawingArea(weld::DrawingArea* pDrawingArea) override
    {
        weld::CustomWidgetController::SetDrawingArea(pDrawingArea);
        pDrawingArea->get_ref_device().SetBackground(Wallpaper(COL_SURFACE_WHITE));
    }

    virtual OUString RequestHelp(tools::Rectangle&) override
    {
        if (mnHoveredRow >= 0 && static_cast<size_t>(mnHoveredRow) < maRows.size())
        {
            if (mbHoveredRemove)
                return u"Remove from Recent Files"_ustr;
            INetURLObject aURLObj(maRows[mnHoveredRow].sURL);
            if (aURLObj.GetProtocol() == INetProtocol::File)
                return aURLObj.getFSysPath(FSysStyle::Detect);
            return maRows[mnHoveredRow].sURL;
        }
        return OUString();
    }

    virtual bool MouseMove(const MouseEvent& rMEvt) override
    {
        int nHit = -1;
        bool bRem = false;
        if (!rMEvt.IsLeaveWindow())
        {
            const size_t nVis = std::min(maRows.size(), VisibleRowCount());
            for (size_t i = 0; i < nVis; ++i)
            {
                if (GetRowRect(i).Contains(rMEvt.GetPosPixel()))
                {
                    nHit = static_cast<int>(i);
                    bRem = GetRemoveRect(i).Contains(rMEvt.GetPosPixel());
                    break;
                }
            }
        }
        if (nHit != mnHoveredRow || bRem != mbHoveredRemove)
        {
            mnHoveredRow = nHit;
            mbHoveredRemove = bRem;
            Invalidate();
        }
        SetPointer(nHit >= 0 ? PointerStyle::RefHand : PointerStyle::Arrow);
        return false;
    }

    virtual bool MouseButtonUp(const MouseEvent& rMEvt) override
    {
        if (!rMEvt.IsLeft())
            return false;
        const size_t nVis = std::min(maRows.size(), VisibleRowCount());
        for (size_t i = 0; i < nVis; ++i)
        {
            if (GetRowRect(i).Contains(rMEvt.GetPosPixel()))
            {
                if (GetRemoveRect(i).Contains(rMEvt.GetPosPixel()))
                    mrOwner.removeRecentURL(maRows[i].sURL);
                else
                    mrOwner.openRecentURL(maRows[i].sURL, maRows[i].bReadOnly);
                return true;
            }
        }
        return false;
    }

    virtual bool KeyInput(const KeyEvent& rKEvt) override
    {
        if (maRows.empty())
            return false;
        const int nCount = static_cast<int>(std::min(maRows.size(), VisibleRowCount()));
        if (nCount <= 0)
            return false;
        sal_uInt16 nCode = rKEvt.GetKeyCode().GetCode();
        if (nCode == KEY_DOWN)
        {
            mnFocusedRow = (mnFocusedRow + 1) % nCount;
            Invalidate();
            return true;
        }
        if (nCode == KEY_UP)
        {
            mnFocusedRow = (mnFocusedRow + nCount - 1) % nCount;
            Invalidate();
            return true;
        }
        if ((nCode == KEY_RETURN || nCode == KEY_SPACE) && mnFocusedRow >= 0 && mnFocusedRow < nCount)
        {
            mrOwner.openRecentURL(maRows[mnFocusedRow].sURL, maRows[mnFocusedRow].bReadOnly);
            return true;
        }
        return false;
    }

    virtual void Paint(vcl::RenderContext& rRenderContext, const tools::Rectangle&) override
    {
        auto popIt = rRenderContext.ScopedPush(vcl::PushFlags::ALL);
        rRenderContext.SetAntialiasing(AntialiasingFlags::Enable);

        const Size aSz = GetOutputSizePixel();
        FillSolidRect(rRenderContext, tools::Rectangle(Point(0, 0), aSz), COL_SURFACE_WHITE);

        const tools::Rectangle aCard = GetCardRect();
        DrawRoundedBox(rRenderContext, aCard, 8.0, COL_SURFACE_WHITE, COL_BORDER, 1.2);

        // Header strip
        tools::Rectangle aHdr(Point(aCard.Left() + 1, aCard.Top() + 1), Size(aCard.GetWidth() - 2, gnHeaderH - 1));
        DrawRoundedBox(rRenderContext, aHdr, 6.0, COL_SURFACE_SUBTLE, COL_TRANSPARENT, 0.0);
        DrawStrokePoly(rRenderContext, { { static_cast<double>(aCard.Left() + 1), static_cast<double>(aCard.Top() + gnHeaderH) },
                                         { static_cast<double>(aCard.Right() - 1), static_cast<double>(aCard.Top() + gnHeaderH) } },
                       false, COL_BORDER, 1.0);

        const tools::Long nW = aCard.GetWidth();
        const tools::Long nColNameX = aCard.Left() + 18;
        const tools::Long nColLocX = aCard.Left() + static_cast<tools::Long>(nW * 0.36);
        const tools::Long nColModX = aCard.Left() + static_cast<tools::Long>(nW * 0.72);

        rRenderContext.SetFont(MakeUiFont(12, WEIGHT_SEMIBOLD));
        rRenderContext.SetTextColor(COL_TEXT_MUTED);
        rRenderContext.DrawText(tools::Rectangle(Point(nColNameX, aHdr.Top()), Size(nColLocX - nColNameX - 8, gnHeaderH)),
                                u"Name"_ustr, DrawTextFlags::Left | DrawTextFlags::VCenter);
        rRenderContext.DrawText(tools::Rectangle(Point(nColLocX, aHdr.Top()), Size(nColModX - nColLocX - 8, gnHeaderH)),
                                u"Location"_ustr, DrawTextFlags::Left | DrawTextFlags::VCenter);
        rRenderContext.DrawText(tools::Rectangle(Point(nColModX, aHdr.Top()), Size(aCard.Right() - nColModX - 38, gnHeaderH)),
                                u"Last Modified"_ustr, DrawTextFlags::Left | DrawTextFlags::VCenter);

        if (maRows.empty())
        {
            rRenderContext.SetFont(MakeUiFont(13, WEIGHT_NORMAL));
            rRenderContext.SetTextColor(COL_TEXT_SUBTLE);
            tools::Rectangle aEmpty(Point(aCard.Left() + 16, aCard.Top() + gnHeaderH), Size(aCard.GetWidth() - 32, aCard.GetHeight() - gnHeaderH));
            rRenderContext.DrawText(aEmpty, u"No recent files yet — create a document above or click Open to browse."_ustr,
                                    DrawTextFlags::Center | DrawTextFlags::VCenter | DrawTextFlags::EndEllipsis);
            return;
        }

        const size_t nVis = std::min(maRows.size(), VisibleRowCount());
        for (size_t i = 0; i < nVis; ++i)
        {
            const RecentRow& rRow = maRows[i];
            const tools::Rectangle aRow = GetRowRect(i);
            const bool bHover = (static_cast<int>(i) == mnHoveredRow);
            const bool bFocus = HasFocus() && (static_cast<int>(i) == mnFocusedRow);

            if (bHover || bFocus)
                FillSolidRect(rRenderContext, aRow, COL_SURFACE_HOVER);

            if (i + 1 < nVis)
                DrawStrokePoly(rRenderContext, { { static_cast<double>(aRow.Left() + 12), static_cast<double>(aRow.Bottom()) },
                                                 { static_cast<double>(aRow.Right() - 12), static_cast<double>(aRow.Bottom()) } },
                               false, Color(0xEC, 0xF0, 0xF5), 1.0);

            const int nIconY = static_cast<int>(aRow.Top() + (gnRowH - 20) / 2);
            DrawMiniDocIcon(rRenderContext, nColNameX, nIconY, 20, rRow.sExt);

            const tools::Long nNameMaxW = std::max<tools::Long>(40, nColLocX - nColNameX - 42);
            const OUString sDispName = rRenderContext.GetEllipsisString(rRow.sName, nNameMaxW, DrawTextFlags::EndEllipsis);
            rRenderContext.SetFont(MakeUiFont(13, WEIGHT_SEMIBOLD));
            rRenderContext.SetTextColor(COL_TEXT_PRIMARY);
            rRenderContext.DrawText(
                tools::Rectangle(Point(nColNameX + 28, aRow.Top()), Size(nNameMaxW, gnRowH)),
                sDispName, DrawTextFlags::Left | DrawTextFlags::VCenter | DrawTextFlags::Clip);

            DrawStrokePoly(rRenderContext, {
                { static_cast<double>(nColLocX), aRow.Top() + 17.0 },
                { static_cast<double>(nColLocX + 4.5), aRow.Top() + 17.0 },
                { static_cast<double>(nColLocX + 6.0), aRow.Top() + 19.0 },
                { static_cast<double>(nColLocX + 13.0), aRow.Top() + 19.0 },
                { static_cast<double>(nColLocX + 13.0), aRow.Top() + 27.0 },
                { static_cast<double>(nColLocX), aRow.Top() + 27.0 }
            }, true, COL_TEXT_SUBTLE, 1.3);

            const tools::Long nLocMaxW = std::max<tools::Long>(40, nColModX - nColLocX - 38);
            const tools::Long nModMaxW = std::max<tools::Long>(40, aCard.Right() - nColModX - 44);
            rRenderContext.SetFont(MakeUiFont(12, WEIGHT_NORMAL));
            rRenderContext.SetTextColor(COL_TEXT_MUTED);
            const OUString sDispLoc = rRenderContext.GetEllipsisString(rRow.sLocation, nLocMaxW, DrawTextFlags::PathEllipsis);
            rRenderContext.DrawText(
                tools::Rectangle(Point(nColLocX + 20, aRow.Top()), Size(nLocMaxW, gnRowH)),
                sDispLoc, DrawTextFlags::Left | DrawTextFlags::VCenter | DrawTextFlags::Clip);

            const OUString sDispMod = rRenderContext.GetEllipsisString(rRow.sModified, nModMaxW, DrawTextFlags::EndEllipsis);
            rRenderContext.DrawText(
                tools::Rectangle(Point(nColModX, aRow.Top()), Size(nModMaxW, gnRowH)),
                sDispMod, DrawTextFlags::Left | DrawTextFlags::VCenter | DrawTextFlags::Clip);

            const tools::Rectangle aRemRect = GetRemoveRect(i);
            const Color aDotCol = (bHover && mbHoveredRemove) ? COL_RED_BRAND : COL_TEXT_SUBTLE;
            const tools::Long nDotX = (aRemRect.Left() + aRemRect.Right()) / 2;
            const tools::Long nDotY = (aRemRect.Top() + aRemRect.Bottom()) / 2;
            rRenderContext.SetLineColor();
            rRenderContext.SetFillColor(aDotCol);
            for (int d = -4; d <= 4; d += 4)
                rRenderContext.DrawRect(tools::Rectangle(Point(nDotX - 1, nDotY + d - 1), Size(3, 3)));
        }
    }
};

// ---------------------------------------------------------------------------
// FooterWidget: "OfficeStudio AI v1.0.0 | Built on the LibreOffice engine"
// ---------------------------------------------------------------------------
class FooterWidget final : public weld::CustomWidgetController
{
private:
    BackingWindow& mrOwner;
    bool mbHovered = false;

public:
    explicit FooterWidget(BackingWindow& rOwner)
        : mrOwner(rOwner)
    {
    }

    virtual void SetDrawingArea(weld::DrawingArea* pDrawingArea) override
    {
        weld::CustomWidgetController::SetDrawingArea(pDrawingArea);
        pDrawingArea->get_ref_device().SetBackground(Wallpaper(COL_FOOTER_BG));
        pDrawingArea->set_size_request(-1, 36);
        SetPointer(PointerStyle::RefHand);
    }

    virtual bool MouseMove(const MouseEvent& rMEvt) override
    {
        bool bHover = !rMEvt.IsLeaveWindow();
        if (bHover != mbHovered)
        {
            mbHovered = bHover;
            Invalidate();
        }
        return false;
    }

    virtual bool MouseButtonUp(const MouseEvent& rMEvt) override
    {
        if (rMEvt.IsLeft())
            mrOwner.triggerAbout();
        return true;
    }

    virtual bool KeyInput(const KeyEvent& rKEvt) override
    {
        sal_uInt16 nCode = rKEvt.GetKeyCode().GetCode();
        if (nCode == KEY_RETURN || nCode == KEY_SPACE)
        {
            mrOwner.triggerAbout();
            return true;
        }
        return false;
    }

    virtual void Paint(vcl::RenderContext& rRenderContext, const tools::Rectangle&) override
    {
        auto popIt = rRenderContext.ScopedPush(vcl::PushFlags::ALL);
        rRenderContext.SetAntialiasing(AntialiasingFlags::Enable);
        const Size aSz = GetOutputSizePixel();

        FillSolidRect(rRenderContext, tools::Rectangle(Point(0, 0), aSz),
                      mbHovered ? Color(0xEA, 0xEF, 0xF5) : COL_FOOTER_BG);
        DrawStrokePoly(rRenderContext, { { 0.0, 0.0 }, { static_cast<double>(aSz.Width()), 0.0 } },
                       false, COL_BORDER, 1.0);

        // Info (i) badge
        const tools::Long nCy = aSz.Height() / 2;
        DrawCircleStroke(rRenderContext, 26.0, static_cast<double>(nCy), 7.5, COL_TEXT_MUTED, 1.4);
        FillSolidRect(rRenderContext, tools::Rectangle(Point(25, nCy - 4), Size(2, 2)), COL_TEXT_MUTED);
        FillSolidRect(rRenderContext, tools::Rectangle(Point(25, nCy - 1), Size(2, 6)), COL_TEXT_MUTED);

        const OUString sVer = u"OfficeStudio AI v1.0.0"_ustr;
        rRenderContext.SetFont(MakeUiFont(12, WEIGHT_BOLD));
        rRenderContext.SetTextColor(COL_TEXT_PRIMARY);
        const tools::Long nVerW = rRenderContext.GetTextWidth(sVer);
        rRenderContext.DrawText(tools::Rectangle(Point(42, 0), Size(nVerW + 8, aSz.Height())), sVer,
                                DrawTextFlags::Left | DrawTextFlags::VCenter);

        const tools::Long nPipeX = 42 + nVerW + 12;
        DrawStrokePoly(rRenderContext, { { static_cast<double>(nPipeX), 10.0 }, { static_cast<double>(nPipeX), static_cast<double>(aSz.Height() - 10) } },
                       false, Color(0xCB, 0xD5, 0xE1), 1.0);

        rRenderContext.SetFont(MakeUiFont(12, WEIGHT_NORMAL));
        rRenderContext.SetTextColor(COL_TEXT_MUTED);
        rRenderContext.DrawText(tools::Rectangle(Point(nPipeX + 12, 0), Size(std::max<tools::Long>(100, aSz.Width() - nPipeX - 24), aSz.Height())),
                                u"Built on the LibreOffice engine"_ustr,
                                DrawTextFlags::Left | DrawTextFlags::VCenter | DrawTextFlags::EndEllipsis);
    }
};

// ---------------------------------------------------------------------------
// BackingWindow
// ---------------------------------------------------------------------------
BackingWindow::BackingWindow(vcl::Window* pParent)
    : InterimItemWindow(pParent, u"sfx/ui/startcenter.ui"_ustr, u"StartCenter"_ustr, false)
    , mxOpenButton(m_xBuilder->weld_button(u"open_all"_ustr))
    , mxRemoteButton(m_xBuilder->weld_button(u"open_remote"_ustr))
    , mxRecentButton(m_xBuilder->weld_toggle_button(u"open_recent"_ustr))
    , mxTemplateButton(m_xBuilder->weld_toggle_button(u"templates_all"_ustr))
    , mxCreateLabel(m_xBuilder->weld_label(u"create_label"_ustr))
    , mxAltHelpLabel(m_xBuilder->weld_label(u"althelplabel"_ustr))
    , mxFilter(m_xBuilder->weld_combo_box(u"cbFilter"_ustr))
    , mxActions(m_xBuilder->weld_menu_button(u"mbActions"_ustr))
    , mxWriterAllButton(m_xBuilder->weld_button(u"writer_all"_ustr))
    , mxCalcAllButton(m_xBuilder->weld_button(u"calc_all"_ustr))
    , mxImpressAllButton(m_xBuilder->weld_button(u"impress_all"_ustr))
    , mxDrawAllButton(m_xBuilder->weld_button(u"draw_all"_ustr))
    , mxDBAllButton(m_xBuilder->weld_button(u"database_all"_ustr))
    , mxMathAllButton(m_xBuilder->weld_button(u"math_all"_ustr))
    , mxBrandImage(new BrandImage(*this))
    , mxBrandImageWeld(new weld::CustomWeld(*m_xBuilder, u"daBrand"_ustr, *mxBrandImage))
    , mxSidebarNav(new SidebarNavWidget(*this))
    , mxSidebarNavWeld(new weld::CustomWeld(*m_xBuilder, u"daSidebarNav"_ustr, *mxSidebarNav))
    , mxCreateCards(new CreateCardsWidget(*this))
    , mxCreateCardsWeld(new weld::CustomWeld(*m_xBuilder, u"daCreateSection"_ustr, *mxCreateCards))
    , mxRecentHeader(new RecentHeaderWidget(*this))
    , mxRecentHeaderWeld(new weld::CustomWeld(*m_xBuilder, u"daRecentHeader"_ustr, *mxRecentHeader))
    , mxHomeRecent(new HomeRecentListWidget(*this))
    , mxHomeRecentWeld(new weld::CustomWeld(*m_xBuilder, u"daHomeRecent"_ustr, *mxHomeRecent))
    , mxFooter(new FooterWidget(*this))
    , mxFooterWeld(new weld::CustomWeld(*m_xBuilder, u"daFooter"_ustr, *mxFooter))
    , mxHelpButton(m_xBuilder->weld_button(u"help"_ustr))
    , mxExtensionsButton(m_xBuilder->weld_button(u"extensions"_ustr))
    , mxDonateButton(m_xBuilder->weld_button(u"donate"_ustr))
    , mxAllButtonsBox(m_xBuilder->weld_container(u"all_buttons_box"_ustr))
    , mxButtonsBox(m_xBuilder->weld_container(u"buttons_box"_ustr))
    , mxSmallButtonsBox(m_xBuilder->weld_container(u"small_buttons_box"_ustr))
    , mxRightBox(m_xBuilder->weld_container(u"box2"_ustr))
    , mxHomeSection(m_xBuilder->weld_container(u"home_section"_ustr))
    , mxActionsGrid(m_xBuilder->weld_container(u"actions"_ustr))
    , mxScrollRecent(m_xBuilder->weld_widget(u"scrollrecent"_ustr))
    , mxScrollLocal(m_xBuilder->weld_widget(u"scrolllocal"_ustr))
    , mxAllRecentThumbnails(new sfx2::RecentDocsView(m_xBuilder->weld_scrolled_window(u"scrollrecent"_ustr, true)))
    , mxAllRecentThumbnailsWin(new weld::CustomWeld(*m_xBuilder, u"all_recent"_ustr, *mxAllRecentThumbnails))
    , mxLocalView(new TemplateDefaultView(m_xBuilder->weld_scrolled_window(u"scrolllocal"_ustr, true),
                                          m_xBuilder->weld_menu(u"localmenu"_ustr)))
    , mxLocalViewWin(new weld::CustomWeld(*m_xBuilder, u"local_view"_ustr, *mxLocalView))
    , mxDonationLeft(m_xBuilder->weld_image(u"imgDonationLeft"_ustr))
    , mxDonationRight(m_xBuilder->weld_image(u"imgDonationRight"_ustr))
    , mxDonationLink(m_xBuilder->weld_link_button(u"btnDonateLink"_ustr))
    , mxDonationBox(m_xBuilder->weld_container(u"gdDonation"_ustr))
    , mxDonationBoxTitle(m_xBuilder->weld_label(u"lbDonateTitle"_ustr))
    , mxDonationBoxText(m_xBuilder->weld_label(u"lbDonateText"_ustr))
    , mbLocalViewInitialized(false)
    , meMode(StartCenterMode::Home)
    , mbInitControls(false)
{
    SetPaintTransparent(false);
    mxDonationBox->set_visible(false);

    auto nHeight = mxFilter->get_preferred_size().getHeight();
    mxActions->set_size_request(nHeight, nHeight);

    mxHelpButton->set_label(mxAltHelpLabel->get_label());
    mxHelpButton->connect_clicked(LINK(this, BackingWindow, ClickHelpHdl));

    mxDropTarget = mxAllRecentThumbnails->GetDropTarget();
    mxHomeDropTarget = mxHomeRecent->GetDropTarget();

    try
    {
        mxContext.set(::comphelper::getProcessComponentContext(), uno::UNO_SET_THROW);
    }
    catch (const Exception&)
    {
        TOOLS_WARN_EXCEPTION("fwk", "BackingWindow");
    }

    SetStyle(GetStyle() | WB_DIALOGCONTROL);

    Reference<XDesktop2> xDesktop = Desktop::create(comphelper::getProcessComponentContext());
    mxDesktopDispatchProvider = xDesktop;
}

void BackingWindow::initDonationBanner()
{
    mxDonationBox->set_visible(false);
}

IMPL_STATIC_LINK_NOARG(BackingWindow, OnDonateLinkClick, weld::LinkButton&, bool)
{
    SfxDispatcher& rDispatcher = *SfxGetpApp()->GetDispatcher_Impl();
    rDispatcher.Execute(SID_DONATION, SfxCallMode::ASYNCHRON);
    return true;
}

IMPL_LINK(BackingWindow, ClickHelpHdl, weld::Button&, rButton, void)
{
    if (Help* pHelp = Application::GetHelp())
        pHelp->Start(m_xContainer->get_help_id(), &rButton);
}

BackingWindow::~BackingWindow()
{
    disposeOnce();
}

void BackingWindow::dispose()
{
    if (mxDropTargetListener.is())
    {
        if (mxDropTarget.is())
        {
            mxDropTarget->removeDropTargetListener(mxDropTargetListener);
            mxDropTarget->setActive(false);
        }
        if (mxHomeDropTarget.is())
        {
            mxHomeDropTarget->removeDropTargetListener(mxDropTargetListener);
            mxHomeDropTarget->setActive(false);
        }
        mxDropTargetListener.clear();
    }
    mxDropTarget.clear();
    mxHomeDropTarget.clear();

    mxOpenButton.reset();
    mxRemoteButton.reset();
    mxRecentButton.reset();
    mxTemplateButton.reset();
    mxCreateLabel.reset();
    mxAltHelpLabel.reset();
    mxFilter.reset();
    mxActions.reset();
    mxWriterAllButton.reset();
    mxCalcAllButton.reset();
    mxImpressAllButton.reset();
    mxDrawAllButton.reset();
    mxDBAllButton.reset();
    mxMathAllButton.reset();
    mxFooterWeld.reset();
    mxFooter.reset();
    mxHomeRecentWeld.reset();
    mxHomeRecent.reset();
    mxRecentHeaderWeld.reset();
    mxRecentHeader.reset();
    mxCreateCardsWeld.reset();
    mxCreateCards.reset();
    mxSidebarNavWeld.reset();
    mxSidebarNav.reset();
    mxBrandImageWeld.reset();
    mxBrandImage.reset();
    mxHelpButton.reset();
    mxDonateButton.reset();
    mxExtensionsButton.reset();
    mxHomeSection.reset();
    mxActionsGrid.reset();
    mxScrollRecent.reset();
    mxScrollLocal.reset();
    mxRightBox.reset();
    mxAllButtonsBox.reset();
    mxButtonsBox.reset();
    mxSmallButtonsBox.reset();
    mxAllRecentThumbnailsWin.reset();
    mxAllRecentThumbnails.reset();
    mxLocalViewWin.reset();
    mxLocalView.reset();
    InterimItemWindow::dispose();
}

void BackingWindow::initControls()
{
    if (mbInitControls)
        return;

    mbInitControls = true;

    SvtModuleOptions aModuleOptions;
    if (aModuleOptions.IsWriterInstalled())
        mxAllRecentThumbnails->mnFileTypes |= sfx2::ApplicationType::TYPE_WRITER;
    if (aModuleOptions.IsCalcInstalled())
        mxAllRecentThumbnails->mnFileTypes |= sfx2::ApplicationType::TYPE_CALC;
    if (aModuleOptions.IsImpressInstalled())
        mxAllRecentThumbnails->mnFileTypes |= sfx2::ApplicationType::TYPE_IMPRESS;
    if (aModuleOptions.IsDrawInstalled())
        mxAllRecentThumbnails->mnFileTypes |= sfx2::ApplicationType::TYPE_DRAW;
    if (aModuleOptions.IsDataBaseInstalled())
        mxAllRecentThumbnails->mnFileTypes |= sfx2::ApplicationType::TYPE_DATABASE;
    if (aModuleOptions.IsMathInstalled())
        mxAllRecentThumbnails->mnFileTypes |= sfx2::ApplicationType::TYPE_MATH;

    mxAllRecentThumbnails->mnFileTypes |= sfx2::ApplicationType::TYPE_OTHER;
    mxAllRecentThumbnails->Reload();
    mxAllRecentThumbnails->ShowTooltips(true);
    mxHomeRecent->Reload();

    mxLocalView->setCreateContextMenuHdl(LINK(this, BackingWindow, CreateContextMenuHdl));
    mxLocalView->setOpenTemplateHdl(LINK(this, BackingWindow, OpenTemplateHdl));
    mxLocalView->setEditTemplateHdl(LINK(this, BackingWindow, EditTemplateHdl));
    mxLocalView->ShowTooltips(true);

    checkInstalledModules();

    mxExtensionsButton->connect_clicked(LINK(this, BackingWindow, ExtLinkClickHdl));
    mxDonateButton->connect_clicked(LINK(this, BackingWindow, ExtLinkClickHdl));
    mxOpenButton->connect_clicked(LINK(this, BackingWindow, ClickHdl));

    SvtCommandOptions aCmdOptions;
    const bool bRemoteDisabled = SvtCommandOptions().HasEntriesDisabled() && aCmdOptions.LookupDisabled(u"OpenRemote"_ustr);
    mxSidebarNav->SetRemoteEnabled(!bRemoteDisabled);
    if (bRemoteDisabled)
        mxRemoteButton->set_visible(false);
    else
        mxRemoteButton->connect_clicked(LINK(this, BackingWindow, ClickHdl));

    mxWriterAllButton->connect_clicked(LINK(this, BackingWindow, ClickHdl));
    mxDrawAllButton->connect_clicked(LINK(this, BackingWindow, ClickHdl));
    mxCalcAllButton->connect_clicked(LINK(this, BackingWindow, ClickHdl));
    mxDBAllButton->connect_clicked(LINK(this, BackingWindow, ClickHdl));
    mxImpressAllButton->connect_clicked(LINK(this, BackingWindow, ClickHdl));
    mxMathAllButton->connect_clicked(LINK(this, BackingWindow, ClickHdl));

    mxRecentButton->connect_toggled(LINK(this, BackingWindow, ToggleHdl));
    mxTemplateButton->connect_toggled(LINK(this, BackingWindow, ToggleHdl));

    mxFilter->connect_changed(LINK(this, BackingWindow, FilterHdl));
    mxActions->connect_selected(LINK(this, BackingWindow, MenuSelectHdl));

    activateMode(StartCenterMode::Home);
    ApplyStyleSettings();
}

void BackingWindow::activateMode(StartCenterMode eMode)
{
    meMode = eMode;
    mxSidebarNav->meActiveMode = eMode;
    mxSidebarNav->Invalidate();

    if (eMode == StartCenterMode::Home)
    {
        mxHomeSection->set_visible(true);
        mxHomeRecent->Show();
        mxHomeRecent->Reload();
        mxActionsGrid->set_visible(false);
        mxScrollRecent->set_visible(false);
        mxAllRecentThumbnails->Hide();
        mxScrollLocal->set_visible(false);
        mxLocalView->Hide();
    }
    else if (eMode == StartCenterMode::Recent)
    {
        mxHomeSection->set_visible(false);
        mxHomeRecent->Hide();
        mxScrollLocal->set_visible(false);
        mxLocalView->Hide();
        mxActionsGrid->set_visible(true);
        mxActions->show();
        mxScrollRecent->set_visible(true);
        mxAllRecentThumbnails->Show();
        mxAllRecentThumbnails->Reload();
        mxAllRecentThumbnails->GrabFocus();
        applyFilter();
    }
    else // Templates
    {
        mxHomeSection->set_visible(false);
        mxHomeRecent->Hide();
        mxScrollRecent->set_visible(false);
        mxAllRecentThumbnails->Hide();
        mxActionsGrid->set_visible(true);
        mxActions->hide();
        mxScrollLocal->set_visible(true);
        initializeLocalView();
        mxLocalView->Show();
        mxLocalView->reload();
        mxLocalView->GrabFocus();
        applyFilter();
    }
}

void BackingWindow::triggerOpen()
{
    Reference<XDispatchProvider> xFrame(mxFrame, UNO_QUERY);
    dispatchURL(u".uno:Open"_ustr, OUString(), xFrame, { comphelper::makePropertyValue(u"Referer"_ustr, u"private:user"_ustr) });
}

void BackingWindow::triggerOpenRemote()
{
    Reference<XDispatchProvider> xFrame(mxFrame, UNO_QUERY);
    dispatchURL(u".uno:OpenRemote"_ustr, OUString(), xFrame, {});
}

void BackingWindow::triggerHelp()
{
    if (Help* pHelp = Application::GetHelp())
        pHelp->Start(m_xContainer->get_help_id(), mxSidebarNav->GetDrawingArea());
}

void BackingWindow::triggerAbout()
{
    Reference<XDispatchProvider> xFrame(mxFrame, UNO_QUERY);
    dispatchURL(u".uno:About"_ustr, OUString(), xFrame, {});
}

void BackingWindow::triggerCreateApp(int nIndex)
{
    if (nIndex == 0)
        dispatchURL(u"private:factory/swriter"_ustr);
    else if (nIndex == 1)
        dispatchURL(u"private:factory/scalc"_ustr);
    else if (nIndex == 2)
        dispatchURL(u"private:factory/simpress?slot=6686"_ustr);
    else if (nIndex == 3)
        dispatchURL(u"private:factory/sdraw"_ustr);
}

void BackingWindow::openRecentURL(const OUString& rURL, bool bReadOnly)
{
    Sequence<PropertyValue> aArgsList = {
        comphelper::makePropertyValue(u"Referer"_ustr, u"private:user"_ustr),
        comphelper::makePropertyValue(u"AsTemplate"_ustr, false)
    };
    if (bReadOnly)
    {
        aArgsList.realloc(aArgsList.size() + 1);
        aArgsList.getArray()[aArgsList.size() - 1] = comphelper::makePropertyValue(u"ReadOnly"_ustr, true);
    }
    Reference<XDispatchProvider> xFrame(mxFrame, UNO_QUERY);
    dispatchURL(rURL, u"_default"_ustr, xFrame, aArgsList);
}

void BackingWindow::removeRecentURL(const OUString& rURL)
{
    SvtHistoryOptions::DeleteItem(EHistoryType::PickList, rURL);
    mxHomeRecent->Reload();
    mxAllRecentThumbnails->Reload();
}

void BackingWindow::DataChanged(const DataChangedEvent& rDCEvt)
{
    if ((rDCEvt.GetType() != DataChangedEventType::SETTINGS)
        || !(rDCEvt.GetFlags() & AllSettingsFlags::STYLE))
    {
        InterimItemWindow::DataChanged(rDCEvt);
        return;
    }

    ApplyStyleSettings();
    Invalidate();
}

template <typename WidgetClass>
void BackingWindow::setLargerFont(WidgetClass&, const vcl::Font&)
{
}

void BackingWindow::ApplyStyleSettings()
{
    mxAllButtonsBox->set_background(COL_NAV_BG);
    mxRightBox->set_background(COL_SURFACE_WHITE);
    SetBackground(COL_SURFACE_WHITE);

    set_width_request(920);
    set_height_request(620);

    mxBrandImage->Invalidate();
    mxSidebarNav->Invalidate();
    mxCreateCards->Invalidate();
    mxRecentHeader->Invalidate();
    mxHomeRecent->Invalidate();
    mxFooter->Invalidate();
}

void BackingWindow::initializeLocalView()
{
    if (!mbLocalViewInitialized)
    {
        mbLocalViewInitialized = true;
        mxLocalView->Populate();
        mxLocalView->filterItems(ViewFilter_Application(FILTER_APPLICATION::NONE));
        mxLocalView->showAllTemplates();
    }
}

void BackingWindow::checkInstalledModules()
{
    if (officecfg::Office::Common::Misc::ViewerAppMode::get())
    {
        mxCreateCards->SetModuleStates(false, false, false, false);
        return;
    }

    SvtModuleOptions aModuleOpt;
    mxCreateCards->SetModuleStates(
        aModuleOpt.IsWriterInstalled(),
        aModuleOpt.IsCalcInstalled(),
        aModuleOpt.IsImpressInstalled(),
        aModuleOpt.IsDrawInstalled());
}

bool BackingWindow::PreNotify(NotifyEvent& rNEvt)
{
    if (rNEvt.GetType() == NotifyEventType::KEYINPUT)
    {
        const KeyEvent* pEvt = rNEvt.GetKeyEvent();
        const vcl::KeyCode& rKeyCode(pEvt->GetKeyCode());

        bool bThumbnailHasFocus = mxAllRecentThumbnails->HasFocus() || mxLocalView->HasFocus() || mxHomeRecent->HasFocus();

        if (rKeyCode.GetCode() == KEY_F6)
        {
            if (rKeyCode.IsShift())
            {
                if (bThumbnailHasFocus)
                {
                    mxSidebarNav->GrabFocus();
                    return true;
                }
            }
            else if (rKeyCode.IsMod1())
            {
                if (meMode == StartCenterMode::Home)
                {
                    mxHomeRecent->GrabFocus();
                    return true;
                }
                if (mxAllRecentThumbnails->IsVisible())
                {
                    mxAllRecentThumbnails->GrabFocus();
                    return true;
                }
                if (mxLocalView->IsVisible())
                {
                    mxLocalView->GrabFocus();
                    return true;
                }
            }
            else if (!bThumbnailHasFocus)
            {
                if (meMode == StartCenterMode::Home)
                {
                    mxHomeRecent->GrabFocus();
                    return true;
                }
                if (mxAllRecentThumbnails->IsVisible())
                {
                    mxAllRecentThumbnails->GrabFocus();
                    return true;
                }
                if (mxLocalView->IsVisible())
                {
                    mxLocalView->GrabFocus();
                    return true;
                }
            }
        }

        if (!mpAccExec)
        {
            mpAccExec = svt::AcceleratorExecute::createAcceleratorHelper();
            mpAccExec->init(comphelper::getProcessComponentContext(), mxFrame);
        }

        const OUString aCommand = mpAccExec->findCommand(svt::AcceleratorExecute::st_VCLKey2AWTKey(rKeyCode));
        if ((aCommand != "vnd.sun.star.findbar:FocusToFindbar") && pEvt && mpAccExec->execute(rKeyCode))
            return true;
    }
    return InterimItemWindow::PreNotify(rNEvt);
}

void BackingWindow::GetFocus()
{
    GetFocusFlags nFlags = GetParent()->GetGetFocusFlags();
    if (nFlags & GetFocusFlags::F6)
    {
        if (nFlags & GetFocusFlags::Forward)
        {
            mxSidebarNav->GrabFocus();
            return;
        }
        if (meMode == StartCenterMode::Home)
            mxHomeRecent->GrabFocus();
        else if (mxAllRecentThumbnails->IsVisible())
            mxAllRecentThumbnails->GrabFocus();
        else if (mxLocalView->IsVisible())
            mxLocalView->GrabFocus();
        return;
    }
    InterimItemWindow::GetFocus();
}

void BackingWindow::setOwningFrame(const css::uno::Reference<css::frame::XFrame>& xFrame)
{
    mxFrame = xFrame;
    if (!mbInitControls)
        initControls();

    mxDropTargetListener.set(new OpenFileDropTargetListener(mxContext, mxFrame));

    if (mxDropTarget.is())
    {
        mxDropTarget->addDropTargetListener(mxDropTargetListener);
        mxDropTarget->setActive(true);
    }
    if (mxHomeDropTarget.is())
    {
        mxHomeDropTarget->addDropTargetListener(mxDropTargetListener);
        mxHomeDropTarget->setActive(true);
    }

    css::uno::Reference<XFramesSupplier> xFramesSupplier(mxDesktopDispatchProvider, UNO_QUERY);
    if (xFramesSupplier)
        xFramesSupplier->setActiveFrame(mxFrame);
}

IMPL_STATIC_LINK_NOARG(BackingWindow, ExtLinkClickHdl, weld::Button&, void)
{
    try
    {
        OUString sURL;
        if (officecfg::Office::Common::Misc::ShowDonation::get())
            sURL = officecfg::Office::Common::Menus::DonationURL::get() +
                "?BCP47=" + LanguageTag(utl::ConfigManager::getUILocale()).getBcp47() +
                "&LOlang=" + LanguageTag(utl::ConfigManager::getUILocale()).getLanguage();
        else
            sURL = officecfg::Office::Common::Menus::ExtensionsURL::get() +
                "?LOvers=" + utl::ConfigManager::getProductVersion() +
                "&LOlocale=" + LanguageTag(utl::ConfigManager::getUILocale()).getBcp47();

        Reference<css::system::XSystemShellExecute> const xSystemShellExecute(
            css::system::SystemShellExecute::create(::comphelper::getProcessComponentContext()));
        xSystemShellExecute->execute(sURL, OUString(), css::system::SystemShellExecuteFlags::URIS_ONLY);
    }
    catch (const Exception&)
    {
    }
}

void BackingWindow::applyFilter()
{
    const int nFilter = mxFilter->get_active();
    if (mxLocalView->IsVisible())
    {
        FILTER_APPLICATION aFilter = static_cast<FILTER_APPLICATION>(nFilter);
        mxLocalView->filterItems(ViewFilter_Application(aFilter));
    }
    else
    {
        sfx2::ApplicationType aFilter;
        if (nFilter == 0)
            aFilter = sfx2::ApplicationType::TYPE_NONE;
        else
            aFilter = static_cast<sfx2::ApplicationType>(1 << (nFilter - 1));
        mxAllRecentThumbnails->setFilter(aFilter);
    }
}

IMPL_LINK_NOARG(BackingWindow, FilterHdl, weld::ComboBox&, void)
{
    applyFilter();
}

IMPL_LINK(BackingWindow, ToggleHdl, weld::Toggleable&, rButton, void)
{
    if (&rButton == mxRecentButton.get())
        activateMode(StartCenterMode::Recent);
    else
        activateMode(StartCenterMode::Templates);
}

IMPL_LINK(BackingWindow, ClickHdl, weld::Button&, rButton, void)
{
    if (&rButton == mxWriterAllButton.get())
        triggerCreateApp(0);
    else if (&rButton == mxCalcAllButton.get())
        triggerCreateApp(1);
    else if (&rButton == mxImpressAllButton.get())
        triggerCreateApp(2);
    else if (&rButton == mxDrawAllButton.get())
        triggerCreateApp(3);
    else if (&rButton == mxDBAllButton.get())
        dispatchURL(u"private:factory/sdatabase?Interactive"_ustr);
    else if (&rButton == mxMathAllButton.get())
        dispatchURL(u"private:factory/smath"_ustr);
    else if (&rButton == mxOpenButton.get())
        triggerOpen();
    else if (&rButton == mxRemoteButton.get())
        triggerOpenRemote();
}

IMPL_LINK(BackingWindow, MenuSelectHdl, const OUString&, rId, void)
{
    if (rId == "clear_all")
    {
        SvtHistoryOptions::Clear(EHistoryType::PickList, false);
        mxAllRecentThumbnails->Reload();
        mxHomeRecent->Reload();
        return;
    }
    else if (rId == "clear_unavailable")
    {
        mxAllRecentThumbnails->clearUnavailableFiles();
        mxHomeRecent->Reload();
    }
}

IMPL_LINK(BackingWindow, CreateContextMenuHdl, TemplateViewItem*, pItem, void)
{
    if (!pItem)
        return;

    bool bIsInternal = TemplateLocalView::IsInternalTemplate(pItem->getPath());
    mxLocalView->createContextMenu(bIsInternal);
}

IMPL_LINK(BackingWindow, OpenTemplateHdl, const OUString&, rTemplatePath, void)
{
    uno::Sequence<PropertyValue> aArgs{
        comphelper::makePropertyValue(u"AsTemplate"_ustr, true),
        comphelper::makePropertyValue(u"MacroExecutionMode"_ustr, MacroExecMode::USE_CONFIG),
        comphelper::makePropertyValue(u"UpdateDocMode"_ustr, UpdateDocMode::ACCORDING_TO_CONFIG),
        comphelper::makePropertyValue(u"InteractionHandler"_ustr, task::InteractionHandler::createWithParent(::comphelper::getProcessComponentContext(), nullptr))
    };

    Reference<XDispatchProvider> xFrame(mxFrame, UNO_QUERY);
    try
    {
        dispatchURL(rTemplatePath, u"_default"_ustr, xFrame, aArgs);
    }
    catch (const uno::Exception&)
    {
    }
}

IMPL_LINK(BackingWindow, EditTemplateHdl, const OUString&, rTemplatePath, void)
{
    uno::Sequence<PropertyValue> aArgs{
        comphelper::makePropertyValue(u"AsTemplate"_ustr, false),
        comphelper::makePropertyValue(u"MacroExecutionMode"_ustr, MacroExecMode::USE_CONFIG),
        comphelper::makePropertyValue(u"UpdateDocMode"_ustr, UpdateDocMode::ACCORDING_TO_CONFIG),
    };

    Reference<XDispatchProvider> xFrame(mxFrame, UNO_QUERY);
    try
    {
        dispatchURL(rTemplatePath, u"_default"_ustr, xFrame, aArgs);
    }
    catch (const uno::Exception&)
    {
    }
}

namespace
{
struct ImplDelayedDispatch
{
    Reference<XDispatch> xDispatch;
    css::util::URL aDispatchURL;
    Sequence<PropertyValue> aArgs;

    ImplDelayedDispatch(const Reference<XDispatch>& i_xDispatch,
                        css::util::URL i_aURL,
                        const Sequence<PropertyValue>& i_rArgs)
        : xDispatch(i_xDispatch)
        , aDispatchURL(std::move(i_aURL))
        , aArgs(i_rArgs)
    {
    }
};
}

static void implDispatchDelayed(void*, void* pArg)
{
    struct ImplDelayedDispatch* pDispatch = static_cast<ImplDelayedDispatch*>(pArg);
    try
    {
        pDispatch->xDispatch->dispatch(pDispatch->aDispatchURL, pDispatch->aArgs);
    }
    catch (const Exception&)
    {
    }
    delete pDispatch;
}

void BackingWindow::dispatchURL(const OUString& i_rURL,
                                const OUString& rTarget,
                                const Reference<XDispatchProvider>& i_xProv,
                                const Sequence<PropertyValue>& i_rArgs)
{
    Reference<XDispatchProvider> xProvider(i_xProv.is() ? i_xProv : mxDesktopDispatchProvider);
    if (!xProvider.is())
        return;

    css::util::URL aDispatchURL;
    aDispatchURL.Complete = i_rURL;

    Reference<css::util::XURLTransformer> xURLTransformer(
        css::util::URLTransformer::create(comphelper::getProcessComponentContext()));
    try
    {
        xURLTransformer->parseStrict(aDispatchURL);
        Reference<XDispatch> xDispatch(xProvider->queryDispatch(aDispatchURL, rTarget, 0));
        if (xDispatch.is())
        {
            std::unique_ptr<ImplDelayedDispatch> pDisp(new ImplDelayedDispatch(xDispatch, std::move(aDispatchURL), i_rArgs));
            if (Application::PostUserEvent(LINK_NONMEMBER(nullptr, implDispatchDelayed), pDisp.get()))
                pDisp.release();
        }
    }
    catch (const css::uno::RuntimeException&)
    {
        throw;
    }
    catch (const css::uno::Exception&)
    {
    }
}

void BackingWindow::clearRecentFileList()
{
    mxAllRecentThumbnails->Clear();
    mxAllRecentThumbnails->Reload();
    mxHomeRecent->Reload();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab:*/
