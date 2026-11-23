/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <notebookbar/CalcBackstageView.hxx>

#include <algorithm>
#include <cmath>
#include <memory>

#include <basegfx/polygon/b2dpolygon.hxx>
#include <basegfx/polygon/b2dpolygontools.hxx>
#include <basegfx/range/b2drectangle.hxx>
#include <com/sun/star/document/MacroExecMode.hpp>
#include <com/sun/star/document/UpdateDocMode.hpp>
#include <com/sun/star/frame/Desktop.hpp>
#include <com/sun/star/frame/XDispatch.hpp>
#include <com/sun/star/frame/XDispatchProvider.hpp>
#include <com/sun/star/task/InteractionHandler.hpp>
#include <com/sun/star/util/URL.hpp>
#include <com/sun/star/util/URLTransformer.hpp>
#include <comphelper/dispatchcommand.hxx>
#include <comphelper/processfactory.hxx>
#include <comphelper/propertyvalue.hxx>
#include <sfx2/objsh.hxx>
#include <sfx2/viewfrm.hxx>
#include <tools/urlobj.hxx>
#include <unotools/historyoptions.hxx>
#include <vcl/OfficeStudioTheme.hxx>
#include <vcl/commandevent.hxx>
#include <vcl/event.hxx>
#include <vcl/ptrstyle.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/rendercontext/DrawTextFlags.hxx>
#include <vcl/svapp.hxx>

using namespace css;

namespace
{

struct DelayedBackstageAction
{
    uno::Reference<frame::XFrame> xFrame;
    OUString sCommand;
    OUString sFileUrl;
    bool bReadOnly = false;
};

void ExecuteDelayedBackstageAction(void*, void* pArg)
{
    std::unique_ptr<DelayedBackstageAction> pAction(static_cast<DelayedBackstageAction*>(pArg));
    if (!pAction)
        return;

    const uno::Reference<uno::XComponentContext>& xContext = comphelper::getProcessComponentContext();
    try
    {
        if (!pAction->sFileUrl.isEmpty())
        {
            uno::Reference<frame::XDesktop2> xDesktop = frame::Desktop::create(xContext);
            uno::Reference<util::XURLTransformer> xTrans = util::URLTransformer::create(xContext);
            util::URL aURL;
            aURL.Complete = pAction->sFileUrl;
            xTrans->parseStrict(aURL);

            std::vector<beans::PropertyValue> aArgs{
                comphelper::makePropertyValue(u"Referer"_ustr, u"private:user"_ustr),
                comphelper::makePropertyValue(u"AsTemplate"_ustr, false),
                comphelper::makePropertyValue(u"MacroExecutionMode"_ustr, document::MacroExecMode::USE_CONFIG),
                comphelper::makePropertyValue(u"UpdateDocMode"_ustr, document::UpdateDocMode::ACCORDING_TO_CONFIG),
                comphelper::makePropertyValue(u"InteractionHandler"_ustr,
                                              task::InteractionHandler::createWithParent(xContext, nullptr))
            };
            if (pAction->bReadOnly)
                aArgs.push_back(comphelper::makePropertyValue(u"ReadOnly"_ustr, true));

            uno::Sequence<beans::PropertyValue> aSeq(aArgs.data(), static_cast<sal_Int32>(aArgs.size()));
            uno::Reference<frame::XDispatch> xDispatch = xDesktop->queryDispatch(aURL, u"_default"_ustr, 0);
            if (xDispatch.is())
                xDispatch->dispatch(aURL, aSeq);
        }
        else if (!pAction->sCommand.isEmpty())
        {
            bool bDispatched = false;
            if (pAction->xFrame.is())
            {
                uno::Reference<util::XURLTransformer> xTrans = util::URLTransformer::create(xContext);
                util::URL aURL;
                aURL.Complete = pAction->sCommand;
                xTrans->parseStrict(aURL);
                uno::Reference<frame::XDispatchProvider> xProvider(pAction->xFrame, uno::UNO_QUERY);
                if (xProvider.is())
                {
                    uno::Reference<frame::XDispatch> xDispatch = xProvider->queryDispatch(aURL, OUString(), 0);
                    if (xDispatch.is())
                    {
                        xDispatch->dispatch(aURL, uno::Sequence<beans::PropertyValue>());
                        bDispatched = true;
                    }
                }
            }
            if (!bDispatched)
                comphelper::dispatchCommand(pAction->sCommand, uno::Sequence<beans::PropertyValue>());
        }
    }
    catch (const uno::Exception&)
    {
    }
}

bool IsCalcExtension(const OUString& rExt)
{
    const OUString sLower = rExt.toAsciiLowerCase();
    return sLower == u"ods" || sLower == u"fods" || sLower == u"ots"
        || sLower == u"xlsx" || sLower == u"xlsm" || sLower == u"xls"
        || sLower == u"xltx" || sLower == u"xlt" || sLower == u"csv"
        || sLower == u"tsv";
}

basegfx::B2DPolygon MakeRoundRect(const tools::Rectangle& rRect, double fRadius)
{
    const double w = std::max<double>(1.0, rRect.GetWidth());
    const double h = std::max<double>(1.0, rRect.GetHeight());
    const double rx = std::clamp((2.0 * fRadius) / w, 0.0, 1.0);
    const double ry = std::clamp((2.0 * fRadius) / h, 0.0, 1.0);
    return basegfx::utils::createPolygonFromRect(
        basegfx::B2DRectangle(rRect.Left(), rRect.Top(), rRect.Right(), rRect.Bottom()), rx, ry);
}

void DrawBox(vcl::RenderContext& rRenderContext, const tools::Rectangle& rRect, double fRadius,
             Color aFill, Color aBorder, double fStrokeWidth = 1.0)
{
    const basegfx::B2DPolygon aPoly = MakeRoundRect(rRect, fRadius);
    if (aFill != COL_TRANSPARENT)
    {
        rRenderContext.SetLineColor();
        rRenderContext.SetFillColor(aFill);
        rRenderContext.DrawPolygon(aPoly);
    }
    if (aBorder != COL_TRANSPARENT && fStrokeWidth > 0.0)
    {
        rRenderContext.SetFillColor();
        rRenderContext.SetLineColor(aBorder);
        rRenderContext.DrawPolyLine(aPoly, fStrokeWidth, basegfx::B2DLineJoin::Round);
    }
}

void DrawLineSeg(vcl::RenderContext& rCtx, double x1, double y1, double x2, double y2,
                 Color aColor, double fWidth)
{
    basegfx::B2DPolygon aPoly;
    aPoly.append(basegfx::B2DPoint(x1, y1));
    aPoly.append(basegfx::B2DPoint(x2, y2));
    rCtx.SetFillColor();
    rCtx.SetLineColor(aColor);
    rCtx.DrawPolyLine(aPoly, fWidth, basegfx::B2DLineJoin::Round, css::drawing::LineCap_ROUND);
}

enum class NavGlyph
{
    Back,
    Recent,
    New,
    Open,
    Save,
    SaveAs,
    Print,
    Export,
    Properties,
    Options,
    BlankSheet,
    Template,
    Folder,
    Pdf,
    Mail
};

void DrawVectorGlyph(vcl::RenderContext& rCtx, NavGlyph eGlyph, double cx, double cy,
                     double r, Color aColor, double fStroke)
{
    rCtx.SetFillColor();
    rCtx.SetLineColor(aColor);
    switch (eGlyph)
    {
        case NavGlyph::Back:
        {
            basegfx::B2DPolygon aCircle = basegfx::utils::createPolygonFromCircle(basegfx::B2DPoint(cx, cy), r * 1.15);
            rCtx.DrawPolyLine(aCircle, fStroke * 0.9, basegfx::B2DLineJoin::Round);
            DrawLineSeg(rCtx, cx + r * 0.45, cy, cx - r * 0.45, cy, aColor, fStroke);
            basegfx::B2DPolygon aTip;
            aTip.append(basegfx::B2DPoint(cx - r * 0.08, cy - r * 0.38));
            aTip.append(basegfx::B2DPoint(cx - r * 0.45, cy));
            aTip.append(basegfx::B2DPoint(cx - r * 0.08, cy + r * 0.38));
            rCtx.DrawPolyLine(aTip, fStroke, basegfx::B2DLineJoin::Round, css::drawing::LineCap_ROUND);
            break;
        }
        case NavGlyph::Recent:
        {
            basegfx::B2DPolygon aClock = basegfx::utils::createPolygonFromCircle(basegfx::B2DPoint(cx, cy), r * 0.9);
            rCtx.DrawPolyLine(aClock, fStroke, basegfx::B2DLineJoin::Round);
            basegfx::B2DPolygon aHands;
            aHands.append(basegfx::B2DPoint(cx, cy - r * 0.48));
            aHands.append(basegfx::B2DPoint(cx, cy));
            aHands.append(basegfx::B2DPoint(cx + r * 0.38, cy + r * 0.24));
            rCtx.DrawPolyLine(aHands, fStroke, basegfx::B2DLineJoin::Round, css::drawing::LineCap_ROUND);
            break;
        }
        case NavGlyph::New:
        case NavGlyph::BlankSheet:
        {
            tools::Rectangle aDoc(Point(std::lround(cx - r * 0.72), std::lround(cy - r * 0.85)),
                                  Size(std::lround(r * 1.44), std::lround(r * 1.7)));
            DrawBox(rCtx, aDoc, r * 0.2, COL_TRANSPARENT, aColor, fStroke);
            DrawLineSeg(rCtx, cx - r * 0.35, cy, cx + r * 0.35, cy, aColor, fStroke);
            DrawLineSeg(rCtx, cx, cy - r * 0.35, cx, cy + r * 0.35, aColor, fStroke);
            break;
        }
        case NavGlyph::Open:
        case NavGlyph::Folder:
        {
            basegfx::B2DPolygon aFolder;
            aFolder.append(basegfx::B2DPoint(cx - r * 0.9, cy - r * 0.55));
            aFolder.append(basegfx::B2DPoint(cx - r * 0.25, cy - r * 0.55));
            aFolder.append(basegfx::B2DPoint(cx - r * 0.05, cy - r * 0.3));
            aFolder.append(basegfx::B2DPoint(cx + r * 0.9, cy - r * 0.3));
            aFolder.append(basegfx::B2DPoint(cx + r * 0.9, cy + r * 0.65));
            aFolder.append(basegfx::B2DPoint(cx - r * 0.9, cy + r * 0.65));
            aFolder.setClosed(true);
            rCtx.DrawPolyLine(aFolder, fStroke, basegfx::B2DLineJoin::Round);
            break;
        }
        case NavGlyph::Save:
        case NavGlyph::SaveAs:
        {
            tools::Rectangle aDisk(Point(std::lround(cx - r * 0.8), std::lround(cy - r * 0.8)),
                                   Size(std::lround(r * 1.6), std::lround(r * 1.6)));
            DrawBox(rCtx, aDisk, r * 0.2, COL_TRANSPARENT, aColor, fStroke);
            tools::Rectangle aSlot(Point(std::lround(cx - r * 0.42), std::lround(cy - r * 0.8)),
                                   Size(std::lround(r * 0.84), std::lround(r * 0.55)));
            DrawBox(rCtx, aSlot, r * 0.08, COL_TRANSPARENT, aColor, fStroke * 0.85);
            tools::Rectangle aLabel(Point(std::lround(cx - r * 0.48), std::lround(cy + r * 0.15)),
                                    Size(std::lround(r * 0.96), std::lround(r * 0.62)));
            DrawBox(rCtx, aLabel, r * 0.08, COL_TRANSPARENT, aColor, fStroke * 0.85);
            break;
        }
        case NavGlyph::Print:
        {
            tools::Rectangle aBody(Point(std::lround(cx - r * 0.85), std::lround(cy - r * 0.3)),
                                   Size(std::lround(r * 1.7), std::lround(r * 0.85)));
            DrawBox(rCtx, aBody, r * 0.18, COL_TRANSPARENT, aColor, fStroke);
            tools::Rectangle aTop(Point(std::lround(cx - r * 0.5), std::lround(cy - r * 0.8)),
                                  Size(std::lround(r * 1.0), std::lround(r * 0.5)));
            DrawBox(rCtx, aTop, r * 0.08, COL_TRANSPARENT, aColor, fStroke * 0.85);
            tools::Rectangle aOut(Point(std::lround(cx - r * 0.5), std::lround(cy + r * 0.2)),
                                  Size(std::lround(r * 1.0), std::lround(r * 0.65)));
            DrawBox(rCtx, aOut, r * 0.08, COL_TRANSPARENT, aColor, fStroke * 0.85);
            break;
        }
        case NavGlyph::Export:
        case NavGlyph::Pdf:
        {
            tools::Rectangle aPage(Point(std::lround(cx - r * 0.72), std::lround(cy - r * 0.82)),
                                   Size(std::lround(r * 1.44), std::lround(r * 1.64)));
            DrawBox(rCtx, aPage, r * 0.2, COL_TRANSPARENT, aColor, fStroke);
            DrawLineSeg(rCtx, cx - r * 0.1, cy, cx + r * 0.85, cy, aColor, fStroke);
            basegfx::B2DPolygon aArrow;
            aArrow.append(basegfx::B2DPoint(cx + r * 0.48, cy - r * 0.34));
            aArrow.append(basegfx::B2DPoint(cx + r * 0.85, cy));
            aArrow.append(basegfx::B2DPoint(cx + r * 0.48, cy + r * 0.34));
            rCtx.DrawPolyLine(aArrow, fStroke, basegfx::B2DLineJoin::Round, css::drawing::LineCap_ROUND);
            break;
        }
        case NavGlyph::Properties:
        case NavGlyph::Template:
        {
            tools::Rectangle aPage(Point(std::lround(cx - r * 0.72), std::lround(cy - r * 0.82)),
                                   Size(std::lround(r * 1.44), std::lround(r * 1.64)));
            DrawBox(rCtx, aPage, r * 0.2, COL_TRANSPARENT, aColor, fStroke);
            DrawLineSeg(rCtx, cx - r * 0.38, cy - r * 0.3, cx + r * 0.38, cy - r * 0.3, aColor, fStroke * 0.9);
            DrawLineSeg(rCtx, cx - r * 0.38, cy + r * 0.08, cx + r * 0.38, cy + r * 0.08, aColor, fStroke * 0.9);
            DrawLineSeg(rCtx, cx - r * 0.38, cy + r * 0.44, cx + r * 0.16, cy + r * 0.44, aColor, fStroke * 0.9);
            break;
        }
        case NavGlyph::Options:
        {
            basegfx::B2DPolygon aOuter = basegfx::utils::createPolygonFromCircle(basegfx::B2DPoint(cx, cy), r * 0.72);
            rCtx.DrawPolyLine(aOuter, fStroke, basegfx::B2DLineJoin::Round);
            basegfx::B2DPolygon aInner = basegfx::utils::createPolygonFromCircle(basegfx::B2DPoint(cx, cy), r * 0.28);
            rCtx.DrawPolyLine(aInner, fStroke * 0.9, basegfx::B2DLineJoin::Round);
            DrawLineSeg(rCtx, cx, cy - r * 0.96, cx, cy - r * 0.72, aColor, fStroke);
            DrawLineSeg(rCtx, cx, cy + r * 0.72, cx, cy + r * 0.96, aColor, fStroke);
            DrawLineSeg(rCtx, cx - r * 0.96, cy, cx - r * 0.72, cy, aColor, fStroke);
            DrawLineSeg(rCtx, cx + r * 0.72, cy, cx + r * 0.96, cy, aColor, fStroke);
            break;
        }
        case NavGlyph::Mail:
        {
            tools::Rectangle aEnv(Point(std::lround(cx - r * 0.85), std::lround(cy - r * 0.6)),
                                  Size(std::lround(r * 1.7), std::lround(r * 1.2)));
            DrawBox(rCtx, aEnv, r * 0.18, COL_TRANSPARENT, aColor, fStroke);
            basegfx::B2DPolygon aFlap;
            aFlap.append(basegfx::B2DPoint(cx - r * 0.8, cy - r * 0.52));
            aFlap.append(basegfx::B2DPoint(cx, cy + r * 0.1));
            aFlap.append(basegfx::B2DPoint(cx + r * 0.8, cy - r * 0.52));
            rCtx.DrawPolyLine(aFlap, fStroke * 0.9, basegfx::B2DLineJoin::Round);
            break;
        }
    }
}

struct ActionCardDef
{
    const char16_t* pTitle;
    const char16_t* pDesc;
    const char16_t* pCmd;
    NavGlyph eGlyph;
    Color aAccent;
    Color aSoftBg;
};

} // namespace

CalcBackstageView::CalcBackstageView(vcl::Window* pParent,
                                     uno::Reference<frame::XFrame> xFrame,
                                     Link<CalcBackstageView*, void> aCloseHdl)
    : Control(pParent, WB_CLIPCHILDREN | WB_TABSTOP)
    , mxFrame(std::move(xFrame))
    , maCloseHdl(std::move(aCloseHdl))
{
    EnableClipSiblings();
    SetPaintTransparent(false);
    SetBackground(Wallpaper(vcl::officestudio::SurfaceSubtle));
    RefreshData();
}

CalcBackstageView::~CalcBackstageView()
{
    disposeOnce();
}

void CalcBackstageView::dispose()
{
    maHitTargets.clear();
    maRecentEntries.clear();
    Control::dispose();
}

void CalcBackstageView::RefreshData()
{
    msDocTitle = u"Untitled 1"_ustr;
    msDocPath.clear();
    mbDocModified = false;

    if (SfxViewFrame* pViewFrm = SfxViewFrame::Current())
    {
        if (SfxObjectShell* pObjSh = pViewFrm->GetObjectShell())
        {
            const OUString sTitle = pObjSh->GetTitle(SFX_TITLE_DETECT);
            if (!sTitle.isEmpty())
                msDocTitle = sTitle;
            mbDocModified = pObjSh->IsModified();
            if (pObjSh->HasName())
                msDocPath = pObjSh->GetTitle(SFX_TITLE_FULLNAME);
        }
    }

    maRecentEntries.clear();
    std::vector<SvtHistoryOptions::HistoryItem> aHistory = SvtHistoryOptions::GetList(EHistoryType::PickList);
    for (const auto& rItem : aHistory)
    {
        const INetURLObject aURLObj(rItem.sURL);
        RecentEntry aEntry;
        aEntry.sURL = rItem.sURL;
        aEntry.sExt = aURLObj.getExtension().toAsciiUpperCase();
        aEntry.bReadOnly = rItem.isReadOnly;
        aEntry.bPinned = rItem.isPinned;
        aEntry.bIsCalc = IsCalcExtension(aEntry.sExt);
        aEntry.sName = aURLObj.GetLastName(INetURLObject::DecodeMechanism::WithCharset);
        if (aEntry.sName.isEmpty())
            aEntry.sName = aURLObj.GetBase();

        if (aURLObj.GetProtocol() == INetProtocol::File)
        {
            OUString sPath = aURLObj.getFSysPath(FSysStyle::Detect);
            const sal_Int32 nSlash = std::max(sPath.lastIndexOf('\\'), sPath.lastIndexOf('/'));
            aEntry.sFolder = (nSlash > 0) ? sPath.copy(0, nSlash) : sPath;
        }
        else
        {
            aEntry.sFolder = aURLObj.GetURLNoPass();
        }
        maRecentEntries.push_back(std::move(aEntry));
    }

    mnScrollY = 0;
    Invalidate();
}

void CalcBackstageView::SetSection(CalcBackstageSection eSection)
{
    if (meSection != eSection)
    {
        meSection = eSection;
        mnScrollY = 0;
        mnHoverId = -1;
        Invalidate();
    }
}

void CalcBackstageView::PostDispatchCommand(const OUString& rCommand)
{
    auto pAction = std::make_unique<DelayedBackstageAction>();
    pAction->xFrame = mxFrame;
    pAction->sCommand = rCommand;
    maCloseHdl.Call(this);
    if (Application::PostUserEvent(LINK_NONMEMBER(nullptr, ExecuteDelayedBackstageAction), pAction.get()))
        pAction.release();
}

void CalcBackstageView::PostOpenRecentFile(const OUString& rURL, bool bReadOnly)
{
    auto pAction = std::make_unique<DelayedBackstageAction>();
    pAction->xFrame = mxFrame;
    pAction->sFileUrl = rURL;
    pAction->bReadOnly = bReadOnly;
    maCloseHdl.Call(this);
    if (Application::PostUserEvent(LINK_NONMEMBER(nullptr, ExecuteDelayedBackstageAction), pAction.get()))
        pAction.release();
}

const CalcBackstageView::HitTarget* CalcBackstageView::FindHitTarget(const Point& rPos) const
{
    for (auto it = maHitTargets.rbegin(); it != maHitTargets.rend(); ++it)
    {
        if (it->aRect.Contains(rPos))
            return &(*it);
    }
    return nullptr;
}

void CalcBackstageView::Resize()
{
    Control::Resize();
    Invalidate();
}

void CalcBackstageView::RebuildTargets(const Size& rSize, double fScale)
{
    maHitTargets.clear();
    maRecentScrollBounds.SetEmpty();
    mnMaxScrollY = 0;
    if (rSize.IsEmpty())
        return;

    auto sx = [fScale](double v) -> tools::Long {
        return static_cast<tools::Long>(std::lround(v * fScale));
    };

    int nNextId = 1;
    const tools::Long nRailW = sx(220.0);

    // Back button
    maHitTargets.push_back({ nNextId++, TargetKind::CloseBackstage, CalcBackstageSection::Recent,
                             tools::Rectangle(Point(sx(18.0), sx(18.0)), Size(sx(36.0), sx(36.0))),
                             {}, {}, false });

    struct NavItem
    {
        TargetKind eKind;
        CalcBackstageSection eSec;
        const char16_t* pCmd;
    };
    static constexpr NavItem aTopNav[] = {
        { TargetKind::SelectSection,   CalcBackstageSection::Recent, u"" },
        { TargetKind::SelectSection,   CalcBackstageSection::New,    u"" },
        { TargetKind::SelectSection,   CalcBackstageSection::Open,   u"" },
    };
    static constexpr NavItem aMidNav[] = {
        { TargetKind::DispatchCommand, CalcBackstageSection::Recent, u".uno:Save" },
        { TargetKind::SelectSection,   CalcBackstageSection::SaveAs, u"" },
        { TargetKind::SelectSection,   CalcBackstageSection::Print,  u"" },
        { TargetKind::SelectSection,   CalcBackstageSection::Export, u"" },
    };

    tools::Long nY = sx(74.0);
    const tools::Long nItemH = sx(40.0);
    const tools::Long nItemStep = sx(44.0);
    for (const auto& rNav : aTopNav)
    {
        maHitTargets.push_back({ nNextId++, rNav.eKind, rNav.eSec,
                                 tools::Rectangle(Point(sx(10.0), nY), Size(nRailW - sx(20.0), nItemH)),
                                 OUString(rNav.pCmd), {}, false });
        nY += nItemStep;
    }

    nY += sx(16.0);
    for (const auto& rNav : aMidNav)
    {
        maHitTargets.push_back({ nNextId++, rNav.eKind, rNav.eSec,
                                 tools::Rectangle(Point(sx(10.0), nY), Size(nRailW - sx(20.0), nItemH)),
                                 OUString(rNav.pCmd), {}, false });
        nY += nItemStep;
    }

    // Bottom-anchored Properties & Options
    const tools::Long nOptY = std::max(nY + sx(24.0), rSize.Height() - sx(104.0));
    maHitTargets.push_back({ nNextId++, TargetKind::DispatchCommand, CalcBackstageSection::Recent,
                             tools::Rectangle(Point(sx(10.0), nOptY), Size(nRailW - sx(20.0), nItemH)),
                             u".uno:SetDocumentProperties"_ustr, {}, false });
    maHitTargets.push_back({ nNextId++, TargetKind::DispatchCommand, CalcBackstageSection::Recent,
                             tools::Rectangle(Point(sx(10.0), nOptY + nItemStep), Size(nRailW - sx(20.0), nItemH)),
                             u".uno:OptionsTreeDialog"_ustr, {}, false });

    // Content area geometry
    const tools::Long nPadX = sx(36.0);
    const tools::Long nContentX = nRailW + nPadX;
    const tools::Long nContentW = std::min<tools::Long>(sx(960.0), std::max<tools::Long>(sx(300.0), rSize.Width() - nContentX - nPadX));

    auto addCardGrid = [&](tools::Long nStartY, const ActionCardDef* pCards, size_t nCount, int nCols, tools::Long nCardH) -> tools::Long
    {
        const tools::Long nGap = sx(14.0);
        const tools::Long nCardW = std::max<tools::Long>(sx(180.0), (nContentW - nGap * (nCols - 1)) / nCols);
        for (size_t i = 0; i < nCount; ++i)
        {
            const int nRow = static_cast<int>(i) / nCols;
            const int nCol = static_cast<int>(i) % nCols;
            const tools::Long x = nContentX + nCol * (nCardW + nGap);
            const tools::Long y = nStartY + nRow * (nCardH + nGap);
            maHitTargets.push_back({ nNextId++, TargetKind::DispatchCommand, meSection,
                                     tools::Rectangle(Point(x, y), Size(nCardW, nCardH)),
                                     OUString(pCards[i].pCmd), {}, false });
        }
        const int nRows = static_cast<int>((nCount + nCols - 1) / nCols);
        return nStartY + nRows * (nCardH + nGap);
    };

    auto addRecentList = [&](tools::Long nPanelTop)
    {
        const tools::Long nPanelH = std::max<tools::Long>(sx(160.0), rSize.Height() - nPanelTop - sx(28.0));
        const tools::Long nHeaderH = sx(46.0);
        // Filter pills & Browse button
        maHitTargets.push_back({ nNextId++, TargetKind::FilterCalcOnly, meSection,
                                 tools::Rectangle(Point(nContentX + nContentW - sx(276.0), nPanelTop + sx(10.0)), Size(sx(98.0), sx(26.0))),
                                 {}, {}, false });
        maHitTargets.push_back({ nNextId++, TargetKind::FilterAllFiles, meSection,
                                 tools::Rectangle(Point(nContentX + nContentW - sx(172.0), nPanelTop + sx(10.0)), Size(sx(78.0), sx(26.0))),
                                 {}, {}, false });
        maHitTargets.push_back({ nNextId++, TargetKind::DispatchCommand, meSection,
                                 tools::Rectangle(Point(nContentX + nContentW - sx(86.0), nPanelTop + sx(10.0)), Size(sx(72.0), sx(26.0))),
                                 u".uno:OpenFromCalc"_ustr, {}, false });

        maRecentScrollBounds = tools::Rectangle(Point(nContentX + 2, nPanelTop + nHeaderH),
                                                Size(nContentW - 4, std::max<tools::Long>(0, nPanelH - nHeaderH - sx(6.0))));
        std::vector<const RecentEntry*> aFiltered;
        for (const auto& rRec : maRecentEntries)
        {
            if (!mbOnlySpreadsheets || rRec.bIsCalc)
                aFiltered.push_back(&rRec);
        }

        const tools::Long nRowH = sx(50.0);
        const tools::Long nTotalH = static_cast<tools::Long>(aFiltered.size()) * nRowH;
        mnMaxScrollY = std::max<tools::Long>(0, nTotalH - maRecentScrollBounds.GetHeight());
        mnScrollY = std::clamp<tools::Long>(mnScrollY, 0, mnMaxScrollY);

        if (aFiltered.empty())
        {
            const tools::Long nBtnW = sx(140.0);
            const tools::Long nBtnH = sx(34.0);
            maHitTargets.push_back({ nNextId++, TargetKind::DispatchCommand, meSection,
                                     tools::Rectangle(Point(nContentX + (nContentW - nBtnW) / 2,
                                                            maRecentScrollBounds.Top() + sx(86.0)),
                                                      Size(nBtnW, nBtnH)),
                                     u".uno:OpenFromCalc"_ustr, {}, false });
        }
        else
        {
            for (size_t i = 0; i < aFiltered.size(); ++i)
            {
                const tools::Long nRy = maRecentScrollBounds.Top() + static_cast<tools::Long>(i) * nRowH - mnScrollY;
                tools::Rectangle aRowRc(Point(nContentX + sx(10.0), nRy + sx(3.0)),
                                        Size(nContentW - sx(20.0), nRowH - sx(6.0)));
                aRowRc.Intersection(maRecentScrollBounds);
                if (!aRowRc.IsEmpty() && aRowRc.GetHeight() > 6)
                {
                    maHitTargets.push_back({ nNextId++, TargetKind::OpenRecentFile, meSection, aRowRc,
                                             {}, aFiltered[i]->sURL, aFiltered[i]->bReadOnly });
                }
            }
        }
    };

    const tools::Long nBodyTop = sx(94.0);
    switch (meSection)
    {
        case CalcBackstageSection::Recent:
        {
            static constexpr ActionCardDef aQuick[] = {
                { u"Blank workbook", u"Create a new spreadsheet", u".uno:AddDirect", NavGlyph::BlankSheet, vcl::officestudio::CalcAccent, vcl::officestudio::CalcAccentSoft },
                { u"Templates", u"Start from a template", u".uno:NewDoc", NavGlyph::Template, vcl::officestudio::Purple, vcl::officestudio::PurpleTile },
                { u"Open file...", u"Browse this computer", u".uno:OpenFromCalc", NavGlyph::Folder, vcl::officestudio::Amber, vcl::officestudio::AmberTile },
            };
            const tools::Long nNextY = addCardGrid(nBodyTop, aQuick, 3, 3, sx(88.0)) + sx(10.0);
            addRecentList(nNextY);
            break;
        }
        case CalcBackstageSection::New:
        {
            static constexpr ActionCardDef aNewCards[] = {
                { u"Blank workbook", u"Create an empty multi-sheet workbook ready for formulas and data", u".uno:AddDirect", NavGlyph::BlankSheet, vcl::officestudio::CalcAccent, vcl::officestudio::CalcAccentSoft },
                { u"New from template...", u"Choose from installed and custom spreadsheet templates", u".uno:NewDoc", NavGlyph::Template, vcl::officestudio::Purple, vcl::officestudio::PurpleTile },
                { u"Save as template...", u"Store the active workbook structure as a reusable .ots template", u".uno:SaveAsTemplate", NavGlyph::SaveAs, vcl::officestudio::Teal, vcl::officestudio::TealTile },
            };
            addCardGrid(nBodyTop, aNewCards, 3, 2, sx(106.0));
            break;
        }
        case CalcBackstageSection::Open:
        {
            static constexpr ActionCardDef aOpenCards[] = {
                { u"Browse This PC...", u"Open .xlsx, .ods, .xls, or .csv files from local folders", u".uno:OpenFromCalc", NavGlyph::Folder, vcl::officestudio::CalcAccent, vcl::officestudio::CalcAccentSoft },
                { u"Open Remote File...", u"Connect to WebDAV, SharePoint, or CMIS remote storage", u".uno:OpenRemote", NavGlyph::Open, vcl::officestudio::RoyalBlue, vcl::officestudio::RoyalBlueTile },
            };
            const tools::Long nNextY = addCardGrid(nBodyTop, aOpenCards, 2, 2, sx(94.0)) + sx(10.0);
            addRecentList(nNextY);
            break;
        }
        case CalcBackstageSection::SaveAs:
        {
            static constexpr ActionCardDef aSaveCards[] = {
                { u"Save As...", u"Choose folder and format (XLSX, ODS, XLS, CSV)", u".uno:SaveAs", NavGlyph::SaveAs, vcl::officestudio::CalcAccent, vcl::officestudio::CalcAccentSoft },
                { u"Save a Copy...", u"Save an independent copy without leaving the current workbook", u".uno:SaveACopy", NavGlyph::Save, vcl::officestudio::RoyalBlue, vcl::officestudio::RoyalBlueTile },
                { u"Save as Template...", u"Save current layout and styles as a template (.ots)", u".uno:SaveAsTemplate", NavGlyph::Template, vcl::officestudio::Purple, vcl::officestudio::PurpleTile },
                { u"Save Remote...", u"Save this spreadsheet to a configured remote server", u".uno:SaveAsRemote", NavGlyph::Folder, vcl::officestudio::Teal, vcl::officestudio::TealTile },
            };
            addCardGrid(nBodyTop, aSaveCards, 4, 2, sx(106.0));
            break;
        }
        case CalcBackstageSection::Print:
        {
            static constexpr ActionCardDef aPrintCards[] = {
                { u"Print...", u"Configure printer, page range, copies, and scaling (Ctrl+P)", u".uno:Print", NavGlyph::Print, vcl::officestudio::CalcAccent, vcl::officestudio::CalcAccentSoft },
                { u"Print Preview", u"Inspect page breaks, headers, footers, and margins", u".uno:PrintPreview", NavGlyph::Properties, vcl::officestudio::RoyalBlue, vcl::officestudio::RoyalBlueTile },
                { u"Quick Print", u"Send immediately to your default printer", u".uno:PrintDefault", NavGlyph::Print, vcl::officestudio::Teal, vcl::officestudio::TealTile },
                { u"Printer Setup...", u"Select default printer and paper orientation settings", u".uno:PrinterSetup", NavGlyph::Options, vcl::officestudio::Slate700, vcl::officestudio::SurfaceSubtle },
            };
            addCardGrid(nBodyTop, aPrintCards, 4, 2, sx(106.0));
            break;
        }
        case CalcBackstageSection::Export:
        {
            static constexpr ActionCardDef aExportCards[] = {
                { u"Export as PDF...", u"Configure PDF resolution, sheet selection, and security", u".uno:ExportToPDF", NavGlyph::Pdf, vcl::officestudio::Red, vcl::officestudio::RedTile },
                { u"Quick Export Directly as PDF", u"Save a PDF copy with current default settings", u".uno:ExportDirectToPDF", NavGlyph::Export, vcl::officestudio::CalcAccent, vcl::officestudio::CalcAccentSoft },
                { u"Export to Other Format...", u"Export spreadsheet to HTML, XHTML, or other formats", u".uno:ExportTo", NavGlyph::SaveAs, vcl::officestudio::Purple, vcl::officestudio::PurpleTile },
                { u"Send as Email...", u"Attach this spreadsheet to a new email message", u".uno:SendMail", NavGlyph::Mail, vcl::officestudio::RoyalBlue, vcl::officestudio::RoyalBlueTile },
            };
            addCardGrid(nBodyTop, aExportCards, 4, 2, sx(106.0));
            break;
        }
    }
}

void CalcBackstageView::Paint(vcl::RenderContext& rRenderContext, const tools::Rectangle&)
{
    const Size aSize = GetOutputSizePixel();
    if (aSize.IsEmpty())
        return;

    const double fScale = std::max(1.0, static_cast<double>(GetDPIScaleFactor()));
    auto sx = [fScale](double v) -> tools::Long {
        return static_cast<tools::Long>(std::lround(v * fScale));
    };

    RebuildTargets(aSize, fScale);

    auto popAll = rRenderContext.ScopedPush(vcl::PushFlags::ALL);
    rRenderContext.SetAntialiasing(AntialiasingFlags::Enable | AntialiasingFlags::PixelSnapHairline);

    const tools::Long nRailW = sx(220.0);

    // 1. Right canvas background
    rRenderContext.SetLineColor();
    rRenderContext.SetFillColor(vcl::officestudio::SurfaceSubtle);
    rRenderContext.DrawRect(tools::Rectangle(Point(nRailW, 0), Size(std::max<tools::Long>(0, aSize.Width() - nRailW), aSize.Height())));

    // 2. Left Excel-green Backstage rail
    rRenderContext.SetFillColor(vcl::officestudio::CalcBackstageRailBg);
    rRenderContext.DrawRect(tools::Rectangle(Point(0, 0), Size(nRailW, aSize.Height())));

    auto makeFont = [&](double fPx, bool bSemibold) -> vcl::Font {
        vcl::Font aF(rRenderContext.GetFont());
        aF.SetFamilyName(bSemibold ? u"Segoe UI Semibold"_ustr : u"Segoe UI"_ustr);
        aF.SetWeight(WEIGHT_NORMAL);
        aF.SetFontHeight(sx(fPx));
        aF.SetTransparent(true);
        return aF;
    };

    // Draw rail Back button (target 0)
    if (!maHitTargets.empty())
    {
        const HitTarget& rBack = maHitTargets[0];
        if (mnHoverId == rBack.nId)
        {
            const basegfx::B2DPolygon aCirc = basegfx::utils::createPolygonFromCircle(
                basegfx::B2DPoint(rBack.aRect.Center().X(), rBack.aRect.Center().Y()), rBack.aRect.GetWidth() * 0.5);
            rRenderContext.SetLineColor();
            rRenderContext.SetFillColor(vcl::officestudio::CalcBackstageHover);
            rRenderContext.DrawPolygon(aCirc);
        }
        DrawVectorGlyph(rRenderContext, NavGlyph::Back, rBack.aRect.Center().X(), rBack.aRect.Center().Y(),
                        12.0 * fScale, COL_WHITE, 1.7 * fScale);
    }

    struct RailMeta
    {
        const char16_t* pLabel;
        NavGlyph eGlyph;
    };
    static constexpr RailMeta aRailMetas[] = {
        { u"Recent Files", NavGlyph::Recent },
        { u"New",          NavGlyph::New },
        { u"Open",         NavGlyph::Open },
        { u"Save",         NavGlyph::Save },
        { u"Save As",      NavGlyph::SaveAs },
        { u"Print",        NavGlyph::Print },
        { u"Export",       NavGlyph::Export },
        { u"Properties",   NavGlyph::Properties },
        { u"Options",      NavGlyph::Options },
    };

    // Subtle translucent white rail dividers
    if (maHitTargets.size() >= 10)
    {
        rRenderContext.SetLineColor(Color(0x2A, 0x93, 0x59));
        const tools::Long nDiv1Y = maHitTargets[3].aRect.Bottom() + sx(8.0);
        rRenderContext.DrawLine(Point(sx(18.0), nDiv1Y), Point(nRailW - sx(18.0), nDiv1Y));
        const tools::Long nDiv2Y = maHitTargets[8].aRect.Top() - sx(10.0);
        rRenderContext.DrawLine(Point(sx(18.0), nDiv2Y), Point(nRailW - sx(18.0), nDiv2Y));
    }

    rRenderContext.SetTextFillColor();
    for (size_t i = 0; i < 9 && (i + 1) < maHitTargets.size(); ++i)
    {
        const HitTarget& rT = maHitTargets[i + 1];
        const bool bActive = (rT.eKind == TargetKind::SelectSection && rT.eSection == meSection);
        const bool bHover = (mnHoverId == rT.nId);

        if (bActive)
        {
            DrawBox(rRenderContext, rT.aRect, 6.0 * fScale, vcl::officestudio::CalcBackstageActive, COL_TRANSPARENT);
            tools::Rectangle aInd(Point(rT.aRect.Left() + sx(4.0), rT.aRect.Top() + sx(8.0)),
                                  Size(sx(3.0), rT.aRect.GetHeight() - sx(16.0)));
            DrawBox(rRenderContext, aInd, 1.5 * fScale, COL_WHITE, COL_TRANSPARENT);
        }
        else if (bHover)
        {
            DrawBox(rRenderContext, rT.aRect, 6.0 * fScale, vcl::officestudio::CalcBackstageHover, COL_TRANSPARENT);
        }

        const double nIconCx = rT.aRect.Left() + sx(24.0);
        const double nIconCy = rT.aRect.Center().Y();
        DrawVectorGlyph(rRenderContext, aRailMetas[i].eGlyph, nIconCx, nIconCy, 8.0 * fScale, COL_WHITE, 1.5 * fScale);

        rRenderContext.SetFont(makeFont(13.5, bActive));
        rRenderContext.SetTextColor(COL_WHITE);
        tools::Rectangle aTxtRc(rT.aRect.Left() + sx(44.0), rT.aRect.Top(), rT.aRect.Right() - sx(10.0), rT.aRect.Bottom());
        rRenderContext.DrawText(aTxtRc, OUString(aRailMetas[i].pLabel), DrawTextFlags::Left | DrawTextFlags::VCenter);
    }

    // 3. Content Area Header
    const tools::Long nPadX = sx(36.0);
    const tools::Long nContentX = nRailW + nPadX;
    const tools::Long nContentW = std::min<tools::Long>(sx(960.0), std::max<tools::Long>(sx(300.0), aSize.Width() - nContentX - nPadX));

    OUString sHeading = u"Recent Files"_ustr;
    switch (meSection)
    {
        case CalcBackstageSection::Recent: sHeading = u"Recent Files"_ustr; break;
        case CalcBackstageSection::New:    sHeading = u"New Workbook"_ustr; break;
        case CalcBackstageSection::Open:   sHeading = u"Open Workbook"_ustr; break;
        case CalcBackstageSection::SaveAs: sHeading = u"Save As"_ustr; break;
        case CalcBackstageSection::Print:  sHeading = u"Print"_ustr; break;
        case CalcBackstageSection::Export: sHeading = u"Export"_ustr; break;
    }

    rRenderContext.SetFont(makeFont(22.0, true));
    rRenderContext.SetTextColor(vcl::officestudio::TextPrimary);
    rRenderContext.DrawText(tools::Rectangle(Point(nContentX, sx(28.0)), Size(nContentW - sx(260.0), sx(36.0))),
                            sHeading, DrawTextFlags::Left | DrawTextFlags::VCenter);

    // Current workbook status badge on top-right
    {
        const OUString sStatus = mbDocModified ? (msDocTitle + u"  •  Unsaved changes"_ustr)
                                               : (msDocTitle + u"  •  Ready"_ustr);
        rRenderContext.SetFont(makeFont(12.0, false));
        const tools::Long nBadgeW = std::min<tools::Long>(sx(300.0), rRenderContext.GetTextWidth(sStatus) + sx(28.0));
        tools::Rectangle aStatusRc(Point(nContentX + nContentW - nBadgeW, sx(32.0)), Size(nBadgeW, sx(28.0)));
        DrawBox(rRenderContext, aStatusRc, 14.0 * fScale, COL_WHITE, vcl::officestudio::RibbonCardBorder, 1.0 * fScale);
        rRenderContext.SetTextColor(mbDocModified ? vcl::officestudio::AmberDark : vcl::officestudio::CalcAccentDark);
        rRenderContext.DrawText(tools::Rectangle(aStatusRc.Left() + sx(12.0), aStatusRc.Top(), aStatusRc.Right() - sx(12.0), aStatusRc.Bottom()),
                                sStatus, DrawTextFlags::Center | DrawTextFlags::VCenter | DrawTextFlags::EndEllipsis);
    }

    auto drawCardArray = [&](size_t nTargetOffset, const ActionCardDef* pCards, size_t nCount)
    {
        for (size_t i = 0; i < nCount && (nTargetOffset + i) < maHitTargets.size(); ++i)
        {
            const HitTarget& rT = maHitTargets[nTargetOffset + i];
            const ActionCardDef& rDef = pCards[i];
            const bool bHover = (mnHoverId == rT.nId);

            DrawBox(rRenderContext, rT.aRect, 8.0 * fScale,
                    bHover ? vcl::officestudio::CalcFileNormalBg : COL_WHITE,
                    bHover ? vcl::officestudio::CalcAccent : vcl::officestudio::RibbonCardBorder,
                    1.0 * fScale);

            const tools::Long nTileSz = sx(44.0);
            tools::Rectangle aTileRc(Point(rT.aRect.Left() + sx(16.0), rT.aRect.Top() + (rT.aRect.GetHeight() - nTileSz) / 2),
                                     Size(nTileSz, nTileSz));
            DrawBox(rRenderContext, aTileRc, 8.0 * fScale, rDef.aSoftBg, COL_TRANSPARENT);
            DrawVectorGlyph(rRenderContext, rDef.eGlyph, aTileRc.Center().X(), aTileRc.Center().Y(),
                            10.0 * fScale, rDef.aAccent, 1.6 * fScale);

            const tools::Long nTx = aTileRc.Right() + sx(14.0);
            const tools::Long nTw = std::max<tools::Long>(40, rT.aRect.Right() - nTx - sx(14.0));
            const tools::Long nMidY = rT.aRect.Center().Y();

            rRenderContext.SetFont(makeFont(14.0, true));
            rRenderContext.SetTextColor(vcl::officestudio::TextPrimary);
            rRenderContext.DrawText(tools::Rectangle(Point(nTx, nMidY - sx(20.0)), Size(nTw, sx(22.0))),
                                    OUString(rDef.pTitle), DrawTextFlags::Left | DrawTextFlags::VCenter | DrawTextFlags::EndEllipsis);

            rRenderContext.SetFont(makeFont(12.0, false));
            rRenderContext.SetTextColor(vcl::officestudio::TextMuted);
            rRenderContext.DrawText(tools::Rectangle(Point(nTx, nMidY + sx(2.0)), Size(nTw, sx(20.0))),
                                    OUString(rDef.pDesc), DrawTextFlags::Left | DrawTextFlags::VCenter | DrawTextFlags::EndEllipsis);
        }
    };

    auto drawRecentPanel = [&](size_t nFilterTargetOffset, tools::Long nPanelTop)
    {
        const tools::Long nPanelH = std::max<tools::Long>(sx(160.0), aSize.Height() - nPanelTop - sx(28.0));
        tools::Rectangle aPanelRc(Point(nContentX, nPanelTop), Size(nContentW, nPanelH));
        DrawBox(rRenderContext, aPanelRc, 8.0 * fScale, COL_WHITE, vcl::officestudio::RibbonCardBorder, 1.0 * fScale);

        const tools::Long nHeaderH = sx(46.0);
        rRenderContext.SetFont(makeFont(14.0, true));
        rRenderContext.SetTextColor(vcl::officestudio::TextPrimary);
        rRenderContext.DrawText(tools::Rectangle(Point(nContentX + sx(18.0), nPanelTop), Size(sx(220.0), nHeaderH)),
                                u"Recent Workbooks"_ustr, DrawTextFlags::Left | DrawTextFlags::VCenter);

        // Filter pills & Browse button
        if (nFilterTargetOffset + 2 < maHitTargets.size())
        {
            const HitTarget& rCalcBtn = maHitTargets[nFilterTargetOffset];
            const HitTarget& rAllBtn = maHitTargets[nFilterTargetOffset + 1];
            const HitTarget& rBrowseBtn = maHitTargets[nFilterTargetOffset + 2];

            DrawBox(rRenderContext, rCalcBtn.aRect, 13.0 * fScale,
                    mbOnlySpreadsheets ? vcl::officestudio::CalcAccentSoft : (mnHoverId == rCalcBtn.nId ? vcl::officestudio::HoverBg : vcl::officestudio::SurfaceSubtle),
                    mbOnlySpreadsheets ? vcl::officestudio::CalcAccentBorder : vcl::officestudio::Divider,
                    1.0 * fScale);
            rRenderContext.SetFont(makeFont(12.0, mbOnlySpreadsheets));
            rRenderContext.SetTextColor(mbOnlySpreadsheets ? vcl::officestudio::CalcAccentDark : vcl::officestudio::Slate600);
            rRenderContext.DrawText(rCalcBtn.aRect, u"Spreadsheets"_ustr, DrawTextFlags::Center | DrawTextFlags::VCenter);

            DrawBox(rRenderContext, rAllBtn.aRect, 13.0 * fScale,
                    !mbOnlySpreadsheets ? vcl::officestudio::CalcAccentSoft : (mnHoverId == rAllBtn.nId ? vcl::officestudio::HoverBg : vcl::officestudio::SurfaceSubtle),
                    !mbOnlySpreadsheets ? vcl::officestudio::CalcAccentBorder : vcl::officestudio::Divider,
                    1.0 * fScale);
            rRenderContext.SetFont(makeFont(12.0, !mbOnlySpreadsheets));
            rRenderContext.SetTextColor(!mbOnlySpreadsheets ? vcl::officestudio::CalcAccentDark : vcl::officestudio::Slate600);
            rRenderContext.DrawText(rAllBtn.aRect, u"All Files"_ustr, DrawTextFlags::Center | DrawTextFlags::VCenter);

            if (mnHoverId == rBrowseBtn.nId)
                DrawBox(rRenderContext, rBrowseBtn.aRect, 6.0 * fScale, vcl::officestudio::CalcAccentSoft, COL_TRANSPARENT);
            rRenderContext.SetFont(makeFont(12.0, true));
            rRenderContext.SetTextColor(vcl::officestudio::CalcAccent);
            rRenderContext.DrawText(rBrowseBtn.aRect, u"Browse..."_ustr, DrawTextFlags::Center | DrawTextFlags::VCenter);
        }

        rRenderContext.SetLineColor(vcl::officestudio::Divider);
        rRenderContext.DrawLine(Point(nContentX + 1, nPanelTop + nHeaderH), Point(nContentX + nContentW - 2, nPanelTop + nHeaderH));

        std::vector<const RecentEntry*> aFiltered;
        for (const auto& rRec : maRecentEntries)
        {
            if (!mbOnlySpreadsheets || rRec.bIsCalc)
                aFiltered.push_back(&rRec);
        }

        auto popClip = rRenderContext.ScopedPush(vcl::PushFlags::CLIPREGION);
        rRenderContext.IntersectClipRegion(maRecentScrollBounds);

        if (aFiltered.empty())
        {
            rRenderContext.SetFont(makeFont(14.5, true));
            rRenderContext.SetTextColor(vcl::officestudio::TextPrimary);
            rRenderContext.DrawText(tools::Rectangle(Point(nContentX + sx(24.0), maRecentScrollBounds.Top() + sx(28.0)),
                                                     Size(nContentW - sx(48.0), sx(24.0))),
                                    mbOnlySpreadsheets ? u"No recent spreadsheets found"_ustr : u"No recent files yet"_ustr,
                                    DrawTextFlags::Center | DrawTextFlags::VCenter);
            rRenderContext.SetFont(makeFont(12.5, false));
            rRenderContext.SetTextColor(vcl::officestudio::TextMuted);
            rRenderContext.DrawText(tools::Rectangle(Point(nContentX + sx(24.0), maRecentScrollBounds.Top() + sx(54.0)),
                                                     Size(nContentW - sx(48.0), sx(22.0))),
                                    u"Spreadsheets you open or save will appear here for quick access."_ustr,
                                    DrawTextFlags::Center | DrawTextFlags::VCenter);

            const size_t nBtnIdx = nFilterTargetOffset + 3;
            if (nBtnIdx < maHitTargets.size())
            {
                const HitTarget& rOpenBtn = maHitTargets[nBtnIdx];
                const bool bHover = (mnHoverId == rOpenBtn.nId);
                DrawBox(rRenderContext, rOpenBtn.aRect, 6.0 * fScale,
                        bHover ? vcl::officestudio::CalcAccentHover : vcl::officestudio::CalcAccent,
                        COL_TRANSPARENT);
                rRenderContext.SetFont(makeFont(13.0, true));
                rRenderContext.SetTextColor(COL_WHITE);
                rRenderContext.DrawText(rOpenBtn.aRect, u"Open File..."_ustr, DrawTextFlags::Center | DrawTextFlags::VCenter);
            }
        }
        else
        {
            const tools::Long nRowH = sx(50.0);
            for (size_t i = 0; i < aFiltered.size(); ++i)
            {
                const RecentEntry* pRec = aFiltered[i];
                const tools::Long nRy = maRecentScrollBounds.Top() + static_cast<tools::Long>(i) * nRowH - mnScrollY;
                if (nRy + nRowH < maRecentScrollBounds.Top() || nRy > maRecentScrollBounds.Bottom())
                    continue;

                const tools::Rectangle aRowRc(Point(nContentX + sx(10.0), nRy + sx(3.0)),
                                              Size(nContentW - sx(20.0), nRowH - sx(6.0)));
                bool bHover = false;
                if (mnHoverId >= 0)
                {
                    for (size_t t = nFilterTargetOffset + 3; t < maHitTargets.size(); ++t)
                    {
                        const HitTarget& rT = maHitTargets[t];
                        if (rT.nId == mnHoverId && rT.eKind == TargetKind::OpenRecentFile
                            && rT.sFileUrl == pRec->sURL && rT.aRect.Overlaps(aRowRc))
                        {
                            bHover = true;
                            break;
                        }
                    }
                }

                if (bHover)
                    DrawBox(rRenderContext, aRowRc, 6.0 * fScale, vcl::officestudio::CalcFileNormalBg, vcl::officestudio::CalcAccentBorder, 1.0 * fScale);

                const tools::Long nIconSz = sx(28.0);
                const tools::Rectangle aIconRc(Point(aRowRc.Left() + sx(10.0), aRowRc.Top() + (aRowRc.GetHeight() - nIconSz) / 2),
                                               Size(nIconSz, nIconSz));
                const Color aIconFg = pRec->bIsCalc ? vcl::officestudio::CalcAccent : vcl::officestudio::RoyalBlue;
                const Color aIconBg = pRec->bIsCalc ? vcl::officestudio::CalcAccentSoft : vcl::officestudio::RoyalBlueTile;
                DrawBox(rRenderContext, aIconRc, 6.0 * fScale, aIconBg, COL_TRANSPARENT);
                DrawVectorGlyph(rRenderContext, pRec->bIsCalc ? NavGlyph::BlankSheet : NavGlyph::Properties,
                                aIconRc.Center().X(), aIconRc.Center().Y(), 6.5 * fScale, aIconFg, 1.4 * fScale);

                const tools::Long nExtW = sx(54.0);
                const tools::Rectangle aExtRc(Point(aRowRc.Right() - nExtW - sx(12.0), aRowRc.Top() + (aRowRc.GetHeight() - sx(22.0)) / 2),
                                              Size(nExtW, sx(22.0)));
                if (!pRec->sExt.isEmpty())
                {
                    DrawBox(rRenderContext, aExtRc, 4.0 * fScale,
                            pRec->bIsCalc ? vcl::officestudio::CalcAccentSoft : vcl::officestudio::SurfaceSubtle,
                            pRec->bIsCalc ? vcl::officestudio::CalcAccentBorder : vcl::officestudio::Divider,
                            1.0 * fScale);
                    rRenderContext.SetFont(makeFont(10.5, true));
                    rRenderContext.SetTextColor(pRec->bIsCalc ? vcl::officestudio::CalcAccentDark : vcl::officestudio::Slate600);
                    rRenderContext.DrawText(aExtRc, pRec->sExt, DrawTextFlags::Center | DrawTextFlags::VCenter);
                }

                const tools::Long nTextX = aIconRc.Right() + sx(12.0);
                const tools::Long nTextW = std::max<tools::Long>(60, aExtRc.Left() - nTextX - sx(12.0));
                rRenderContext.SetFont(makeFont(13.0, true));
                rRenderContext.SetTextColor(vcl::officestudio::TextPrimary);
                rRenderContext.DrawText(tools::Rectangle(Point(nTextX, aRowRc.Top() + sx(3.0)), Size(nTextW, sx(20.0))),
                                        pRec->sName, DrawTextFlags::Left | DrawTextFlags::VCenter | DrawTextFlags::EndEllipsis);

                rRenderContext.SetFont(makeFont(11.5, false));
                rRenderContext.SetTextColor(vcl::officestudio::TextMuted);
                rRenderContext.DrawText(tools::Rectangle(Point(nTextX, aRowRc.Top() + sx(22.0)), Size(nTextW, sx(18.0))),
                                        pRec->sFolder, DrawTextFlags::Left | DrawTextFlags::VCenter | DrawTextFlags::EndEllipsis);
            }
        }
    };

    const tools::Long nBodyTop = sx(94.0);
    switch (meSection)
    {
        case CalcBackstageSection::Recent:
        {
            static constexpr ActionCardDef aQuick[] = {
                { u"Blank workbook", u"Create a new spreadsheet", u".uno:AddDirect", NavGlyph::BlankSheet, vcl::officestudio::CalcAccent, vcl::officestudio::CalcAccentSoft },
                { u"Templates", u"Start from a template", u".uno:NewDoc", NavGlyph::Template, vcl::officestudio::Purple, vcl::officestudio::PurpleTile },
                { u"Open file...", u"Browse this computer", u".uno:OpenFromCalc", NavGlyph::Folder, vcl::officestudio::Amber, vcl::officestudio::AmberTile },
            };
            drawCardArray(10, aQuick, 3);
            drawRecentPanel(13, nBodyTop + sx(88.0 + 14.0 + 10.0));
            break;
        }
        case CalcBackstageSection::New:
        {
            static constexpr ActionCardDef aNewCards[] = {
                { u"Blank workbook", u"Create an empty multi-sheet workbook ready for formulas and data", u".uno:AddDirect", NavGlyph::BlankSheet, vcl::officestudio::CalcAccent, vcl::officestudio::CalcAccentSoft },
                { u"New from template...", u"Choose from installed and custom spreadsheet templates", u".uno:NewDoc", NavGlyph::Template, vcl::officestudio::Purple, vcl::officestudio::PurpleTile },
                { u"Save as template...", u"Store the active workbook structure as a reusable .ots template", u".uno:SaveAsTemplate", NavGlyph::SaveAs, vcl::officestudio::Teal, vcl::officestudio::TealTile },
            };
            drawCardArray(10, aNewCards, 3);
            break;
        }
        case CalcBackstageSection::Open:
        {
            static constexpr ActionCardDef aOpenCards[] = {
                { u"Browse This PC...", u"Open .xlsx, .ods, .xls, or .csv files from local folders", u".uno:OpenFromCalc", NavGlyph::Folder, vcl::officestudio::CalcAccent, vcl::officestudio::CalcAccentSoft },
                { u"Open Remote File...", u"Connect to WebDAV, SharePoint, or CMIS remote storage", u".uno:OpenRemote", NavGlyph::Open, vcl::officestudio::RoyalBlue, vcl::officestudio::RoyalBlueTile },
            };
            drawCardArray(10, aOpenCards, 2);
            drawRecentPanel(12, nBodyTop + sx(94.0 + 14.0 + 10.0));
            break;
        }
        case CalcBackstageSection::SaveAs:
        {
            static constexpr ActionCardDef aSaveCards[] = {
                { u"Save As...", u"Choose folder and format (XLSX, ODS, XLS, CSV)", u".uno:SaveAs", NavGlyph::SaveAs, vcl::officestudio::CalcAccent, vcl::officestudio::CalcAccentSoft },
                { u"Save a Copy...", u"Save an independent copy without leaving the current workbook", u".uno:SaveACopy", NavGlyph::Save, vcl::officestudio::RoyalBlue, vcl::officestudio::RoyalBlueTile },
                { u"Save as Template...", u"Save current layout and styles as a template (.ots)", u".uno:SaveAsTemplate", NavGlyph::Template, vcl::officestudio::Purple, vcl::officestudio::PurpleTile },
                { u"Save Remote...", u"Save this spreadsheet to a configured remote server", u".uno:SaveAsRemote", NavGlyph::Folder, vcl::officestudio::Teal, vcl::officestudio::TealTile },
            };
            drawCardArray(10, aSaveCards, 4);
            break;
        }
        case CalcBackstageSection::Print:
        {
            static constexpr ActionCardDef aPrintCards[] = {
                { u"Print...", u"Configure printer, page range, copies, and scaling (Ctrl+P)", u".uno:Print", NavGlyph::Print, vcl::officestudio::CalcAccent, vcl::officestudio::CalcAccentSoft },
                { u"Print Preview", u"Inspect page breaks, headers, footers, and margins", u".uno:PrintPreview", NavGlyph::Properties, vcl::officestudio::RoyalBlue, vcl::officestudio::RoyalBlueTile },
                { u"Quick Print", u"Send immediately to your default printer", u".uno:PrintDefault", NavGlyph::Print, vcl::officestudio::Teal, vcl::officestudio::TealTile },
                { u"Printer Setup...", u"Select default printer and paper orientation settings", u".uno:PrinterSetup", NavGlyph::Options, vcl::officestudio::Slate700, vcl::officestudio::SurfaceSubtle },
            };
            drawCardArray(10, aPrintCards, 4);
            break;
        }
        case CalcBackstageSection::Export:
        {
            static constexpr ActionCardDef aExportCards[] = {
                { u"Export as PDF...", u"Configure PDF resolution, sheet selection, and security", u".uno:ExportToPDF", NavGlyph::Pdf, vcl::officestudio::Red, vcl::officestudio::RedTile },
                { u"Quick Export Directly as PDF", u"Save a PDF copy with current default settings", u".uno:ExportDirectToPDF", NavGlyph::Export, vcl::officestudio::CalcAccent, vcl::officestudio::CalcAccentSoft },
                { u"Export to Other Format...", u"Export spreadsheet to HTML, XHTML, or other formats", u".uno:ExportTo", NavGlyph::SaveAs, vcl::officestudio::Purple, vcl::officestudio::PurpleTile },
                { u"Send as Email...", u"Attach this spreadsheet to a new email message", u".uno:SendMail", NavGlyph::Mail, vcl::officestudio::RoyalBlue, vcl::officestudio::RoyalBlueTile },
            };
            drawCardArray(10, aExportCards, 4);
            break;
        }
    }
}

void CalcBackstageView::MouseMove(const MouseEvent& rMEvt)
{
    const HitTarget* pHit = rMEvt.IsLeaveWindow() ? nullptr : FindHitTarget(rMEvt.GetPosPixel());
    const int nNewHover = pHit ? pHit->nId : -1;
    if (nNewHover != mnHoverId)
    {
        mnHoverId = nNewHover;
        Invalidate();
    }
    SetPointer(pHit ? PointerStyle::RefHand : PointerStyle::Arrow);
    Control::MouseMove(rMEvt);
}

void CalcBackstageView::MouseButtonDown(const MouseEvent& rMEvt)
{
    if (!rMEvt.IsLeft())
    {
        Control::MouseButtonDown(rMEvt);
        return;
    }

    const HitTarget* pHit = FindHitTarget(rMEvt.GetPosPixel());
    if (!pHit)
        return;

    switch (pHit->eKind)
    {
        case TargetKind::CloseBackstage:
            maCloseHdl.Call(this);
            break;
        case TargetKind::SelectSection:
            SetSection(pHit->eSection);
            break;
        case TargetKind::FilterCalcOnly:
            if (!mbOnlySpreadsheets)
            {
                mbOnlySpreadsheets = true;
                mnScrollY = 0;
                Invalidate();
            }
            break;
        case TargetKind::FilterAllFiles:
            if (mbOnlySpreadsheets)
            {
                mbOnlySpreadsheets = false;
                mnScrollY = 0;
                Invalidate();
            }
            break;
        case TargetKind::DispatchCommand:
            PostDispatchCommand(pHit->sCommand);
            break;
        case TargetKind::OpenRecentFile:
            PostOpenRecentFile(pHit->sFileUrl, pHit->bReadOnly);
            break;
    }
}

void CalcBackstageView::KeyInput(const KeyEvent& rKEvt)
{
    if (rKEvt.GetKeyCode().GetCode() == KEY_ESCAPE)
    {
        maCloseHdl.Call(this);
        return;
    }
    Control::KeyInput(rKEvt);
}

void CalcBackstageView::Command(const CommandEvent& rCEvt)
{
    if (rCEvt.GetCommand() == CommandEventId::Wheel && mnMaxScrollY > 0)
    {
        const CommandWheelData* pData = rCEvt.GetWheelData();
        if (pData && pData->GetMode() == CommandWheelMode::SCROLL && pData->GetDelta() != 0)
        {
            const tools::Long nStep = static_cast<tools::Long>(std::lround(36.0 * GetDPIScaleFactor()));
            mnScrollY = std::clamp<tools::Long>(mnScrollY + (pData->GetDelta() < 0 ? nStep : -nStep), 0, mnMaxScrollY);
            Invalidate();
            return;
        }
    }
    Control::Command(rCEvt);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
