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

#include <sfx2/sidebar/TabBar.hxx>
#include <sidebar/DeckDescriptor.hxx>
#include <sfx2/sidebar/Theme.hxx>
#include <sidebar/Tools.hxx>
#include <sfx2/sidebar/FocusManager.hxx>
#include <sfx2/sidebar/SidebarController.hxx>

#include <algorithm>
#include <basegfx/polygon/b2dpolygon.hxx>
#include <basegfx/polygon/b2dpolygontools.hxx>
#include <cmath>
#include <comphelper/lok.hxx>
#include <comphelper/processfactory.hxx>
#include <o3tl/safeint.hxx>
#include <utility>
#include <vector>
#include <vcl/commandevent.hxx>
#include <vcl/commandinfoprovider.hxx>
#include <vcl/event.hxx>
#include <vcl/graph.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/rendercontext/DrawTextFlags.hxx>
#include <vcl/settings.hxx>
#include <vcl/svapp.hxx>
#include <vcl/virdev.hxx>
#include <vcl/weld/Box.hxx>
#include <vcl/weld/Menu.hxx>
#include <vcl/weld/MenuButton.hxx>
#include <vcl/weld/Toolbar.hxx>
#include <svtools/acceleratorexecute.hxx>
#include <osl/diagnose.h>

#include "uiobject.hxx"

using namespace css;
using namespace css::uno;

static int gDefaultWidth;

namespace sfx2::sidebar {

TabBar::TabBar(vcl::Window* pParentWindow,
               const Reference<frame::XFrame>& rxFrame,
               std::function<void (const OUString&)> aDeckActivationFunctor,
               PopupMenuSignalConnectFunction aPopupMenuSignalConnectFunction,
               SidebarController& rParentSidebarController
              )
    : InterimItemWindow(pParentWindow, u"sfx/ui/tabbar.ui"_ustr, u"TabBar"_ustr)
    , mxFrame(rxFrame)
    , mxAuxBuilder(Application::CreateBuilder(m_xContainer.get(), u"sfx/ui/tabbarcontents.ui"_ustr))
    , mxTempToplevel(mxAuxBuilder->weld_box(u"toplevel"_ustr))
    , mxContents(mxAuxBuilder->weld_widget(u"TabBarContents"_ustr))
    , mxMeasureBox(mxAuxBuilder->weld_widget(u"measure"_ustr))
    , maDeckActivationFunctor(std::move(aDeckActivationFunctor))
    , mrParentSidebarController(rParentSidebarController)
{
    set_id(u"TabBar"_ustr); // for uitest

    InitControlBase(mxMenuButton.get());

    mxTempToplevel->move(mxContents.get(), m_xContainer.get());

    // For Gtk4 defer menu_button until after the contents have been
    // transferred to its final home (where the old parent is a GtkWindow to
    // support loading the accelerators in the menu for Gtk3)
    mxMenuButton = mxAuxBuilder->weld_menu_button(u"menubutton"_ustr);
    mxMainMenu = mxAuxBuilder->weld_menu(u"mainmenu"_ustr);
    mxSubMenu = mxAuxBuilder->weld_menu(u"submenu"_ustr);
    aPopupMenuSignalConnectFunction(*mxMainMenu, *mxSubMenu);

    UpdateMenus();

    gDefaultWidth = m_xContainer->get_preferred_size().Width();

    // we have this widget just so we can measure best width for static TabBar::GetDefaultWidth
    mxMeasureBox->hide();

    SetBackground(Wallpaper(Theme::GetColor(Theme::Color_TabBarBackground)));

#if OSL_DEBUG_LEVEL >= 2
    SetText(OUString("TabBar"));
#endif
}

TabBar::~TabBar()
{
    disposeOnce();
}

void TabBar::dispose()
{
    maItems.clear();
    mxMeasureBox.reset();
    mxSubMenu.reset();
    mxMainMenu.reset();
    mxMenuButton.reset();
    m_xContainer->move(mxContents.get(), mxTempToplevel.get());
    mxContents.reset();
    mxTempToplevel.reset();
    mxAuxBuilder.reset();
    InterimItemWindow::dispose();
}

sal_Int32 TabBar::GetDefaultWidth()
{
    if (!gDefaultWidth)
    {
        std::unique_ptr<weld::Builder> xBuilder(Application::CreateBuilder(nullptr, u"sfx/ui/tabbarcontents.ui"_ustr));
        std::unique_ptr<weld::Widget> xContainer(xBuilder->weld_widget(u"TabBarContents"_ustr));
        gDefaultWidth = xContainer->get_preferred_size().Width();
    }
    return gDefaultWidth;
}

void TabBar::SetDecks(const ResourceManager::DeckContextDescriptorContainer& rDecks)
{
    // invisible with LOK, so keep empty to avoid invalidations
    if (comphelper::LibreOfficeKit::isActive())
        return;

    // Remove the current buttons.
    maItems.clear();
    for (auto const& deck : rDecks)
    {
        std::shared_ptr<DeckDescriptor> xDescriptor = mrParentSidebarController.GetResourceManager()->GetDeckDescriptor(deck.msId);
        if (xDescriptor == nullptr)
        {
            OSL_ASSERT(xDescriptor!=nullptr);
            continue;
        }

        maItems.emplace_back(std::make_unique<Item>(*this));
        auto& xItem(maItems.back());
        xItem->msDeckId = xDescriptor->msId;
        CreateTabItem(*xItem->mxButton, *xDescriptor);
        xItem->mxButton->connect_clicked(LINK(xItem.get(), TabBar::Item, HandleClick));
        xItem->maDeckActivationFunctor = maDeckActivationFunctor;
        xItem->mbIsHidden = !xDescriptor->mbIsEnabled;

        xItem->mxButton->set_visible(deck.mbIsEnabled);
    }

    UpdateButtonIcons();
    UpdateMenus();
}

void TabBar::UpdateButtonIcons()
{
    for (auto const& item : maItems)
    {
        std::shared_ptr<DeckDescriptor> xDeckDescriptor = mrParentSidebarController.GetResourceManager()->GetDeckDescriptor(item->msDeckId);
        if (!xDeckDescriptor)
            continue;
        item->mxButton->set_item_image(u"toggle"_ustr, GetItemImage(*xDeckDescriptor));
    }
}

void TabBar::HighlightDeck(std::u16string_view rsDeckId)
{
    for (auto const& item : maItems)
        item->mxButton->set_item_active(u"toggle"_ustr, item->msDeckId == rsDeckId);
    UpdateMenus();
}

void TabBar::RemoveDeckHighlight()
{
    for (auto const& item : maItems)
        item->mxButton->set_item_active(u"toggle"_ustr, false);
    UpdateMenus();
}

void TabBar::DataChanged(const DataChangedEvent& rDataChangedEvent)
{
    SetBackground(Theme::GetColor(Theme::Color_TabBarBackground));
    UpdateButtonIcons();
    UpdateMenus();

    InterimItemWindow::DataChanged(rDataChangedEvent);
}

bool TabBar::EventNotify(NotifyEvent& rEvent)
{
    NotifyEventType nType = rEvent.GetType();
    if(NotifyEventType::KEYINPUT == nType)
    {
        return InterimItemWindow::EventNotify(rEvent);
    }
    else if(NotifyEventType::COMMAND == nType)
    {
        const CommandEvent& rCommandEvent = *rEvent.GetCommandEvent();
        if(rCommandEvent.GetCommand() == CommandEventId::Wheel)
        {
            const CommandWheelData* pData = rCommandEvent.GetWheelData();
            if(!pData->GetModifier() && (pData->GetMode() == CommandWheelMode::SCROLL))
            {
                auto pItem = std::find_if(maItems.begin(), maItems.end(),
                    [] (const auto& item) { return item->mxButton->get_item_active("toggle"); });
                if(pItem == maItems.end())
                    return true;
                if(pData->GetNotchDelta()<0)
                {
                    if(pItem+1 == maItems.end())
                        return true;
                    ++pItem;
                }
                else
                {
                    if(pItem == maItems.begin())
                        return true;
                    --pItem;
                }
                try
                {
                    (*pItem)->maDeckActivationFunctor((*pItem)->msDeckId);
                    GrabFocusToDocument();
                }
                catch(const css::uno::Exception&) {};
                return true;
            }
        }
    }
    return false;
}

void TabBar::CreateTabItem(weld::Toolbar& rItem, const DeckDescriptor& rDeckDescriptor)
{
    rItem.set_accessible_description(rDeckDescriptor.msHelpText);
    const OUString sCommand = ".uno:SidebarDeck." + rDeckDescriptor.msId;
    OUString sShortcut = vcl::CommandInfoProvider::GetCommandShortcut(sCommand, mxFrame);
    if (!sShortcut.isEmpty())
        sShortcut = u" (" + sShortcut + u")";
    rItem.set_item_accessible_name(u"toggle"_ustr, rDeckDescriptor.msTitle);
    rItem.set_item_tooltip_text(u"toggle"_ustr, rDeckDescriptor.msHelpText + sShortcut);
}

namespace
{
struct DeckColorStyle
{
    Color aFg;
    Color aTileBg;
    Color aTileBorder;
};

void DrawPoly(VirtualDevice& rDev, const std::vector<basegfx::B2DPoint>& rPts,
              bool bClosed, Color aColor, double fWidth)
{
    basegfx::B2DPolygon aPoly;
    for (const auto& pt : rPts)
        aPoly.append(pt);
    aPoly.setClosed(bClosed);
    rDev.SetFillColor();
    rDev.SetLineColor(aColor);
    rDev.DrawPolyLine(aPoly, fWidth, basegfx::B2DLineJoin::Round, css::drawing::LineCap_ROUND);
}

void DrawCircle(VirtualDevice& rDev, double cx, double cy, double r,
                Color aColor, double fWidth, bool bFill = false)
{
    basegfx::B2DPolygon aPoly = basegfx::utils::createPolygonFromCircle(basegfx::B2DPoint(cx, cy), r);
    if (bFill)
    {
        rDev.SetLineColor();
        rDev.SetFillColor(aColor);
        rDev.DrawPolygon(aPoly);
    }
    else
    {
        rDev.SetFillColor();
        rDev.SetLineColor(aColor);
        rDev.DrawPolyLine(aPoly, fWidth, basegfx::B2DLineJoin::Round);
    }
}

css::uno::Reference<css::graphic::XGraphic> MakeColorfulDeckGraphic(const OUString& rDeckId, float fScale)
{
    int nKind = -1;
    DeckColorStyle aStyle{ Color(0x25, 0x63, 0xEB), Color(0xEF, 0xF6, 0xFF), Color(0xBF, 0xDB, 0xFE) };

    if (rDeckId == u"PropertyDeck")
    {
        nKind = 0;
        aStyle = { Color(0x25, 0x63, 0xEB), Color(0xEF, 0xF6, 0xFF), Color(0xBF, 0xDB, 0xFE) };
    }
    else if (rDeckId == u"StyleListDeck")
    {
        nKind = 1;
        aStyle = { Color(0x7C, 0x3A, 0xED), Color(0xF5, 0xF3, 0xFF), Color(0xDD, 0xD6, 0xFE) };
    }
    else if (rDeckId == u"GalleryDeck")
    {
        nKind = 2;
        aStyle = { Color(0x16, 0xA3, 0x4A), Color(0xF0, 0xFD, 0xF4), Color(0xBB, 0xF7, 0xD0) };
    }
    else if (rDeckId == u"NavigatorDeck")
    {
        nKind = 3;
        aStyle = { Color(0xEA, 0x58, 0x0C), Color(0xFF, 0xF7, 0xED), Color(0xFE, 0xD7, 0xAA) };
    }
    else if (rDeckId == u"WriterPageDeck")
    {
        nKind = 4;
        aStyle = { Color(0x02, 0x84, 0xC7), Color(0xF0, 0xF9, 0xFF), Color(0xBA, 0xE6, 0xFD) };
    }
    else if (rDeckId == u"InspectorDeck")
    {
        nKind = 5;
        aStyle = { Color(0x4F, 0x46, 0xE5), Color(0xEE, 0xF2, 0xFF), Color(0xC7, 0xD2, 0xFE) };
    }
    else if (rDeckId == u"SwManageChangesDeck")
    {
        nKind = 6;
        aStyle = { Color(0xE1, 0x1D, 0x48), Color(0xFF, 0xF1, 0xF2), Color(0xFE, 0xCD, 0xD3) };
    }
    else if (rDeckId == u"SwDesignDeck")
    {
        nKind = 7;
        aStyle = { Color(0xDB, 0x27, 0x77), Color(0xFD, 0xF2, 0xF8), Color(0xFB, 0xCF, 0xE8) };
    }
    else if (rDeckId == u"A11yCheckDeck")
    {
        nKind = 8;
        aStyle = { Color(0x0D, 0x94, 0x88), Color(0xF0, 0xFD, 0xFA), Color(0x99, 0xF6, 0xE4) };
    }
    else if (rDeckId == u"CommentsDeck")
    {
        nKind = 9;
        aStyle = { Color(0xD9, 0x77, 0x06), Color(0xFF, 0xFB, 0xEB), Color(0xFD, 0xE6, 0x8A) };
    }
    else if (rDeckId == u"FindDeck")
    {
        nKind = 10;
        aStyle = { Color(0x08, 0x91, 0xB2), Color(0xEC, 0xFE, 0xFF), Color(0xA5, 0xF3, 0xFC) };
    }
    else if (rDeckId == u"ScFunctionsDeck")
    {
        nKind = 11;
        aStyle = { Color(0x16, 0xA3, 0x4A), Color(0xF0, 0xFD, 0xF4), Color(0xBB, 0xF7, 0xD0) };
    }
    else if (rDeckId == u"ShapesDeck")
    {
        nKind = 12;
        aStyle = { Color(0xE1, 0x1D, 0x48), Color(0xFF, 0xF1, 0xF2), Color(0xFE, 0xCD, 0xD3) };
    }
    else if (rDeckId == u"SdSlideTransitionDeck")
    {
        nKind = 13;
        aStyle = { Color(0x0D, 0x94, 0x88), Color(0xF0, 0xFD, 0xFA), Color(0x99, 0xF6, 0xE4) };
    }
    else if (rDeckId == u"SdCustomAnimationDeck")
    {
        nKind = 14;
        aStyle = { Color(0xD9, 0x77, 0x06), Color(0xFF, 0xFB, 0xEB), Color(0xFD, 0xE6, 0x8A) };
    }
    else if (rDeckId == u"SdMasterPagesDeck")
    {
        nKind = 15;
        aStyle = { Color(0x4F, 0x46, 0xE5), Color(0xEE, 0xF2, 0xFF), Color(0xC7, 0xD2, 0xFE) };
    }
    else if (rDeckId == u"ElementsDeck")
    {
        nKind = 16;
        aStyle = { Color(0x7C, 0x3A, 0xED), Color(0xF5, 0xF3, 0xFF), Color(0xDD, 0xD6, 0xFE) };
    }
    else
    {
        return nullptr;
    }

    const double fS = std::max(1.0, static_cast<double>(fScale));
    const tools::Long nSz = static_cast<tools::Long>(std::lround(24.0 * fS));
    ScopedVclPtrInstance<VirtualDevice> pVDev(DeviceFormat::WITH_ALPHA);
    pVDev->SetOutputSizePixel(Size(nSz, nSz), true, true);
    pVDev->SetAntialiasing(AntialiasingFlags::Enable);

    const double rad = 5.5 * fS;
    const basegfx::B2DPolygon aTile = basegfx::utils::createPolygonFromRect(
        basegfx::B2DRectangle(1.0 * fS, 1.0 * fS, 23.0 * fS, 23.0 * fS),
        std::min(1.0, (2.0 * rad) / (22.0 * fS)),
        std::min(1.0, (2.0 * rad) / (22.0 * fS)));
    pVDev->SetLineColor();
    pVDev->SetFillColor(aStyle.aTileBg);
    pVDev->DrawPolygon(aTile);
    pVDev->SetFillColor();
    pVDev->SetLineColor(aStyle.aTileBorder);
    pVDev->DrawPolyLine(aTile, 1.0 * fS, basegfx::B2DLineJoin::Round);

    const double cx = 12.0 * fS;
    const double cy = 12.0 * fS;
    const double sw = 1.75 * fS;
    const Color c = aStyle.aFg;

    switch (nKind)
    {
        case 0: // PropertyDeck: sliders
            DrawPoly(*pVDev, { { cx - 6 * fS, cy - 4 * fS }, { cx + 6 * fS, cy - 4 * fS } }, false, c, sw);
            DrawPoly(*pVDev, { { cx - 6 * fS, cy }, { cx + 6 * fS, cy } }, false, c, sw);
            DrawPoly(*pVDev, { { cx - 6 * fS, cy + 4 * fS }, { cx + 6 * fS, cy + 4 * fS } }, false, c, sw);
            DrawCircle(*pVDev, cx - 2 * fS, cy - 4 * fS, 1.8 * fS, c, sw, true);
            DrawCircle(*pVDev, cx + 2.5 * fS, cy, 1.8 * fS, c, sw, true);
            DrawCircle(*pVDev, cx - 1 * fS, cy + 4 * fS, 1.8 * fS, c, sw, true);
            break;
        case 1: // StyleListDeck: bold 'A' + style lines
            DrawPoly(*pVDev, { { cx - 5.5 * fS, cy + 2.5 * fS }, { cx - 2.0 * fS, cy - 6.0 * fS }, { cx + 1.5 * fS, cy + 2.5 * fS } }, false, c, sw);
            DrawPoly(*pVDev, { { cx - 4.3 * fS, cy - 0.5 * fS }, { cx + 0.3 * fS, cy - 0.5 * fS } }, false, c, 1.5 * fS);
            DrawPoly(*pVDev, { { cx - 5.5 * fS, cy + 5.8 * fS }, { cx + 6.0 * fS, cy + 5.8 * fS } }, false, c, sw);
            DrawPoly(*pVDev, { { cx + 2.5 * fS, cy - 3.5 * fS }, { cx + 6.0 * fS, cy - 3.5 * fS } }, false, c, 1.5 * fS);
            DrawPoly(*pVDev, { { cx + 2.5 * fS, cy + 0.5 * fS }, { cx + 6.0 * fS, cy + 0.5 * fS } }, false, c, 1.5 * fS);
            break;
        case 2: // GalleryDeck: mountain + sun frame
            DrawPoly(*pVDev, { { cx - 6 * fS, cy - 5 * fS }, { cx + 6 * fS, cy - 5 * fS }, { cx + 6 * fS, cy + 5 * fS }, { cx - 6 * fS, cy + 5 * fS } }, true, c, sw);
            DrawPoly(*pVDev, { { cx - 5.5 * fS, cy + 4.5 * fS }, { cx - 1.5 * fS, cy - 0.5 * fS }, { cx + 1.5 * fS, cy + 2.5 * fS }, { cx + 3.5 * fS, cy + 0.5 * fS }, { cx + 5.5 * fS, cy + 4.5 * fS } }, false, c, 1.5 * fS);
            DrawCircle(*pVDev, cx + 2.8 * fS, cy - 2.3 * fS, 1.2 * fS, c, sw, true);
            break;
        case 3: // NavigatorDeck: compass
            DrawCircle(*pVDev, cx, cy, 6.2 * fS, c, sw);
            DrawPoly(*pVDev, { { cx - 2.8 * fS, cy + 2.8 * fS }, { cx - 0.8 * fS, cy - 0.8 * fS }, { cx + 2.8 * fS, cy - 2.8 * fS }, { cx + 0.8 * fS, cy + 0.8 * fS } }, true, c, 1.5 * fS);
            break;
        case 4: // WriterPageDeck: folded page
            DrawPoly(*pVDev, { { cx - 4.5 * fS, cy - 6 * fS }, { cx + 1.5 * fS, cy - 6 * fS }, { cx + 4.5 * fS, cy - 3 * fS }, { cx + 4.5 * fS, cy + 6 * fS }, { cx - 4.5 * fS, cy + 6 * fS } }, true, c, sw);
            DrawPoly(*pVDev, { { cx + 1.5 * fS, cy - 6 * fS }, { cx + 1.5 * fS, cy - 3 * fS }, { cx + 4.5 * fS, cy - 3 * fS } }, false, c, 1.4 * fS);
            break;
        case 5: // InspectorDeck: magnifier + inner lines
        case 10: // FindDeck: magnifier
            DrawCircle(*pVDev, cx - 1.5 * fS, cy - 1.5 * fS, 4.5 * fS, c, sw);
            DrawPoly(*pVDev, { { cx + 1.8 * fS, cy + 1.8 * fS }, { cx + 5.8 * fS, cy + 5.8 * fS } }, false, c, 2.0 * fS);
            if (nKind == 5)
                DrawPoly(*pVDev, { { cx - 3.2 * fS, cy - 1.5 * fS }, { cx + 0.2 * fS, cy - 1.5 * fS } }, false, c, 1.4 * fS);
            break;
        case 6: // SwManageChangesDeck: tracked edit lines + dot
            DrawPoly(*pVDev, { { cx - 5.5 * fS, cy - 4 * fS }, { cx + 5.5 * fS, cy - 4 * fS } }, false, c, sw);
            DrawPoly(*pVDev, { { cx - 5.5 * fS, cy }, { cx + 1.5 * fS, cy } }, false, c, sw);
            DrawPoly(*pVDev, { { cx - 5.5 * fS, cy + 4 * fS }, { cx + 5.5 * fS, cy + 4 * fS } }, false, c, sw);
            DrawCircle(*pVDev, cx + 4.5 * fS, cy, 1.6 * fS, c, sw, true);
            break;
        case 7: // SwDesignDeck: swatch/palette frame
            DrawPoly(*pVDev, { { cx - 5.5 * fS, cy - 5.5 * fS }, { cx + 5.5 * fS, cy - 5.5 * fS }, { cx + 5.5 * fS, cy + 5.5 * fS }, { cx - 5.5 * fS, cy + 5.5 * fS } }, true, c, sw);
            DrawPoly(*pVDev, { { cx - 5.5 * fS, cy - 1.5 * fS }, { cx + 5.5 * fS, cy - 1.5 * fS } }, false, c, 1.5 * fS);
            DrawPoly(*pVDev, { { cx - 1.5 * fS, cy - 1.5 * fS }, { cx - 1.5 * fS, cy + 5.5 * fS } }, false, c, 1.5 * fS);
            break;
        case 8: // A11yCheckDeck: check in circle
            DrawCircle(*pVDev, cx, cy, 6.2 * fS, c, sw);
            DrawPoly(*pVDev, { { cx - 3 * fS, cy }, { cx - 0.8 * fS, cy + 2.5 * fS }, { cx + 3.5 * fS, cy - 2.5 * fS } }, false, c, sw);
            break;
        case 9: // CommentsDeck: speech bubble
            DrawPoly(*pVDev, {
                { cx - 6 * fS, cy - 4.5 * fS }, { cx + 6 * fS, cy - 4.5 * fS },
                { cx + 6 * fS, cy + 2.5 * fS }, { cx - 0.5 * fS, cy + 2.5 * fS },
                { cx - 3.5 * fS, cy + 5.5 * fS }, { cx - 3.5 * fS, cy + 2.5 * fS },
                { cx - 6 * fS, cy + 2.5 * fS }
            }, true, c, sw);
            break;
        case 11: // ScFunctionsDeck: "fx"
        {
            vcl::Font aFont(Application::GetSettings().GetStyleSettings().GetLabelFont());
            aFont.SetFamilyName(u"Segoe UI"_ustr);
            aFont.SetFontHeight(static_cast<tools::Long>(std::lround(12.0 * fS)));
            aFont.SetWeight(WEIGHT_BOLD);
            aFont.SetTransparent(true);
            pVDev->SetFont(aFont);
            pVDev->SetTextColor(c);
            pVDev->DrawText(tools::Rectangle(Point(0, 0), Size(nSz, nSz)), u"fx"_ustr, DrawTextFlags::Center | DrawTextFlags::VCenter);
            break;
        }
        case 12: // ShapesDeck: circle + square + triangle
            DrawCircle(*pVDev, cx - 3 * fS, cy - 2.8 * fS, 2.8 * fS, c, 1.6 * fS);
            DrawPoly(*pVDev, { { cx + 0.5 * fS, cy - 5.2 * fS }, { cx + 5.8 * fS, cy - 5.2 * fS }, { cx + 5.8 * fS, cy + 0.1 * fS }, { cx + 0.5 * fS, cy + 0.1 * fS } }, true, c, 1.6 * fS);
            DrawPoly(*pVDev, { { cx - 0.5 * fS, cy + 5.8 * fS }, { cx + 2.8 * fS, cy + 0.8 * fS }, { cx + 6.0 * fS, cy + 5.8 * fS } }, true, c, 1.6 * fS);
            break;
        case 13: // SdSlideTransitionDeck: slide + motion lines
            DrawPoly(*pVDev, { { cx - 2.5 * fS, cy - 4.5 * fS }, { cx + 6 * fS, cy - 4.5 * fS }, { cx + 6 * fS, cy + 4.5 * fS }, { cx - 2.5 * fS, cy + 4.5 * fS } }, true, c, sw);
            DrawPoly(*pVDev, { { cx - 6 * fS, cy - 3 * fS }, { cx - 4 * fS, cy - 3 * fS } }, false, c, sw);
            DrawPoly(*pVDev, { { cx - 6.5 * fS, cy }, { cx - 4 * fS, cy } }, false, c, sw);
            DrawPoly(*pVDev, { { cx - 6 * fS, cy + 3 * fS }, { cx - 4 * fS, cy + 3 * fS } }, false, c, sw);
            break;
        case 14: // SdCustomAnimationDeck: 4-point star
            DrawPoly(*pVDev, {
                { cx, cy - 6.5 * fS }, { cx + 1.8 * fS, cy - 1.8 * fS },
                { cx + 6.5 * fS, cy }, { cx + 1.8 * fS, cy + 1.8 * fS },
                { cx, cy + 6.5 * fS }, { cx - 1.8 * fS, cy + 1.8 * fS },
                { cx - 6.5 * fS, cy }, { cx - 1.8 * fS, cy - 1.8 * fS }
            }, true, c, 1.6 * fS);
            break;
        case 15: // SdMasterPagesDeck: overlapping stacked slides
            DrawPoly(*pVDev, { { cx - 5.5 * fS, cy - 5 * fS }, { cx + 3.5 * fS, cy - 5 * fS }, { cx + 3.5 * fS, cy + 2 * fS }, { cx - 5.5 * fS, cy + 2 * fS } }, true, c, 1.6 * fS);
            DrawPoly(*pVDev, { { cx - 2.5 * fS, cy + 5 * fS }, { cx + 5.5 * fS, cy + 5 * fS }, { cx + 5.5 * fS, cy - 2 * fS } }, false, c, 1.6 * fS);
            break;
        case 16: // ElementsDeck: Sigma
            DrawPoly(*pVDev, {
                { cx + 4.5 * fS, cy - 5.5 * fS }, { cx - 4.5 * fS, cy - 5.5 * fS },
                { cx + 0.8 * fS, cy }, { cx - 4.5 * fS, cy + 5.5 * fS },
                { cx + 4.5 * fS, cy + 5.5 * fS }
            }, false, c, sw);
            break;
    }

    return Graphic(pVDev->GetBitmap(Point(0, 0), Size(nSz, nSz))).GetXGraphic();
}
} // namespace

css::uno::Reference<css::graphic::XGraphic> TabBar::GetItemImage(const DeckDescriptor& rDeckDescriptor) const
{
    if (!Theme::IsHighContrastMode())
    {
        if (auto xCustom = MakeColorfulDeckGraphic(rDeckDescriptor.msId, GetDPIScaleFactor()))
            return xCustom;
    }
    return Tools::GetImage(
        rDeckDescriptor.msIconURL,
        rDeckDescriptor.msHighContrastIconURL,
        mxFrame);
}

TabBar::Item::Item(TabBar& rTabBar)
    : mrTabBar(rTabBar)
    , mxBuilder(Application::CreateBuilder(rTabBar.GetContainer(), u"sfx/ui/tabbutton.ui"_ustr))
    , mxButton(mxBuilder->weld_toolbar(u"button"_ustr))
    , mbIsHidden(false)
{
}

TabBar::Item::~Item()
{
    mrTabBar.GetContainer()->move(mxButton.get(), nullptr);
}

IMPL_LINK_NOARG(TabBar::Item, HandleClick, const OUString&, void)
{
    // tdf#143146 copy the functor and arg before calling
    // GrabFocusToDocument which may destroy this object
    DeckActivationFunctor aDeckActivationFunctor = maDeckActivationFunctor;
    auto sDeckId = msDeckId;

    mrTabBar.GrabFocusToDocument();
    try
    {
        aDeckActivationFunctor(sDeckId);
    }
    catch(const css::uno::Exception&)
    {} // workaround for #i123198#
}

OUString const & TabBar::GetDeckIdForIndex (const sal_Int32 nIndex) const
{
    if (nIndex<0 || o3tl::make_unsigned(nIndex)>=maItems.size())
        throw RuntimeException();
    return maItems[nIndex]->msDeckId;
}

void TabBar::ToggleHideFlag (const sal_Int32 nIndex)
{
    if (nIndex<0 || o3tl::make_unsigned(nIndex) >= maItems.size())
        throw RuntimeException();

    maItems[nIndex]->mbIsHidden = ! maItems[nIndex]->mbIsHidden;

    std::shared_ptr<DeckDescriptor> xDeckDescriptor = mrParentSidebarController.GetResourceManager()->GetDeckDescriptor(maItems[nIndex]->msDeckId);
    if (xDeckDescriptor)
    {
        xDeckDescriptor->mbIsEnabled = ! maItems[nIndex]->mbIsHidden;

        Context aContext;
        aContext.msApplication = mrParentSidebarController.GetCurrentContext().msApplication;
        // leave aContext.msContext on default 'any' ... this func is used only for decks
        // and we don't have context-sensitive decks anyway

        xDeckDescriptor->maContextList.ToggleVisibilityForContext(
            aContext, xDeckDescriptor->mbIsEnabled );
    }
    UpdateMenus();
}

void TabBar::UpdateFocusManager(FocusManager& rFocusManager)
{
    std::vector<weld::Widget*> aButtons;
    aButtons.reserve(maItems.size()+1);
    aButtons.push_back(mxMenuButton.get());
    for (auto const& item : maItems)
    {
        aButtons.push_back(item->mxButton.get());
    }
    rFocusManager.SetButtons(aButtons);
}

void TabBar::UpdateMenus()
{
    if (Application::GetToolkitName() == u"gtk4"_ustr)
    {
        SAL_WARN("sfx", "Skipping update of sidebar menus to avoid crash due to gtk4 menu brokenness.");
        return;
    }

    for (int i = mxMainMenu->n_children() - 1; i >= 0; --i)
    {
        OUString sIdent = mxMainMenu->get_id(i);
        if (sIdent.startsWith("select"))
            mxMainMenu->remove(sIdent);
    }
    for (int i = mxSubMenu->n_children() - 1; i >= 0; --i)
    {
        OUString sIdent = mxSubMenu->get_id(i);
        if (sIdent.indexOf("customize") != -1)
            mxSubMenu->remove(sIdent);
    }

    // Add one entry for every tool panel element to individually make
    // them visible or hide them.
    sal_Int32 nIndex (0);
    for (auto const& rItem : maItems)
    {
        std::shared_ptr<DeckDescriptor> xDeckDescriptor
            = mrParentSidebarController.GetResourceManager()->GetDeckDescriptor(rItem->msDeckId);

        if (!xDeckDescriptor)
            continue;

        const OUString sDisplayName = xDeckDescriptor->msTitle;
        OUString sIdent("select" + OUString::number(nIndex));
        const bool bCurrentDeck = rItem->mxButton->get_item_active(u"toggle"_ustr);
        const bool bActive = !rItem->mbIsHidden;
        const bool bEnabled = rItem->mxButton->get_visible();
        mxMainMenu->insert(nIndex, sIdent, sDisplayName, nullptr, nullptr, nullptr, TRISTATE_FALSE);
        mxMainMenu->set_active(sIdent, bCurrentDeck);
        mxMainMenu->set_sensitive(sIdent, bEnabled && bActive);

        if (!comphelper::LibreOfficeKit::isActive())
        {
            if (bCurrentDeck)
            {
                // Don't allow the currently visible deck to be disabled.
                OUString sSubIdent("nocustomize" + OUString::number(nIndex));
                mxSubMenu->insert(nIndex, sSubIdent, sDisplayName, nullptr, nullptr, nullptr,
                                  TRISTATE_FALSE);
                mxSubMenu->set_active(sSubIdent, true);
            }
            else
            {
                OUString sSubIdent("customize" + OUString::number(nIndex));
                mxSubMenu->insert(nIndex, sSubIdent, sDisplayName, nullptr, nullptr, nullptr,
                                  TRISTATE_TRUE);
                mxSubMenu->set_active(sSubIdent, bEnabled && bActive);
            }
        }

        ++nIndex;
    }

    bool bHideLock = true;
    bool bHideUnLock = true;
    // LOK doesn't support docked/undocked; Sidebar is floating but rendered docked in browser.
    if (!comphelper::LibreOfficeKit::isActive())
    {
        // Add entry for docking or un-docking the tool panel.
        if (!mrParentSidebarController.IsDocked())
            bHideLock = false;
        else
            bHideUnLock = false;
    }
    mxMainMenu->set_visible(u"locktaskpanel"_ustr, !bHideLock);
    mxMainMenu->set_visible(u"unlocktaskpanel"_ustr, !bHideUnLock);

    // No Restore or Customize options for LoKit.
    mxMainMenu->set_visible(u"customization"_ustr, !comphelper::LibreOfficeKit::isActive());
}

void TabBar::EnableMenuButton(const bool bEnable)
{
    mxMenuButton->set_sensitive(bEnable);
}

FactoryFunction TabBar::GetUITestFactory() const
{
    return TabBarUIObject::create;
}

} // end of namespace sfx2::sidebar

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
