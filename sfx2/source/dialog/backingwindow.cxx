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
#include <cmath>
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
constexpr Color COL_NAV_BG(0xF8, 0xFA, 0xFC);
constexpr Color COL_NAV_HOVER(0xEE, 0xF3, 0xFA);
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
constexpr Color COL_CARD_DESIGN(0x7C, 0x3A, 0xED);
constexpr Color COL_CARD_DB(0xDC, 0x26, 0x26);
constexpr Color COL_CARD_EMAIL(0x1A, 0x73, 0xE8);
constexpr Color COL_CARD_PDF(0xC8, 0x1E, 0x2B);
constexpr Color COL_CARD_ESIGN(0x0D, 0x94, 0x88);
constexpr Color COL_CARD_MATH(0x7C, 0x3A, 0xED);
constexpr Color COL_BADGE_BG(0xE8, 0xEE, 0xF5);
constexpr Color COL_BADGE_TEXT(0x33, 0x41, 0x55);

double GetScale(weld::DrawingArea* pArea)
{
    if (!pArea)
        return 1.0;
    return std::max(1.0, static_cast<double>(pArea->get_ref_device().GetDPIScaleFactor()));
}

double GetScale(const vcl::RenderContext& rCtx)
{
    return std::max(1.0, static_cast<double>(rCtx.GetDPIScaleFactor()));
}

tools::Long Sx(tools::Long nPx, double fScale)
{
    return static_cast<tools::Long>(std::lround(static_cast<double>(nPx) * fScale));
}

vcl::Font MakeUiFont(int nHeightPx, FontWeight eWeight = WEIGHT_NORMAL, bool bItalic = false)
{
    vcl::Font aFont(Application::GetSettings().GetStyleSettings().GetLabelFont());
    aFont.SetFamilyName(u"Segoe UI"_ustr);
    aFont.SetFontHeight(nHeightPx);
    aFont.SetWeight(eWeight);
    if (bItalic)
        aFont.SetItalic(ITALIC_NORMAL);
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

void DrawDownArc(vcl::RenderContext& rRenderContext, double cx, double y0, double rx, double ry,
                 Color aColor, double fWidth)
{
    basegfx::B2DPolygon aArc;
    aArc.append(basegfx::B2DPoint(cx - rx, y0));
    aArc.appendBezierSegment(
        basegfx::B2DPoint(cx - rx * 0.45, y0 + ry),
        basegfx::B2DPoint(cx + rx * 0.45, y0 + ry),
        basegfx::B2DPoint(cx + rx, y0));
    rRenderContext.SetFillColor();
    rRenderContext.SetLineColor(aColor);
    rRenderContext.DrawPolyLine(aArc, fWidth, basegfx::B2DLineJoin::Round, css::drawing::LineCap_ROUND);
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
    int nKind = 0; // 0=other, 1=docs, 2=sheet, 3=slides, 4=design, 5=db, 6=math
    if (aExt == u"odt" || aExt == u"fodt" || aExt == u"doc" || aExt == u"docx" || aExt == u"rtf" || aExt == u"txt" || aExt == u"ott")
        nKind = 1;
    else if (aExt == u"ods" || aExt == u"fods" || aExt == u"xls" || aExt == u"xlsx" || aExt == u"csv" || aExt == u"ots")
        nKind = 2;
    else if (aExt == u"odp" || aExt == u"fodp" || aExt == u"ppt" || aExt == u"pptx" || aExt == u"pps" || aExt == u"otp")
        nKind = 3;
    else if (aExt == u"odg" || aExt == u"fodg" || aExt == u"otg" || aExt == u"vsd" || aExt == u"vsdx")
        nKind = 4;
    else if (aExt == u"odb" || aExt == u"dbf")
        nKind = 5;
    else if (aExt == u"odf" || aExt == u"sxm" || aExt == u"mml")
        nKind = 6;

    Color aColor = COL_TEXT_MUTED;
    if (nKind == 1) aColor = COL_CARD_DOCS;
    else if (nKind == 2) aColor = COL_CARD_SHEET;
    else if (nKind == 3) aColor = COL_CARD_SLIDE;
    else if (nKind == 4) aColor = COL_CARD_DESIGN;
    else if (nKind == 5) aColor = COL_CARD_DB;
    else if (nKind == 6) aColor = COL_CARD_MATH;

    const double x = nX;
    const double y = nY;
    const double w = nSize;
    const double h = nSize;
    const double fS = std::max(1.0, w / 20.0);

    if (nKind == 2)
    {
        DrawRoundedBox(rRenderContext, tools::Rectangle(Point(nX + 1, nY + 2), Size(nSize - 2, nSize - 4)), 2.5 * fS, COL_SURFACE_WHITE, aColor, 1.5 * fS);
        DrawStrokePoly(rRenderContext, { { x + 2 * fS, y + h * 0.42 }, { x + w - 2 * fS, y + h * 0.42 } }, false, aColor, 1.3 * fS);
        DrawStrokePoly(rRenderContext, { { x + 2 * fS, y + h * 0.66 }, { x + w - 2 * fS, y + h * 0.66 } }, false, aColor, 1.3 * fS);
        DrawStrokePoly(rRenderContext, { { x + w * 0.42, y + 3 * fS }, { x + w * 0.42, y + h - 3 * fS } }, false, aColor, 1.3 * fS);
        DrawStrokePoly(rRenderContext, { { x + w * 0.68, y + 3 * fS }, { x + w * 0.68, y + h - 3 * fS } }, false, aColor, 1.3 * fS);
    }
    else if (nKind == 3)
    {
        DrawRoundedBox(rRenderContext, tools::Rectangle(Point(nX + 1, nY + 2), Size(nSize - 2, nSize - static_cast<tools::Long>(7 * fS))), 2.5 * fS, COL_SURFACE_WHITE, aColor, 1.5 * fS);
        DrawStrokePoly(rRenderContext, { { x + w * 0.5, y + h - 5 * fS }, { x + w * 0.3, y + h - fS } }, false, aColor, 1.5 * fS);
        DrawStrokePoly(rRenderContext, { { x + w * 0.5, y + h - 5 * fS }, { x + w * 0.7, y + h - fS } }, false, aColor, 1.5 * fS);
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
        const tools::Long bw = static_cast<tools::Long>(std::lround(7 * fS));
        const tools::Long bh = static_cast<tools::Long>(std::lround(5 * fS));
        DrawRoundedBox(rRenderContext, tools::Rectangle(Point(nX + nSize / 2 - bw / 2, nY + 2), Size(bw + 1, bh)), 1.5 * fS, COL_SURFACE_WHITE, aColor, 1.4 * fS);
        DrawRoundedBox(rRenderContext, tools::Rectangle(Point(nX + 2, nY + nSize - bh - 3), Size(bw, bh)), 1.5 * fS, COL_SURFACE_WHITE, aColor, 1.4 * fS);
        DrawRoundedBox(rRenderContext, tools::Rectangle(Point(nX + nSize - bw - 2, nY + nSize - bh - 3), Size(bw, bh)), 1.5 * fS, COL_SURFACE_WHITE, aColor, 1.4 * fS);
        DrawStrokePoly(rRenderContext, { { x + w * 0.5, y + bh + 2 }, { x + w * 0.5, y + h * 0.55 } }, false, aColor, 1.3 * fS);
        DrawStrokePoly(rRenderContext, { { x + 5.5 * fS, y + h - bh - 3 }, { x + 5.5 * fS, y + h * 0.55 }, { x + w - 5.5 * fS, y + h * 0.55 }, { x + w - 5.5 * fS, y + h - bh - 3 } }, false, aColor, 1.3 * fS);
    }
    else if (nKind == 5)
    {
        const double cx = x + w * 0.5;
        const double rx = w * 0.34;
        const double ry = h * 0.14;
        const double yTop = y + h * 0.24;
        const double yMid = y + h * 0.52;
        const double yBot = y + h * 0.78;
        basegfx::B2DPolygon aTop = basegfx::utils::createPolygonFromEllipse(basegfx::B2DPoint(cx, yTop), rx, ry);
        rRenderContext.SetLineColor();
        rRenderContext.SetFillColor(COL_SURFACE_WHITE);
        rRenderContext.DrawPolygon(aTop);
        rRenderContext.SetFillColor();
        rRenderContext.SetLineColor(aColor);
        rRenderContext.DrawPolyLine(aTop, 1.4 * fS, basegfx::B2DLineJoin::Round);
        DrawStrokePoly(rRenderContext, { { cx - rx, yTop }, { cx - rx, yBot } }, false, aColor, 1.4 * fS);
        DrawStrokePoly(rRenderContext, { { cx + rx, yTop }, { cx + rx, yBot } }, false, aColor, 1.4 * fS);
        DrawDownArc(rRenderContext, cx, yMid, rx, ry * 1.4, aColor, 1.3 * fS);
        DrawDownArc(rRenderContext, cx, yBot, rx, ry * 1.4, aColor, 1.4 * fS);
    }
    else if (nKind == 6)
    {
        DrawStrokePoly(rRenderContext, {
            { x + w * 0.78, y + h * 0.28 },
            { x + w * 0.78, y + h * 0.16 },
            { x + w * 0.20, y + h * 0.16 },
            { x + w * 0.54, y + h * 0.50 },
            { x + w * 0.20, y + h * 0.84 },
            { x + w * 0.78, y + h * 0.84 },
            { x + w * 0.78, y + h * 0.72 }
        }, false, aColor, 1.6 * fS);
    }
    else
    {
        const double fold = w * 0.28;
        DrawStrokePoly(rRenderContext, {
            { x + 3 * fS, y + fS },
            { x + w - 3 * fS - fold, y + fS },
            { x + w - 3 * fS, y + fS + fold },
            { x + w - 3 * fS, y + h - fS },
            { x + 3 * fS, y + h - fS }
        }, true, aColor, 1.6 * fS);
        DrawStrokePoly(rRenderContext, { { x + 6.5 * fS, y + h * 0.46 }, { x + w - 6.5 * fS, y + h * 0.46 } }, false, aColor, 1.5 * fS);
        DrawStrokePoly(rRenderContext, { { x + 6.5 * fS, y + h * 0.64 }, { x + w - 6.5 * fS, y + h * 0.64 } }, false, aColor, 1.5 * fS);
        DrawStrokePoly(rRenderContext, { { x + 6.5 * fS, y + h * 0.80 }, { x + w * 0.62, y + h * 0.80 } }, false, aColor, 1.5 * fS);
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
    double mfScale = 1.0;

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
        mfScale = GetScale(pDrawingArea);
        pDrawingArea->get_ref_device().SetBackground(Wallpaper(COL_SURFACE_WHITE));
        pDrawingArea->set_size_request(-1, Sx(76, mfScale));
        SetPointer(PointerStyle::RefHand);
    }

    virtual void Resize() override
    {
        EnsureLogoLoaded(Sx(224, mfScale));
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
        mfScale = GetScale(rRenderContext);

        const Size aSize = GetOutputSizePixel();
        const tools::Long w = aSize.Width();
        const tools::Long h = aSize.Height();

        FillSolidRect(rRenderContext, tools::Rectangle(Point(0, 0), aSize), COL_SURFACE_WHITE);

        // Top-right sweeping curved corner accents (red outer wave + navy inner corner)
        {
            basegfx::B2DPolygon aRedWave;
            aRedWave.append(basegfx::B2DPoint(std::max<double>(w * 0.74, w - 245.0 * mfScale), h - 1));
            aRedWave.appendBezierSegment(
                basegfx::B2DPoint(std::max<double>(w * 0.86, w - 135.0 * mfScale), h * 0.72),
                basegfx::B2DPoint(std::max<double>(w * 0.94, w - 54.0 * mfScale), h * 0.36),
                basegfx::B2DPoint(w, 0));
            aRedWave.append(basegfx::B2DPoint(w, h * 0.62));
            aRedWave.appendBezierSegment(
                basegfx::B2DPoint(std::max<double>(w * 0.93, w - 64.0 * mfScale), h * 0.78),
                basegfx::B2DPoint(std::max<double>(w * 0.86, w - 140.0 * mfScale), h - 1),
                basegfx::B2DPoint(std::max<double>(w * 0.74, w - 245.0 * mfScale), h - 1));
            aRedWave.setClosed(true);
            rRenderContext.SetLineColor();
            rRenderContext.SetFillColor(COL_RED_BRAND);
            rRenderContext.DrawPolygon(aRedWave);

            basegfx::B2DPolygon aNavyWave;
            aNavyWave.append(basegfx::B2DPoint(std::max<double>(w * 0.79, w - 190.0 * mfScale), h - 1));
            aNavyWave.appendBezierSegment(
                basegfx::B2DPoint(std::max<double>(w * 0.89, w - 100.0 * mfScale), h * 0.68),
                basegfx::B2DPoint(std::max<double>(w * 0.95, w - 38.0 * mfScale), h * 0.32),
                basegfx::B2DPoint(w, 0));
            aNavyWave.append(basegfx::B2DPoint(w, h * 0.40));
            aNavyWave.appendBezierSegment(
                basegfx::B2DPoint(std::max<double>(w * 0.96, w - 32.0 * mfScale), h * 0.56),
                basegfx::B2DPoint(std::max<double>(w * 0.90, w - 92.0 * mfScale), h * 0.82),
                basegfx::B2DPoint(std::max<double>(w * 0.82, w - 165.0 * mfScale), h - 1));
            aNavyWave.setClosed(true);
            rRenderContext.SetFillColor(COL_NAVY_DARK);
            rRenderContext.DrawPolygon(aNavyWave);
        }

        // Approved OfficeStudio AI wordmark logo on the left
        EnsureLogoLoaded(Sx(224, mfScale));
        tools::Long nDivX = Sx(24, mfScale);
        if (!maLogoBitmap.IsEmpty())
        {
            const Size aImg = maLogoBitmap.GetSizePixel();
            const tools::Long nDrawH = Sx(30, mfScale);
            const tools::Long nDrawW = aImg.Height() > 0
                ? std::max<tools::Long>(Sx(145, mfScale), (nDrawH * aImg.Width()) / aImg.Height())
                : Sx(218, mfScale);
            const tools::Long nLogoX = Sx(24, mfScale);
            const tools::Long nLogoY = (h - nDrawH) / 2;
            rRenderContext.DrawBitmap(Point(nLogoX, nLogoY), Size(nDrawW, nDrawH), maLogoBitmap);
            nDivX = nLogoX + nDrawW + Sx(20, mfScale);
        }

        // Vertical divider + muted tagline wrapped cleanly in 2 lines
        const tools::Long nRightLimit = std::max<tools::Long>(nDivX + Sx(20, mfScale), w - Sx(180, mfScale));
        if (nRightLimit - nDivX > Sx(120, mfScale))
        {
            const tools::Long nDivPadY = Sx(18, mfScale);
            DrawStrokePoly(rRenderContext, { { static_cast<double>(nDivX), static_cast<double>(nDivPadY) },
                                             { static_cast<double>(nDivX), static_cast<double>(h - nDivPadY) } },
                           false, Color(0xD5, 0xDC, 0xE6), 1.0 * mfScale);

            rRenderContext.SetFont(MakeUiFont(Sx(12, mfScale), WEIGHT_NORMAL));
            rRenderContext.SetTextColor(COL_TEXT_MUTED);
            const tools::Long nTagPadX = Sx(18, mfScale);
            const tools::Long nTagW = std::min<tools::Long>(Sx(390, mfScale), nRightLimit - (nDivX + nTagPadX));
            tools::Rectangle aTagRect(Point(nDivX + nTagPadX, Sx(12, mfScale)), Size(nTagW, h - Sx(24, mfScale)));
            rRenderContext.DrawText(aTagRect,
                u"A modern, powerful office suite for documents, spreadsheets, presentations, diagrams, and databases."_ustr,
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
    double mfScale = 1.0;

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
        const tools::Long nPadX = Sx(12, mfScale);
        const tools::Long nItemW = std::max<tools::Long>(Sx(60, mfScale), w - 2 * nPadX);
        const tools::Long nItemH = Sx(38, mfScale);
        const tools::Long nGap = Sx(6, mfScale);
        tools::Long y = Sx(16, mfScale);
        for (int i = 0; i < 4; ++i)
        {
            aOut.push_back({ i, tools::Rectangle(Point(nPadX, y), Size(nItemW, nItemH)) });
            y += nItemH + nGap;
        }
        if (h > Sx(275, mfScale))
        {
            tools::Long uy = h - Sx(40, mfScale);
            aOut.push_back({ 5, tools::Rectangle(Point(nPadX, uy), Size(nItemW, Sx(30, mfScale))) });
            if (mbRemoteEnabled && h > Sx(315, mfScale))
                aOut.push_back({ 4, tools::Rectangle(Point(nPadX, uy - Sx(34, mfScale)), Size(nItemW, Sx(30, mfScale))) });
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

    static Color NavAccentColor(int nId)
    {
        switch (nId)
        {
            case 0: return Color(0x25, 0x63, 0xEB); // Home: Royal Blue
            case 1: return Color(0xEA, 0x58, 0x0C); // Recent: Warm Orange
            case 2: return Color(0x7C, 0x3A, 0xED); // Templates: Vibrant Purple
            case 3: return Color(0x16, 0xA3, 0x4A); // Open: Emerald Green
            case 4: return Color(0x0D, 0x94, 0x88); // Remote Files: Teal
            default: return Color(0x02, 0x84, 0xC7); // Help: Sky Blue
        }
    }

    static Color NavActiveFillColor(int nId)
    {
        switch (nId)
        {
            case 0: return Color(0xE8, 0xF1, 0xFD);
            case 1: return Color(0xFF, 0xF1, 0xE6);
            default: return Color(0xF3, 0xEC, 0xFE);
        }
    }

    static Color NavActiveBorderColor(int nId)
    {
        switch (nId)
        {
            case 0: return Color(0xBF, 0xDB, 0xFE);
            case 1: return Color(0xFE, 0xD7, 0xAA);
            default: return Color(0xDD, 0xD6, 0xFE);
        }
    }

    static Color NavActiveTextColor(int nId)
    {
        switch (nId)
        {
            case 0: return Color(0x1E, 0x40, 0xAF);
            case 1: return Color(0x9A, 0x34, 0x12);
            default: return Color(0x5B, 0x21, 0xB6);
        }
    }

    void DrawNavIcon(vcl::RenderContext& rRenderContext, int nId, const tools::Rectangle& rRect, Color aCol) const
    {
        const double cx = rRect.Left() + 24.0 * mfScale;
        const double cy = rRect.Top() + rRect.GetHeight() / 2.0;
        const double fS = mfScale;
        if (nId == 0) // House
        {
            DrawStrokePoly(rRenderContext, {
                { cx - 7 * fS, cy - 0.5 * fS }, { cx, cy - 7 * fS }, { cx + 7 * fS, cy - 0.5 * fS }
            }, false, aCol, 1.9 * fS);
            DrawStrokePoly(rRenderContext, {
                { cx - 5 * fS, cy - 1.5 * fS }, { cx - 5 * fS, cy + 6.5 * fS }, { cx + 5 * fS, cy + 6.5 * fS }, { cx + 5 * fS, cy - 1.5 * fS }
            }, false, aCol, 1.9 * fS);
            DrawStrokePoly(rRenderContext, {
                { cx - 1.6 * fS, cy + 6.5 * fS }, { cx - 1.6 * fS, cy + 2.5 * fS }, { cx + 1.6 * fS, cy + 2.5 * fS }, { cx + 1.6 * fS, cy + 6.5 * fS }
            }, false, aCol, 1.5 * fS);
        }
        else if (nId == 1) // Clock
        {
            DrawCircleStroke(rRenderContext, cx, cy, 7.0 * fS, aCol, 1.9 * fS);
            DrawStrokePoly(rRenderContext, { { cx, cy - 4 * fS }, { cx, cy }, { cx + 3.5 * fS, cy + 2 * fS } }, false, aCol, 1.8 * fS);
        }
        else if (nId == 2) // Layers / Templates
        {
            DrawStrokePoly(rRenderContext, { { cx, cy - 7 * fS }, { cx + 7 * fS, cy - 3.5 * fS }, { cx, cy }, { cx - 7 * fS, cy - 3.5 * fS } }, true, aCol, 1.8 * fS);
            DrawStrokePoly(rRenderContext, { { cx - 7 * fS, cy + 0.5 * fS }, { cx, cy + 4 * fS }, { cx + 7 * fS, cy + 0.5 * fS } }, false, aCol, 1.8 * fS);
            DrawStrokePoly(rRenderContext, { { cx - 7 * fS, cy + 4.5 * fS }, { cx, cy + 8 * fS }, { cx + 7 * fS, cy + 4.5 * fS } }, false, aCol, 1.8 * fS);
        }
        else if (nId == 3) // Open folder
        {
            DrawStrokePoly(rRenderContext, {
                { cx - 7 * fS, cy - 6 * fS }, { cx - 2 * fS, cy - 6 * fS }, { cx, cy - 4 * fS },
                { cx + 7 * fS, cy - 4 * fS }, { cx + 7 * fS, cy + 6 * fS }, { cx - 7 * fS, cy + 6 * fS }
            }, true, aCol, 1.85 * fS);
            DrawStrokePoly(rRenderContext, { { cx - 6 * fS, cy - 1 * fS }, { cx + 6 * fS, cy - 1 * fS } }, false, aCol, 1.4 * fS);
        }
        else if (nId == 4) // Globe / Remote
        {
            DrawCircleStroke(rRenderContext, cx, cy, 6.5 * fS, aCol, 1.7 * fS);
            DrawStrokePoly(rRenderContext, { { cx - 6.5 * fS, cy }, { cx + 6.5 * fS, cy } }, false, aCol, 1.5 * fS);
            basegfx::B2DPolygon aMeridian = basegfx::utils::createPolygonFromEllipse(basegfx::B2DPoint(cx, cy), 3.0 * fS, 6.5 * fS);
            rRenderContext.SetFillColor();
            rRenderContext.SetLineColor(aCol);
            rRenderContext.DrawPolyLine(aMeridian, 1.4 * fS, basegfx::B2DLineJoin::Round);
        }
        else if (nId == 5) // Help circle
        {
            DrawCircleStroke(rRenderContext, cx, cy, 6.5 * fS, aCol, 1.7 * fS);
            DrawStrokePoly(rRenderContext, { { cx - 2 * fS, cy - 2 * fS }, { cx, cy - 3.5 * fS }, { cx + 2 * fS, cy - 2 * fS }, { cx, cy + 0.5 * fS } }, false, aCol, 1.5 * fS);
            FillSolidRect(rRenderContext, tools::Rectangle(Point(static_cast<tools::Long>(cx - fS * 0.5), static_cast<tools::Long>(cy + 2.8 * fS)), Size(Sx(2, fS), Sx(2, fS))), aCol);
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
        mfScale = GetScale(pDrawingArea);
        pDrawingArea->get_ref_device().SetBackground(Wallpaper(COL_NAV_BG));
        pDrawingArea->set_size_request(Sx(188, mfScale), -1);
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
        mfScale = GetScale(rRenderContext);
        const Size aSz = GetOutputSizePixel();

        FillSolidRect(rRenderContext, tools::Rectangle(Point(0, 0), aSz), COL_NAV_BG);
        DrawStrokePoly(rRenderContext, { { static_cast<double>(aSz.Width() - 1), 0.0 }, { static_cast<double>(aSz.Width() - 1), static_cast<double>(aSz.Height()) } },
                       false, COL_BORDER, 1.0);

        if (aSz.Height() > Sx(275, mfScale))
        {
            const double fSepY = static_cast<double>(aSz.Height() - Sx(82, mfScale));
            DrawStrokePoly(rRenderContext, { { static_cast<double>(Sx(14, mfScale)), fSepY }, { static_cast<double>(aSz.Width() - Sx(14, mfScale)), fSepY } },
                           false, Color(0xE2, 0xE8, 0xF0), 1.0);
        }

        const int nActiveId = meActiveMode == StartCenterMode::Home ? 0
                            : (meActiveMode == StartCenterMode::Recent ? 1 : 2);

        for (const auto& item : GetHitRects())
        {
            const bool bActive = (item.nId == nActiveId);
            const bool bHover = (item.nId == mnHoveredIdx);
            const Color aAccent = NavAccentColor(item.nId);

            if (bActive)
            {
                DrawRoundedBox(rRenderContext, item.aRect, 8.0 * mfScale, NavActiveFillColor(item.nId), NavActiveBorderColor(item.nId), 1.0 * mfScale);
                tools::Rectangle aBar(
                    Point(item.aRect.Left() + Sx(3, mfScale), item.aRect.Top() + Sx(7, mfScale)),
                    Size(std::max<tools::Long>(3, Sx(3, mfScale)), std::max<tools::Long>(8, item.aRect.GetHeight() - Sx(14, mfScale))));
                DrawRoundedBox(rRenderContext, aBar, 2.0 * mfScale, aAccent, COL_TRANSPARENT, 0.0);
            }
            else if (bHover)
            {
                DrawRoundedBox(rRenderContext, item.aRect, 8.0 * mfScale, COL_NAV_HOVER, COL_TRANSPARENT, 0.0);
            }

            if (HasFocus() && item.nId == mnFocusedIdx && !bActive)
                DrawRoundedBox(rRenderContext, item.aRect, 8.0 * mfScale, COL_TRANSPARENT, COL_BORDER_HOVER, 1.2 * mfScale);

            DrawNavIcon(rRenderContext, item.nId, item.aRect, aAccent);

            OUString sLabel;
            if (item.nId == 0) sLabel = u"Home"_ustr;
            else if (item.nId == 1) sLabel = u"Recent"_ustr;
            else if (item.nId == 2) sLabel = u"Templates"_ustr;
            else if (item.nId == 3) sLabel = u"Open"_ustr;
            else if (item.nId == 4) sLabel = u"Remote Files"_ustr;
            else sLabel = u"Help"_ustr;

            const Color aTxtCol = bActive
                ? NavActiveTextColor(item.nId)
                : (item.nId >= 4 ? Color(0x47, 0x55, 0x69) : Color(0x1E, 0x29, 0x3B));
            const int nFontH = Sx(item.nId >= 4 ? 13 : 14, mfScale);
            rRenderContext.SetFont(MakeUiFont(nFontH, bActive ? WEIGHT_SEMIBOLD : WEIGHT_NORMAL));
            rRenderContext.SetTextColor(aTxtCol);
            tools::Rectangle aTxtRect(item.aRect.Left() + Sx(44, mfScale), item.aRect.Top(), item.aRect.Right() - Sx(8, mfScale), item.aRect.Bottom());
            rRenderContext.DrawText(aTxtRect, sLabel, DrawTextFlags::Left | DrawTextFlags::VCenter | DrawTextFlags::EndEllipsis);
        }
    }
};

// ---------------------------------------------------------------------------
// CreateCardsWidget: "Applications" heading + 3x3 grid of app cards (6 active apps + 3 Coming soon)
// ---------------------------------------------------------------------------
class CreateCardsWidget final : public weld::CustomWidgetController
{
private:
    static constexpr int CARD_COUNT = 9;
    BackingWindow& mrOwner;
    int mnHoveredCard = -1;
    int mnFocusedCard = 0;
    double mfScale = 1.0;
    std::array<bool, CARD_COUNT> maEnabled = {
        true,  true,  true,   // Row 1: Docs, Spreadsheet, SlideShows
        false, true,  true,   // Row 2: eMail (Coming soon), Design, Database
        false, false, true    // Row 3: eSign (Coming soon), PDF (Coming soon), Math
    };

    static bool IsComingSoonCard(int nIndex)
    {
        return nIndex == 3 || nIndex == 6 || nIndex == 7;
    }

    bool IsCardInteractive(int nIndex) const
    {
        return nIndex >= 0 && nIndex < CARD_COUNT && !IsComingSoonCard(nIndex) && maEnabled[nIndex];
    }

    tools::Rectangle GetCardRect(int nIndex) const
    {
        const Size aSz = GetOutputSizePixel();
        const int row = nIndex / 3;
        const int col = nIndex % 3;
        const tools::Long nPadX = Sx(24, mfScale);
        const tools::Long nGapX = Sx(16, mfScale);
        const tools::Long nTop = Sx(34, mfScale);
        const tools::Long nGapY = Sx(12, mfScale);
        const tools::Long nPadBot = Sx(6, mfScale);
        const tools::Long nAvailW = std::max<tools::Long>(Sx(360, mfScale), aSz.Width() - 2 * nPadX - 2 * nGapX);
        const tools::Long nCardW = nAvailW / 3;
        const tools::Long nCardH = std::max<tools::Long>(Sx(100, mfScale), (aSz.Height() - nTop - 2 * nGapY - nPadBot) / 3);
        return tools::Rectangle(
            Point(nPadX + col * (nCardW + nGapX), nTop + row * (nCardH + nGapY)),
            Size(nCardW, nCardH));
    }

    int HitTest(const Point& rPos) const
    {
        for (int i = 0; i < CARD_COUNT; ++i)
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
        const double fS = mfScale;
        const double fStroke = 2.6 * mfScale;
        if (nIdx == 0) // Docs
        {
            const double w = 30 * fS, h = 38 * fS, fold = 9 * fS;
            DrawStrokePoly(rRenderContext, {
                { x - w / 2, y - h / 2 },
                { x + w / 2 - fold, y - h / 2 },
                { x + w / 2, y - h / 2 + fold },
                { x + w / 2, y + h / 2 },
                { x - w / 2, y + h / 2 }
            }, true, aAccent, fStroke);
            DrawStrokePoly(rRenderContext, { { x + w / 2 - fold, y - h / 2 }, { x + w / 2 - fold, y - h / 2 + fold }, { x + w / 2, y - h / 2 + fold } }, false, aAccent, 2.1 * mfScale);
            DrawStrokePoly(rRenderContext, { { x - 8 * fS, y - 2.5 * fS }, { x + 8 * fS, y - 2.5 * fS } }, false, aAccent, 2.3 * mfScale);
            DrawStrokePoly(rRenderContext, { { x - 8 * fS, y + 4.5 * fS }, { x + 8 * fS, y + 4.5 * fS } }, false, aAccent, 2.3 * mfScale);
            DrawStrokePoly(rRenderContext, { { x - 8 * fS, y + 11.5 * fS }, { x + 2.5 * fS, y + 11.5 * fS } }, false, aAccent, 2.3 * mfScale);
        }
        else if (nIdx == 1) // Spreadsheet
        {
            const tools::Long hw = Sx(18, fS);
            DrawRoundedBox(rRenderContext, tools::Rectangle(Point(nCx - hw, nCy - hw), Size(2 * hw, 2 * hw)), 5.0 * mfScale, COL_SURFACE_WHITE, aAccent, fStroke);
            DrawStrokePoly(rRenderContext, { { x - hw, y - 6 * fS }, { x + hw, y - 6 * fS } }, false, aAccent, 2.2 * mfScale);
            DrawStrokePoly(rRenderContext, { { x - hw, y + 6 * fS }, { x + hw, y + 6 * fS } }, false, aAccent, 2.2 * mfScale);
            DrawStrokePoly(rRenderContext, { { x - 6 * fS, y - hw }, { x - 6 * fS, y + hw } }, false, aAccent, 2.2 * mfScale);
            DrawStrokePoly(rRenderContext, { { x + 6 * fS, y - hw }, { x + 6 * fS, y + hw } }, false, aAccent, 2.2 * mfScale);
        }
        else if (nIdx == 2) // SlideShows
        {
            const tools::Long bw = Sx(40, fS);
            const tools::Long bh = Sx(26, fS);
            DrawRoundedBox(rRenderContext, tools::Rectangle(Point(nCx - bw / 2, nCy - Sx(13, fS)), Size(bw, bh)), 5.0 * mfScale, COL_SURFACE_WHITE, aAccent, fStroke);
            DrawStrokePoly(rRenderContext, { { x, y - 18 * fS }, { x, y - 13 * fS } }, false, aAccent, 2.4 * mfScale);
            DrawStrokePoly(rRenderContext, { { x - 8 * fS, y + 20 * fS }, { x, y + 13 * fS }, { x + 8 * fS, y + 20 * fS } }, false, aAccent, 2.4 * mfScale);
            basegfx::B2DPolygon aPlay;
            aPlay.append(basegfx::B2DPoint(x - 4 * fS, y - 6 * fS));
            aPlay.append(basegfx::B2DPoint(x - 4 * fS, y + 6 * fS));
            aPlay.append(basegfx::B2DPoint(x + 7 * fS, y));
            aPlay.setClosed(true);
            rRenderContext.SetLineColor();
            rRenderContext.SetFillColor(aAccent);
            rRenderContext.DrawPolygon(aPlay);
        }
        else if (nIdx == 3) // eMail (rounded envelope + V flap)
        {
            const tools::Long ew = Sx(42, fS);
            const tools::Long eh = Sx(28, fS);
            DrawRoundedBox(rRenderContext, tools::Rectangle(Point(nCx - ew / 2, nCy - eh / 2), Size(ew, eh)),
                           4.5 * mfScale, COL_SURFACE_WHITE, aAccent, fStroke);
            DrawStrokePoly(rRenderContext, {
                { x - ew * 0.45, y - eh * 0.36 },
                { x, y + eh * 0.14 },
                { x + ew * 0.45, y - eh * 0.36 }
            }, false, aAccent, 2.5 * mfScale);
        }
        else if (nIdx == 4) // Design (purple drawing paintbrush tilted from top-right to bottom-left)
        {
            basegfx::B2DPolygon aBrush;
            aBrush.append(basegfx::B2DPoint(x - 2.5 * fS, y - 1.5 * fS));
            aBrush.append(basegfx::B2DPoint(x + 10.5 * fS, y - 15.0 * fS));
            aBrush.appendBezierSegment(
                basegfx::B2DPoint(x + 14.5 * fS, y - 16.5 * fS),
                basegfx::B2DPoint(x + 16.5 * fS, y - 14.5 * fS),
                basegfx::B2DPoint(x + 15.0 * fS, y - 10.5 * fS));
            aBrush.append(basegfx::B2DPoint(x + 1.5 * fS, y + 2.5 * fS));
            aBrush.setClosed(true);
            rRenderContext.SetFillColor(COL_SURFACE_WHITE);
            rRenderContext.SetLineColor(aAccent);
            rRenderContext.DrawPolygon(aBrush);
            rRenderContext.SetFillColor();
            rRenderContext.DrawPolyLine(aBrush, 2.5 * mfScale, basegfx::B2DLineJoin::Round, css::drawing::LineCap_ROUND);

            DrawStrokePoly(rRenderContext, {
                { x + 0.5 * fS, y - 4.5 * fS },
                { x + 4.5 * fS, y - 0.5 * fS }
            }, false, aAccent, 2.2 * mfScale);

            basegfx::B2DPolygon aBristle;
            aBristle.append(basegfx::B2DPoint(x - 2.5 * fS, y - 1.5 * fS));
            aBristle.appendBezierSegment(
                basegfx::B2DPoint(x - 8.5 * fS, y + 1.0 * fS),
                basegfx::B2DPoint(x - 8.5 * fS, y + 8.5 * fS),
                basegfx::B2DPoint(x - 14.5 * fS, y + 14.5 * fS));
            aBristle.appendBezierSegment(
                basegfx::B2DPoint(x - 5.5 * fS, y + 15.5 * fS),
                basegfx::B2DPoint(x + 0.5 * fS, y + 9.5 * fS),
                basegfx::B2DPoint(x + 1.5 * fS, y + 2.5 * fS));
            rRenderContext.SetFillColor(COL_SURFACE_WHITE);
            rRenderContext.SetLineColor(aAccent);
            rRenderContext.DrawPolygon(aBristle);
            rRenderContext.SetFillColor();
            rRenderContext.DrawPolyLine(aBristle, 2.5 * mfScale, basegfx::B2DLineJoin::Round, css::drawing::LineCap_ROUND);
        }
        else if (nIdx == 5) // Database (3-band stacked cylinder)
        {
            const double rx = 16.0 * fS;
            const double ry = 5.0 * fS;
            const double yTop = y - 13.0 * fS;
            const double yMid1 = y - 4.0 * fS;
            const double yMid2 = y + 5.0 * fS;
            const double yBot = y + 14.0 * fS;
            basegfx::B2DPolygon aTop = basegfx::utils::createPolygonFromEllipse(basegfx::B2DPoint(x, yTop), rx, ry);
            rRenderContext.SetLineColor();
            rRenderContext.SetFillColor(COL_SURFACE_WHITE);
            rRenderContext.DrawPolygon(aTop);
            rRenderContext.SetFillColor();
            rRenderContext.SetLineColor(aAccent);
            rRenderContext.DrawPolyLine(aTop, 2.5 * mfScale, basegfx::B2DLineJoin::Round);
            DrawStrokePoly(rRenderContext, { { x - rx, yTop }, { x - rx, yBot } }, false, aAccent, 2.5 * mfScale);
            DrawStrokePoly(rRenderContext, { { x + rx, yTop }, { x + rx, yBot } }, false, aAccent, 2.5 * mfScale);
            DrawDownArc(rRenderContext, x, yMid1, rx, ry * 1.4, aAccent, 2.3 * mfScale);
            DrawDownArc(rRenderContext, x, yMid2, rx, ry * 1.4, aAccent, 2.3 * mfScale);
            DrawDownArc(rRenderContext, x, yBot, rx, ry * 1.4, aAccent, 2.5 * mfScale);
        }
        else if (nIdx == 6) // eSign (tilted fountain pen + cursive Bezier signature)
        {
            DrawStrokePoly(rRenderContext, {
                { x - 1.5 * fS, y + 3.0 * fS },
                { x + 0.1 * fS, y - 3.2 * fS },
                { x + 11.8 * fS, y - 14.8 * fS },
                { x + 16.3 * fS, y - 10.3 * fS },
                { x + 4.7 * fS, y + 1.4 * fS }
            }, true, aAccent, 2.4 * mfScale);
            DrawStrokePoly(rRenderContext, {
                { x + 8.3 * fS, y - 11.3 * fS },
                { x + 12.8 * fS, y - 6.8 * fS }
            }, false, aAccent, 2.1 * mfScale);

            basegfx::B2DPolygon aSig;
            aSig.append(basegfx::B2DPoint(x - 18.0 * fS, y + 11.0 * fS));
            aSig.appendBezierSegment(
                basegfx::B2DPoint(x - 14.0 * fS, y - 2.0 * fS),
                basegfx::B2DPoint(x - 10.0 * fS, y - 2.0 * fS),
                basegfx::B2DPoint(x - 8.5 * fS, y + 9.0 * fS));
            aSig.appendBezierSegment(
                basegfx::B2DPoint(x - 7.0 * fS, y + 16.0 * fS),
                basegfx::B2DPoint(x - 3.5 * fS, y + 7.0 * fS),
                basegfx::B2DPoint(x + 2.0 * fS, y + 10.0 * fS));
            aSig.appendBezierSegment(
                basegfx::B2DPoint(x + 7.5 * fS, y + 13.0 * fS),
                basegfx::B2DPoint(x + 12.5 * fS, y + 9.5 * fS),
                basegfx::B2DPoint(x + 17.0 * fS, y + 11.0 * fS));
            rRenderContext.SetFillColor();
            rRenderContext.SetLineColor(aAccent);
            rRenderContext.DrawPolyLine(aSig, 2.5 * mfScale, basegfx::B2DLineJoin::Round, css::drawing::LineCap_ROUND);
        }
        else if (nIdx == 7) // PDF (folded page + bold PDF text)
        {
            const double w = 34 * fS, h = 40 * fS, fold = 9 * fS;
            DrawStrokePoly(rRenderContext, {
                { x - w / 2, y - h / 2 },
                { x + w / 2 - fold, y - h / 2 },
                { x + w / 2, y - h / 2 + fold },
                { x + w / 2, y + h / 2 },
                { x - w / 2, y + h / 2 }
            }, true, aAccent, fStroke);
            DrawStrokePoly(rRenderContext, {
                { x + w / 2 - fold, y - h / 2 },
                { x + w / 2 - fold, y - h / 2 + fold },
                { x + w / 2, y - h / 2 + fold }
            }, false, aAccent, 2.1 * mfScale);
            rRenderContext.SetFont(MakeUiFont(Sx(11, mfScale), WEIGHT_BOLD));
            rRenderContext.SetTextColor(aAccent);
            rRenderContext.DrawText(
                tools::Rectangle(Point(nCx - Sx(16, fS), nCy - Sx(3, fS)), Size(Sx(32, fS), Sx(18, fS))),
                u"PDF"_ustr, DrawTextFlags::Center | DrawTextFlags::VCenter);
        }
        else // Math (purple Sigma symbol)
        {
            DrawStrokePoly(rRenderContext, {
                { x + 13.5 * fS, y - 11.0 * fS },
                { x + 13.5 * fS, y - 15.5 * fS },
                { x - 13.0 * fS, y - 15.5 * fS },
                { x + 1.0 * fS,  y },
                { x - 13.0 * fS, y + 15.5 * fS },
                { x + 13.5 * fS, y + 15.5 * fS },
                { x + 13.5 * fS, y + 11.0 * fS }
            }, false, aAccent, 2.8 * mfScale);
        }
    }

public:
    explicit CreateCardsWidget(BackingWindow& rOwner)
        : mrOwner(rOwner)
    {
    }

    void SetModuleStates(bool bDocs, bool bCalc, bool bImpress, bool bDraw, bool bDatabase, bool bMath)
    {
        maEnabled = { bDocs, bCalc, bImpress, false, bDraw, bDatabase, false, false, bMath };
        Invalidate();
    }

    virtual void SetDrawingArea(weld::DrawingArea* pDrawingArea) override
    {
        weld::CustomWidgetController::SetDrawingArea(pDrawingArea);
        mfScale = GetScale(pDrawingArea);
        pDrawingArea->get_ref_device().SetBackground(Wallpaper(COL_SURFACE_WHITE));
        pDrawingArea->set_size_request(-1, Sx(408, mfScale));
    }

    virtual OUString RequestHelp(tools::Rectangle&) override
    {
        static const std::array<OUString, CARD_COUNT> aTips = {
            u"Docs — Create text documents"_ustr,
            u"Spreadsheet — Work with data and tables"_ustr,
            u"SlideShows — Create presentations"_ustr,
            u"eMail — Coming soon"_ustr,
            u"Design — Create drawings, diagrams, and graphics"_ustr,
            u"Database — Manage databases and tables"_ustr,
            u"eSign — Coming soon"_ustr,
            u"PDF — Coming soon"_ustr,
            u"Math — Create and edit mathematical equations"_ustr
        };
        if (mnHoveredCard >= 0 && mnHoveredCard < CARD_COUNT)
            return aTips[mnHoveredCard];
        return OUString();
    }

    virtual bool MouseMove(const MouseEvent& rMEvt) override
    {
        const int nHit = rMEvt.IsLeaveWindow() ? -1 : HitTest(rMEvt.GetPosPixel());
        if (nHit != mnHoveredCard)
        {
            mnHoveredCard = nHit;
            Invalidate();
        }
        SetPointer(IsCardInteractive(nHit) ? PointerStyle::RefHand : PointerStyle::Arrow);
        return false;
    }

    virtual bool MouseButtonUp(const MouseEvent& rMEvt) override
    {
        if (rMEvt.IsLeft())
        {
            const int nHit = HitTest(rMEvt.GetPosPixel());
            if (IsCardInteractive(nHit))
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
        static constexpr std::array<int, 6> aInteractiveOrder = { 0, 1, 2, 4, 5, 8 };
        auto findPos = [&](int nCard) -> int {
            for (int k = 0; k < static_cast<int>(aInteractiveOrder.size()); ++k)
            {
                if (aInteractiveOrder[k] == nCard)
                    return k;
            }
            return 0;
        };

        sal_uInt16 nCode = rKEvt.GetKeyCode().GetCode();
        if (nCode == KEY_RIGHT || nCode == KEY_DOWN)
        {
            const int nPos = findPos(mnFocusedCard);
            mnFocusedCard = aInteractiveOrder[(nPos + 1) % aInteractiveOrder.size()];
            Invalidate();
            return true;
        }
        if (nCode == KEY_LEFT || nCode == KEY_UP)
        {
            const int nPos = findPos(mnFocusedCard);
            mnFocusedCard = aInteractiveOrder[(nPos + aInteractiveOrder.size() - 1) % aInteractiveOrder.size()];
            Invalidate();
            return true;
        }
        if ((nCode == KEY_RETURN || nCode == KEY_SPACE) && IsCardInteractive(mnFocusedCard))
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
        mfScale = GetScale(rRenderContext);
        const Size aSz = GetOutputSizePixel();

        FillSolidRect(rRenderContext, tools::Rectangle(Point(0, 0), aSz), COL_SURFACE_WHITE);

        // Section heading
        rRenderContext.SetFont(MakeUiFont(Sx(17, mfScale), WEIGHT_BOLD));
        rRenderContext.SetTextColor(COL_TEXT_PRIMARY);
        rRenderContext.DrawText(tools::Rectangle(Point(Sx(24, mfScale), Sx(4, mfScale)), Size(Sx(280, mfScale), Sx(24, mfScale))), u"Applications"_ustr,
                                DrawTextFlags::Left | DrawTextFlags::VCenter);

        static const std::array<Color, CARD_COUNT> aAccents = {
            COL_CARD_DOCS, COL_CARD_SHEET, COL_CARD_SLIDE,
            COL_CARD_EMAIL, COL_CARD_DESIGN, COL_CARD_DB,
            COL_CARD_ESIGN, COL_CARD_PDF, COL_CARD_MATH
        };
        static const std::array<OUString, CARD_COUNT> aTitles = {
            u"Docs"_ustr, u"Spreadsheet"_ustr, u"SlideShows"_ustr,
            u"eMail"_ustr, u"Design"_ustr, u"Database"_ustr,
            u"eSign"_ustr, u"PDF"_ustr, u"Math"_ustr
        };
        static const std::array<OUString, CARD_COUNT> aSubsWide = {
            u"Create text documents"_ustr,
            u"Work with data and tables"_ustr,
            u"Create presentations"_ustr,
            u"Read and write email"_ustr,
            u"Create drawings, diagrams, and graphics"_ustr,
            u"Manage databases and tables"_ustr,
            u"Sign and send documents"_ustr,
            u"View and edit PDFs"_ustr,
            u"Create and edit mathematical equations"_ustr
        };
        static const std::array<OUString, CARD_COUNT> aSubsCompact = {
            u"Text documents"_ustr,
            u"Data & tables"_ustr,
            u"Presentations"_ustr,
            u"Read & write email"_ustr,
            u"Drawings & graphics"_ustr,
            u"Databases & tables"_ustr,
            u"Sign & send docs"_ustr,
            u"View and edit PDFs"_ustr,
            u"Math equations"_ustr
        };

        const OUString sComingSoon = u"Coming soon"_ustr;
        rRenderContext.SetFont(MakeUiFont(Sx(10, mfScale), WEIGHT_SEMIBOLD));
        const tools::Long nBadgeW = rRenderContext.GetTextWidth(sComingSoon) + Sx(14, mfScale);
        const tools::Long nBadgeH = Sx(19, mfScale);

        for (int i = 0; i < CARD_COUNT; ++i)
        {
            const tools::Rectangle aCard = GetCardRect(i);
            const bool bComingSoon = IsComingSoonCard(i);
            const bool bEnabled = bComingSoon || maEnabled[i];
            const bool bHover = IsCardInteractive(i) && (i == mnHoveredCard);
            const bool bFocus = IsCardInteractive(i) && HasFocus() && (i == mnFocusedCard);

            // Subtle drop elevation under card + crisp rounded white card
            tools::Rectangle aShadow(Point(aCard.Left() + Sx(1, mfScale), aCard.Top() + Sx(2, mfScale)), Size(aCard.GetWidth(), aCard.GetHeight()));
            DrawRoundedBox(rRenderContext, aShadow, 8.0 * mfScale, Color(0xEC, 0xF0, 0xF5), COL_TRANSPARENT, 0.0);

            DrawRoundedBox(rRenderContext, aCard, 8.0 * mfScale,
                           bHover ? COL_SURFACE_SUBTLE : COL_SURFACE_WHITE,
                           (bHover || bFocus) ? COL_BORDER_HOVER : COL_BORDER, 1.2 * mfScale);

            // Crisp 4px colored bottom accent strip inside card
            const tools::Long nBarH = std::max<tools::Long>(3, Sx(4, mfScale));
            tools::Rectangle aBar(Point(aCard.Left() + Sx(2, mfScale), aCard.Bottom() - nBarH),
                                  Size(std::max<tools::Long>(10, aCard.GetWidth() - Sx(4, mfScale)), nBarH));
            DrawRoundedBox(rRenderContext, aBar, 2.0 * mfScale, bEnabled ? aAccents[i] : COL_TEXT_SUBTLE, COL_TRANSPARENT, 0.0);

            if (bComingSoon)
            {
                const tools::Rectangle aBadge(
                    Point(aCard.Right() - nBadgeW - Sx(8, mfScale), aCard.Top() + Sx(8, mfScale)),
                    Size(nBadgeW, nBadgeH));
                DrawRoundedBox(rRenderContext, aBadge, 9.0 * mfScale, COL_BADGE_BG, COL_TRANSPARENT, 0.0);
                rRenderContext.SetFont(MakeUiFont(Sx(10, mfScale), WEIGHT_SEMIBOLD));
                rRenderContext.SetTextColor(COL_BADGE_TEXT);
                rRenderContext.DrawText(aBadge, sComingSoon, DrawTextFlags::Center | DrawTextFlags::VCenter);
            }

            const int nCx = static_cast<int>((aCard.Left() + aCard.Right()) / 2);
            const int nCy = static_cast<int>(aCard.Top() + aCard.GetHeight() * 0.34);
            DrawCardIcon(rRenderContext, i, nCx, nCy, bEnabled ? aAccents[i] : COL_TEXT_SUBTLE);

            const tools::Long nTextPad = Sx(8, mfScale);
            const tools::Long nMaxTextW = std::max<tools::Long>(40, aCard.GetWidth() - 2 * nTextPad);

            rRenderContext.SetFont(MakeUiFont(Sx(15, mfScale), WEIGHT_BOLD));
            rRenderContext.SetTextColor(bEnabled ? COL_TEXT_PRIMARY : COL_TEXT_SUBTLE);
            const OUString sDispTitle = rRenderContext.GetEllipsisString(aTitles[i], nMaxTextW, DrawTextFlags::EndEllipsis);
            rRenderContext.DrawText(
                tools::Rectangle(Point(aCard.Left() + nTextPad, aCard.Top() + static_cast<tools::Long>(aCard.GetHeight() * 0.58)),
                                 Size(nMaxTextW, Sx(20, mfScale))),
                sDispTitle, DrawTextFlags::Center | DrawTextFlags::VCenter | DrawTextFlags::Clip);

            rRenderContext.SetFont(MakeUiFont(Sx(12, mfScale), WEIGHT_NORMAL));
            rRenderContext.SetTextColor(COL_TEXT_MUTED);
            const OUString& rRawSub = (aCard.GetWidth() < Sx(210, mfScale)) ? aSubsCompact[i] : aSubsWide[i];
            const OUString sDispSub = rRenderContext.GetEllipsisString(rRawSub, nMaxTextW, DrawTextFlags::EndEllipsis);
            rRenderContext.DrawText(
                tools::Rectangle(Point(aCard.Left() + nTextPad, aCard.Top() + static_cast<tools::Long>(aCard.GetHeight() * 0.76)),
                                 Size(nMaxTextW, Sx(18, mfScale))),
                sDispSub, DrawTextFlags::Center | DrawTextFlags::VCenter | DrawTextFlags::Clip);
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
    double mfScale = 1.0;

    tools::Rectangle GetLinkRect() const
    {
        const Size aSz = GetOutputSizePixel();
        return tools::Rectangle(Point(std::max<tools::Long>(Sx(160, mfScale), aSz.Width() - Sx(112, mfScale)), Sx(4, mfScale)),
                                Size(Sx(88, mfScale), std::max<tools::Long>(Sx(20, mfScale), aSz.Height() - Sx(8, mfScale))));
    }

public:
    explicit RecentHeaderWidget(BackingWindow& rOwner)
        : mrOwner(rOwner)
    {
    }

    virtual void SetDrawingArea(weld::DrawingArea* pDrawingArea) override
    {
        weld::CustomWidgetController::SetDrawingArea(pDrawingArea);
        mfScale = GetScale(pDrawingArea);
        pDrawingArea->get_ref_device().SetBackground(Wallpaper(COL_SURFACE_WHITE));
        pDrawingArea->set_size_request(-1, Sx(34, mfScale));
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
        mfScale = GetScale(rRenderContext);
        const Size aSz = GetOutputSizePixel();
        FillSolidRect(rRenderContext, tools::Rectangle(Point(0, 0), aSz), COL_SURFACE_WHITE);

        rRenderContext.SetFont(MakeUiFont(Sx(16, mfScale), WEIGHT_BOLD));
        rRenderContext.SetTextColor(COL_TEXT_PRIMARY);
        rRenderContext.DrawText(tools::Rectangle(Point(Sx(24, mfScale), Sx(2, mfScale)), Size(Sx(240, mfScale), aSz.Height() - Sx(4, mfScale))), u"Recent Files"_ustr,
                                DrawTextFlags::Left | DrawTextFlags::VCenter);

        rRenderContext.SetFont(MakeUiFont(Sx(13, mfScale), mbHoverLink ? WEIGHT_BOLD : WEIGHT_SEMIBOLD));
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
    double mfScale = 1.0;

    tools::Long HeaderHeight() const { return Sx(32, mfScale); }
    tools::Long RowHeight() const { return Sx(40, mfScale); }

    tools::Rectangle GetCardRect() const
    {
        const Size aSz = GetOutputSizePixel();
        const tools::Long nPadX = Sx(24, mfScale);
        const tools::Long nW = std::max<tools::Long>(Sx(200, mfScale), aSz.Width() - 2 * nPadX);
        const tools::Long nMaxH = std::max<tools::Long>(Sx(80, mfScale), aSz.Height() - Sx(14, mfScale));
        const tools::Long nHdrH = HeaderHeight();
        const tools::Long nRowH = RowHeight();
        const size_t nFit = static_cast<size_t>(std::max<tools::Long>(1, (nMaxH - nHdrH) / nRowH));
        const size_t nVisibleRows = maRows.empty() ? 1 : std::min(maRows.size(), nFit);
        const tools::Long nH = maRows.empty() ? std::min<tools::Long>(nMaxH, Sx(82, mfScale))
                                              : std::min<tools::Long>(nMaxH, nHdrH + static_cast<tools::Long>(nVisibleRows) * nRowH);
        return tools::Rectangle(Point(nPadX, Sx(2, mfScale)), Size(nW, nH));
    }

    size_t VisibleRowCount() const
    {
        if (maRows.empty())
            return 0;
        const tools::Rectangle aCard = GetCardRect();
        return static_cast<size_t>(std::max<tools::Long>(0, (aCard.GetHeight() - HeaderHeight()) / RowHeight()));
    }

    tools::Rectangle GetRowRect(size_t i) const
    {
        const tools::Rectangle aCard = GetCardRect();
        const tools::Long nRowH = RowHeight();
        return tools::Rectangle(
            Point(aCard.Left() + 1, aCard.Top() + HeaderHeight() + static_cast<tools::Long>(i) * nRowH),
            Size(aCard.GetWidth() - 2, nRowH));
    }

    tools::Rectangle GetRemoveRect(size_t i) const
    {
        const tools::Rectangle aRow = GetRowRect(i);
        return tools::Rectangle(Point(aRow.Right() - Sx(34, mfScale), aRow.Top() + Sx(5, mfScale)),
                                Size(Sx(28, mfScale), aRow.GetHeight() - Sx(10, mfScale)));
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
        mfScale = GetScale(pDrawingArea);
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
        mfScale = GetScale(rRenderContext);

        const Size aSz = GetOutputSizePixel();
        FillSolidRect(rRenderContext, tools::Rectangle(Point(0, 0), aSz), COL_SURFACE_WHITE);

        const tools::Long nHdrH = HeaderHeight();
        const tools::Long nRowH = RowHeight();
        const tools::Rectangle aCard = GetCardRect();
        DrawRoundedBox(rRenderContext, aCard, 8.0 * mfScale, COL_SURFACE_WHITE, COL_BORDER, 1.2 * mfScale);

        // Header strip
        tools::Rectangle aHdr(Point(aCard.Left() + 1, aCard.Top() + 1), Size(aCard.GetWidth() - 2, nHdrH - 1));
        DrawRoundedBox(rRenderContext, aHdr, 6.0 * mfScale, COL_SURFACE_SUBTLE, COL_TRANSPARENT, 0.0);
        DrawStrokePoly(rRenderContext, { { static_cast<double>(aCard.Left() + 1), static_cast<double>(aCard.Top() + nHdrH) },
                                         { static_cast<double>(aCard.Right() - 1), static_cast<double>(aCard.Top() + nHdrH) } },
                       false, COL_BORDER, 1.0);

        const tools::Long nW = aCard.GetWidth();
        const tools::Long nColNameX = aCard.Left() + Sx(18, mfScale);
        const tools::Long nColLocX = aCard.Left() + static_cast<tools::Long>(nW * 0.36);
        const tools::Long nColModX = aCard.Left() + static_cast<tools::Long>(nW * 0.72);

        rRenderContext.SetFont(MakeUiFont(Sx(12, mfScale), WEIGHT_SEMIBOLD));
        rRenderContext.SetTextColor(COL_TEXT_MUTED);
        rRenderContext.DrawText(tools::Rectangle(Point(nColNameX, aHdr.Top()), Size(nColLocX - nColNameX - Sx(8, mfScale), nHdrH)),
                                u"Name"_ustr, DrawTextFlags::Left | DrawTextFlags::VCenter);
        rRenderContext.DrawText(tools::Rectangle(Point(nColLocX, aHdr.Top()), Size(nColModX - nColLocX - Sx(8, mfScale), nHdrH)),
                                u"Location"_ustr, DrawTextFlags::Left | DrawTextFlags::VCenter);
        rRenderContext.DrawText(tools::Rectangle(Point(nColModX, aHdr.Top()), Size(std::max<tools::Long>(40, aCard.Right() - nColModX - Sx(38, mfScale)), nHdrH)),
                                u"Last Modified"_ustr, DrawTextFlags::Left | DrawTextFlags::VCenter);

        if (maRows.empty())
        {
            rRenderContext.SetFont(MakeUiFont(Sx(13, mfScale), WEIGHT_NORMAL, true));
            rRenderContext.SetTextColor(COL_TEXT_SUBTLE);
            tools::Rectangle aEmpty(Point(aCard.Left() + Sx(16, mfScale), aCard.Top() + nHdrH),
                                    Size(aCard.GetWidth() - Sx(32, mfScale), aCard.GetHeight() - nHdrH));
            rRenderContext.DrawText(aEmpty, u"Your recent files will appear here."_ustr,
                                    DrawTextFlags::Center | DrawTextFlags::VCenter | DrawTextFlags::EndEllipsis);
            return;
        }

        const size_t nVis = std::min(maRows.size(), VisibleRowCount());
        const tools::Long nMiniIcon = Sx(20, mfScale);
        for (size_t i = 0; i < nVis; ++i)
        {
            const RecentRow& rRow = maRows[i];
            const tools::Rectangle aRow = GetRowRect(i);
            const bool bHover = (static_cast<int>(i) == mnHoveredRow);
            const bool bFocus = HasFocus() && (static_cast<int>(i) == mnFocusedRow);

            if (bHover || bFocus)
                FillSolidRect(rRenderContext, aRow, COL_SURFACE_HOVER);

            if (i + 1 < nVis)
                DrawStrokePoly(rRenderContext, { { static_cast<double>(aRow.Left() + Sx(12, mfScale)), static_cast<double>(aRow.Bottom()) },
                                                 { static_cast<double>(aRow.Right() - Sx(12, mfScale)), static_cast<double>(aRow.Bottom()) } },
                               false, Color(0xEC, 0xF0, 0xF5), 1.0);

            const int nIconY = static_cast<int>(aRow.Top() + (nRowH - nMiniIcon) / 2);
            DrawMiniDocIcon(rRenderContext, nColNameX, nIconY, nMiniIcon, rRow.sExt);

            const tools::Long nNameMaxW = std::max<tools::Long>(Sx(40, mfScale), nColLocX - nColNameX - Sx(42, mfScale));
            const OUString sDispName = rRenderContext.GetEllipsisString(rRow.sName, nNameMaxW, DrawTextFlags::EndEllipsis);
            rRenderContext.SetFont(MakeUiFont(Sx(13, mfScale), WEIGHT_SEMIBOLD));
            rRenderContext.SetTextColor(COL_TEXT_PRIMARY);
            rRenderContext.DrawText(
                tools::Rectangle(Point(nColNameX + Sx(28, mfScale), aRow.Top()), Size(nNameMaxW, nRowH)),
                sDispName, DrawTextFlags::Left | DrawTextFlags::VCenter | DrawTextFlags::Clip);

            const double fFolderTop = aRow.Top() + nRowH * 0.36;
            DrawStrokePoly(rRenderContext, {
                { static_cast<double>(nColLocX), fFolderTop },
                { static_cast<double>(nColLocX + 4.5 * mfScale), fFolderTop },
                { static_cast<double>(nColLocX + 6.0 * mfScale), fFolderTop + 2.0 * mfScale },
                { static_cast<double>(nColLocX + 13.0 * mfScale), fFolderTop + 2.0 * mfScale },
                { static_cast<double>(nColLocX + 13.0 * mfScale), fFolderTop + 10.0 * mfScale },
                { static_cast<double>(nColLocX), fFolderTop + 10.0 * mfScale }
            }, true, COL_TEXT_SUBTLE, 1.3 * mfScale);

            const tools::Long nLocMaxW = std::max<tools::Long>(Sx(40, mfScale), nColModX - nColLocX - Sx(38, mfScale));
            const tools::Long nModMaxW = std::max<tools::Long>(Sx(40, mfScale), aCard.Right() - nColModX - Sx(44, mfScale));
            rRenderContext.SetFont(MakeUiFont(Sx(12, mfScale), WEIGHT_NORMAL));
            rRenderContext.SetTextColor(COL_TEXT_MUTED);
            const OUString sDispLoc = rRenderContext.GetEllipsisString(rRow.sLocation, nLocMaxW, DrawTextFlags::PathEllipsis);
            rRenderContext.DrawText(
                tools::Rectangle(Point(nColLocX + Sx(20, mfScale), aRow.Top()), Size(nLocMaxW, nRowH)),
                sDispLoc, DrawTextFlags::Left | DrawTextFlags::VCenter | DrawTextFlags::Clip);

            const OUString sDispMod = rRenderContext.GetEllipsisString(rRow.sModified, nModMaxW, DrawTextFlags::EndEllipsis);
            rRenderContext.DrawText(
                tools::Rectangle(Point(nColModX, aRow.Top()), Size(nModMaxW, nRowH)),
                sDispMod, DrawTextFlags::Left | DrawTextFlags::VCenter | DrawTextFlags::Clip);

            const tools::Rectangle aRemRect = GetRemoveRect(i);
            const Color aDotCol = (bHover && mbHoveredRemove) ? COL_RED_BRAND : COL_TEXT_SUBTLE;
            const tools::Long nDotX = (aRemRect.Left() + aRemRect.Right()) / 2;
            const tools::Long nDotY = (aRemRect.Top() + aRemRect.Bottom()) / 2;
            const tools::Long nDotSz = std::max<tools::Long>(2, Sx(3, mfScale));
            const tools::Long nDotStep = Sx(4, mfScale);
            rRenderContext.SetLineColor();
            rRenderContext.SetFillColor(aDotCol);
            for (tools::Long d = -nDotStep; d <= nDotStep; d += nDotStep)
                rRenderContext.DrawRect(tools::Rectangle(Point(nDotX - nDotSz / 2, nDotY + d - nDotSz / 2), Size(nDotSz, nDotSz)));
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
    double mfScale = 1.0;

public:
    explicit FooterWidget(BackingWindow& rOwner)
        : mrOwner(rOwner)
    {
    }

    virtual void SetDrawingArea(weld::DrawingArea* pDrawingArea) override
    {
        weld::CustomWidgetController::SetDrawingArea(pDrawingArea);
        mfScale = GetScale(pDrawingArea);
        pDrawingArea->get_ref_device().SetBackground(Wallpaper(COL_FOOTER_BG));
        pDrawingArea->set_size_request(-1, Sx(34, mfScale));
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
        mfScale = GetScale(rRenderContext);
        const Size aSz = GetOutputSizePixel();

        FillSolidRect(rRenderContext, tools::Rectangle(Point(0, 0), aSz),
                      mbHovered ? Color(0xEA, 0xEF, 0xF5) : COL_FOOTER_BG);
        DrawStrokePoly(rRenderContext, { { 0.0, 0.0 }, { static_cast<double>(aSz.Width()), 0.0 } },
                       false, COL_BORDER, 1.0);

        // Info (i) badge
        const tools::Long nCy = aSz.Height() / 2;
        const tools::Long nBadgeCx = Sx(24, mfScale);
        const Color aInfoBlue(0x25, 0x63, 0xEB);
        DrawCircleStroke(rRenderContext, static_cast<double>(nBadgeCx), static_cast<double>(nCy), 7.0 * mfScale, aInfoBlue, 1.5 * mfScale);
        FillSolidRect(rRenderContext, tools::Rectangle(Point(nBadgeCx - 1, nCy - Sx(4, mfScale)), Size(Sx(2, mfScale), Sx(2, mfScale))), aInfoBlue);
        FillSolidRect(rRenderContext, tools::Rectangle(Point(nBadgeCx - 1, nCy - Sx(1, mfScale)), Size(Sx(2, mfScale), Sx(6, mfScale))), aInfoBlue);

        const OUString sVer = u"OfficeStudio AI v1.0.0"_ustr;
        rRenderContext.SetFont(MakeUiFont(Sx(12, mfScale), WEIGHT_BOLD));
        rRenderContext.SetTextColor(COL_TEXT_PRIMARY);
        const tools::Long nVerW = rRenderContext.GetTextWidth(sVer);
        const tools::Long nVerX = Sx(40, mfScale);
        rRenderContext.DrawText(tools::Rectangle(Point(nVerX, 0), Size(nVerW + Sx(8, mfScale), aSz.Height())), sVer,
                                DrawTextFlags::Left | DrawTextFlags::VCenter);

        const tools::Long nPipeX = nVerX + nVerW + Sx(12, mfScale);
        DrawStrokePoly(rRenderContext, { { static_cast<double>(nPipeX), static_cast<double>(Sx(9, mfScale)) },
                                         { static_cast<double>(nPipeX), static_cast<double>(aSz.Height() - Sx(9, mfScale)) } },
                       false, Color(0xCB, 0xD5, 0xE1), 1.0);

        rRenderContext.SetFont(MakeUiFont(Sx(12, mfScale), WEIGHT_NORMAL));
        rRenderContext.SetTextColor(COL_TEXT_MUTED);
        rRenderContext.DrawText(tools::Rectangle(Point(nPipeX + Sx(12, mfScale), 0),
                                                 Size(std::max<tools::Long>(Sx(100, mfScale), aSz.Width() - nPipeX - Sx(24, mfScale)), aSz.Height())),
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

    mxFilter->set_active_id(u"filter_none"_ustr);
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
        if (mxFilter->find_id(u"filter_database"_ustr) == -1)
            mxFilter->append(u"filter_database"_ustr, u"Database"_ustr);
        if (mxFilter->find_id(u"filter_math"_ustr) == -1)
            mxFilter->append(u"filter_math"_ustr, u"Math"_ustr);
        if (mxFilter->get_active() < 0)
            mxFilter->set_active_id(u"filter_none"_ustr);

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
        if (mxFilter->get_active_id() == u"filter_database"
            || mxFilter->get_active_id() == u"filter_math")
            mxFilter->set_active_id(u"filter_none"_ustr);
        if (mxFilter->find_id(u"filter_database"_ustr) != -1)
            mxFilter->remove_id(u"filter_database"_ustr);
        if (mxFilter->find_id(u"filter_math"_ustr) != -1)
            mxFilter->remove_id(u"filter_math"_ustr);

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
    else if (nIndex == 4)
        dispatchURL(u"private:factory/sdraw"_ustr);
    else if (nIndex == 5)
        dispatchURL(u"private:factory/sdatabase?Interactive"_ustr);
    else if (nIndex == 8)
        dispatchURL(u"private:factory/smath"_ustr);
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
    mxActionsGrid->set_background(COL_SURFACE_WHITE);
    SetBackground(COL_SURFACE_WHITE);

    set_width_request(880);
    set_height_request(640);

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
        mxCreateCards->SetModuleStates(false, false, false, false, false, false);
        return;
    }

    SvtModuleOptions aModuleOpt;
    mxCreateCards->SetModuleStates(
        aModuleOpt.IsWriterInstalled(),
        aModuleOpt.IsCalcInstalled(),
        aModuleOpt.IsImpressInstalled(),
        aModuleOpt.IsDrawInstalled(),
        aModuleOpt.IsDataBaseInstalled(),
        aModuleOpt.IsMathInstalled());
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
    const OUString sFilterId = mxFilter->get_active_id();
    if (mxLocalView->IsVisible())
    {
        FILTER_APPLICATION aFilter = FILTER_APPLICATION::NONE;
        if (sFilterId == u"filter_writer")
            aFilter = FILTER_APPLICATION::WRITER;
        else if (sFilterId == u"filter_calc")
            aFilter = FILTER_APPLICATION::CALC;
        else if (sFilterId == u"filter_impress")
            aFilter = FILTER_APPLICATION::IMPRESS;
        else if (sFilterId == u"filter_draw")
            aFilter = FILTER_APPLICATION::DRAW;
        mxLocalView->filterItems(ViewFilter_Application(aFilter));
    }
    else
    {
        sfx2::ApplicationType aFilter = sfx2::ApplicationType::TYPE_NONE;
        if (sFilterId == u"filter_writer")
            aFilter = sfx2::ApplicationType::TYPE_WRITER;
        else if (sFilterId == u"filter_calc")
            aFilter = sfx2::ApplicationType::TYPE_CALC;
        else if (sFilterId == u"filter_impress")
            aFilter = sfx2::ApplicationType::TYPE_IMPRESS;
        else if (sFilterId == u"filter_draw")
            aFilter = sfx2::ApplicationType::TYPE_DRAW;
        else if (sFilterId == u"filter_database")
            aFilter = sfx2::ApplicationType::TYPE_DATABASE;
        else if (sFilterId == u"filter_math")
            aFilter = sfx2::ApplicationType::TYPE_MATH;
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
        triggerCreateApp(4);
    else if (&rButton == mxDBAllButton.get())
        triggerCreateApp(5);
    else if (&rButton == mxMathAllButton.get())
        triggerCreateApp(8);
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
