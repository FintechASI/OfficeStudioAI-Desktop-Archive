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

#include "AppIconControl.hxx"
#include <core_resource.hxx>
#include <strings.hrc>
#include <bitmaps.hlst>
#include <sfx2/thumbnailviewitem.hxx>
#include <cmath>
#include <vcl/bitmap.hxx>
#include <vcl/event.hxx>
#include <vcl/i18nhelp.hxx>
#include <vcl/mnemonic.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/settings.hxx>
#include <vcl/svapp.hxx>
#include <vcl/virdev.hxx>
#include <callbacks.hxx>
#include <AppElementType.hxx>

namespace dbaui
{
class OApplicationIconControlDropTarget final : public DropTargetHelper
{
private:
    OApplicationIconControl& m_rControl;

public:
    OApplicationIconControlDropTarget(OApplicationIconControl& rControl)
        : DropTargetHelper(rControl.GetDrawingArea()->get_drop_target())
        , m_rControl(rControl)
    {
    }

    virtual sal_Int8 AcceptDrop(const AcceptDropEvent& rEvt) override
    {
        return m_rControl.AcceptDrop(rEvt);
    }

    virtual sal_Int8 ExecuteDrop(const ExecuteDropEvent& rEvt) override
    {
        return m_rControl.ExecuteDrop(rEvt);
    }
};

namespace
{
Bitmap MakeCategoryBitmap(ElementType eType, float fScale)
{
    const double fS = std::max(1.0, static_cast<double>(fScale));
    const tools::Long nSz = static_cast<tools::Long>(std::lround(36.0 * fS));
    ScopedVclPtrInstance<VirtualDevice> pDev(DeviceFormat::WITH_ALPHA);
    pDev->SetOutputSizePixel(Size(nSz, nSz), true, true);
    pDev->SetAntialiasing(AntialiasingFlags::Enable);

    Color aFg(0x25, 0x63, 0xEB);
    Color aBg(0xEF, 0xF6, 0xFF);
    Color aBorder(0xBF, 0xDB, 0xFE);
    if (eType == E_TABLE)
    {
        aFg = Color(0xDC, 0x26, 0x26);
        aBg = Color(0xFE, 0xF2, 0xF2);
        aBorder = Color(0xFE, 0xCA, 0xCA);
    }
    else if (eType == E_FORM)
    {
        aFg = Color(0x16, 0xA3, 0x4A);
        aBg = Color(0xF0, 0xFD, 0xF4);
        aBorder = Color(0xBB, 0xF7, 0xD0);
    }
    else if (eType == E_REPORT)
    {
        aFg = Color(0x7C, 0x3A, 0xED);
        aBg = Color(0xF5, 0xF3, 0xFF);
        aBorder = Color(0xDD, 0xD6, 0xFE);
    }

    auto px = [fS](double v) -> tools::Long { return static_cast<tools::Long>(std::lround(v * fS)); };
    const tools::Long nPad = px(2.0);
    const tools::Long nRad = px(7.0);
    pDev->SetLineColor(aBorder);
    pDev->SetFillColor(aBg);
    pDev->DrawRect(tools::Rectangle(Point(nPad, nPad), Size(nSz - 2 * nPad, nSz - 2 * nPad)), nRad, nRad);

    pDev->SetLineColor(aFg);
    pDev->SetFillColor();
    if (eType == E_TABLE) // 3-band cylinder
    {
        const tools::Long nLeft = px(9.0), nRight = px(27.0);
        const tools::Long nW = nRight - nLeft;
        const tools::Long nEllH = px(5.0);
        pDev->SetFillColor(COL_WHITE);
        pDev->DrawEllipse(tools::Rectangle(Point(nLeft, px(8.0)), Size(nW, nEllH)));
        pDev->SetFillColor();
        pDev->DrawLine(Point(nLeft, px(10.5)), Point(nLeft, px(24.5)));
        pDev->DrawLine(Point(nRight, px(10.5)), Point(nRight, px(24.5)));
        pDev->DrawArc(tools::Rectangle(Point(nLeft, px(13.0)), Size(nW, nEllH)), Point(nLeft, px(15.5)), Point(nRight, px(15.5)));
        pDev->DrawArc(tools::Rectangle(Point(nLeft, px(18.0)), Size(nW, nEllH)), Point(nLeft, px(20.5)), Point(nRight, px(20.5)));
        pDev->DrawArc(tools::Rectangle(Point(nLeft, px(22.0)), Size(nW, nEllH)), Point(nLeft, px(24.5)), Point(nRight, px(24.5)));
    }
    else if (eType == E_QUERY) // Query table + filter lens
    {
        pDev->SetFillColor(COL_WHITE);
        pDev->DrawRect(tools::Rectangle(Point(px(9.0), px(9.0)), Size(px(18.0), px(18.0))), px(2.0), px(2.0));
        pDev->DrawLine(Point(px(9.0), px(15.0)), Point(px(27.0), px(15.0)));
        pDev->DrawLine(Point(px(9.0), px(21.0)), Point(px(27.0), px(21.0)));
        pDev->DrawLine(Point(px(15.0), px(9.0)), Point(px(15.0), px(27.0)));
        pDev->SetFillColor(aFg);
        pDev->DrawEllipse(tools::Rectangle(Point(px(19.0), px(19.0)), Size(px(7.0), px(7.0))));
    }
    else if (eType == E_FORM) // Form card with fields
    {
        pDev->SetFillColor(COL_WHITE);
        pDev->DrawRect(tools::Rectangle(Point(px(8.0), px(9.0)), Size(px(20.0), px(18.0))), px(2.5), px(2.5));
        pDev->DrawLine(Point(px(8.0), px(14.0)), Point(px(28.0), px(14.0)));
        pDev->DrawRect(tools::Rectangle(Point(px(11.0), px(17.0)), Size(px(14.0), px(3.0))));
        pDev->DrawRect(tools::Rectangle(Point(px(11.0), px(22.0)), Size(px(9.0), px(3.0))));
    }
    else // E_REPORT: bar chart document
    {
        pDev->SetFillColor(COL_WHITE);
        pDev->DrawRect(tools::Rectangle(Point(px(9.0), px(8.0)), Size(px(18.0), px(20.0))), px(2.0), px(2.0));
        pDev->SetFillColor(aFg);
        pDev->DrawRect(tools::Rectangle(Point(px(12.0), px(19.0)), Size(px(3.0), px(6.0))));
        pDev->DrawRect(tools::Rectangle(Point(px(16.5), px(14.0)), Size(px(3.0), px(11.0))));
        pDev->DrawRect(tools::Rectangle(Point(px(21.0), px(16.5)), Size(px(3.0), px(8.5))));
    }

    return pDev->GetBitmap(Point(0, 0), Size(nSz, nSz));
}
} // namespace

OApplicationIconControl::OApplicationIconControl(std::unique_ptr<weld::ScrolledWindow> xScroll)
    : ThumbnailView(std::move(xScroll))
    , m_pActionListener(nullptr)
    , m_nMaxWidth(0)
    , m_nMaxHeight(0)
{
    mnVItemSpace = 6; // row spacing
    mbSelectOnFocus = false;
    DrawMnemonics(true);
    UpdateColors(Application::GetSettings().GetStyleSettings());
}

void OApplicationIconControl::UpdateColors(const StyleSettings& rSettings)
{
    ThumbnailView::UpdateColors(rSettings);
    if (!rSettings.GetHighContrastMode() && !rSettings.GetDialogColor().IsDark())
    {
        maFillColor = Color(0xF8, 0xFA, 0xFC);
        maTextColor = Color(0x0F, 0x1F, 0x3D);
        maHighlightColor = Color(0xDB, 0xEA, 0xFE);
        maHighlightTextColor = Color(0x0F, 0x1F, 0x3D);
    }
}

void OApplicationIconControl::Fill()
{
    static constexpr struct CategoryDescriptor
    {
        TranslateId pLabelResId;
        ElementType eType;
        OUString aImageResId;
    } aCategories[] = { { RID_STR_TABLES_CONTAINER, E_TABLE, BMP_TABLEFOLDER_TREE_L },
                        { RID_STR_QUERIES_CONTAINER, E_QUERY, BMP_QUERYFOLDER_TREE_L },
                        { RID_STR_FORMS_CONTAINER, E_FORM, BMP_FORMFOLDER_TREE_L },
                        { RID_STR_REPORTS_CONTAINER, E_REPORT, BMP_REPORTFOLDER_TREE_L } };

    const bool bUseColorful = !Application::GetSettings().GetStyleSettings().GetHighContrastMode();
    const float fScale = GetDrawingArea() ? GetDrawingArea()->get_ref_device().GetDPIScaleFactor() : 1.0f;

    for (const CategoryDescriptor& aCategory : aCategories)
    {
        // E_TABLE is 0, but 0 means void so use id of enum + 1
        std::unique_ptr<ThumbnailViewItem> xItem(new ThumbnailViewItem(*this, aCategory.eType + 1));
        xItem->mbBorder = false;
        if (bUseColorful)
            xItem->maPreview = MakeCategoryBitmap(aCategory.eType, fScale);
        if (xItem->maPreview.IsEmpty())
            xItem->maPreview = Bitmap(aCategory.aImageResId);
        const Size aSize = xItem->maPreview.GetSizePixel();
        m_nMaxWidth = std::max(m_nMaxWidth, aSize.Width());
        m_nMaxHeight = std::max(m_nMaxHeight, aSize.Height());
        xItem->maTitle = DBA_RES(aCategory.pLabelResId);
        m_nMaxWidth = std::max<tools::Long>(m_nMaxWidth, GetTextWidth(xItem->maTitle));
        AppendItem(std::move(xItem));
    }

    const int nMargin = 12;
    const int nWidthRequest = m_nMaxWidth + 2 * nMargin;
    set_size_request(nWidthRequest, -1);
    // we expect a Resize at which point we'll set the item sizes based on our final size
}

ElementType OApplicationIconControl::GetSelectedItem() const
{
    for (const auto& rItem : mItemList)
    {
        if (!rItem->mbSelected)
            continue;
        return static_cast<ElementType>(rItem->mnId - 1);
    }
    return E_NONE;
}

void OApplicationIconControl::createIconAutoMnemonics(MnemonicGenerator& rMnemonics)
{
    for (const auto& rItem : mItemList)
        rMnemonics.RegisterMnemonic(rItem->maTitle);

    // exchange texts with generated mnemonics
    for (auto& rItem : mItemList)
        rItem->maTitle = rMnemonics.CreateMnemonic(rItem->maTitle);
}

void OApplicationIconControl::Resize()
{
    // fill the full width of the allocated area and give two lines of space to
    // center the title in
    setItemDimensions(GetOutputSizePixel().Width(), m_nMaxHeight, GetTextHeight() * 2, 0);
    ThumbnailView::Resize();
}

bool OApplicationIconControl::IsMnemonicChar(sal_Unicode cChar, ElementType& rType) const
{
    bool bRet = false;

    const vcl::I18nHelper& rI18nHelper = Application::GetSettings().GetUILocaleI18nHelper();
    for (const auto& rItem : mItemList)
    {
        if (rI18nHelper.MatchMnemonic(rItem->maTitle, cChar))
        {
            bRet = true;
            rType = static_cast<ElementType>(rItem->mnId - 1);
            break;
        }
    }

    return bRet;
}

bool OApplicationIconControl::DoKeyShortCut(const KeyEvent& rKEvt)
{
    bool bMod2 = rKEvt.GetKeyCode().IsMod2();
    sal_Unicode cChar = rKEvt.GetCharCode();
    ElementType eType(E_NONE);
    if (bMod2 && cChar && IsMnemonicChar(cChar, eType))
    {
        // shortcut is clicked
        deselectItems();
        SelectItem(eType + 1);
        return true;
    }

    return false;
}

bool OApplicationIconControl::KeyInput(const KeyEvent& rKEvt)
{
    return DoKeyShortCut(rKEvt) || ThumbnailView::KeyInput(rKEvt);
}

void OApplicationIconControl::SetDrawingArea(weld::DrawingArea* pDrawingArea)
{
    ThumbnailView::SetDrawingArea(pDrawingArea);
    m_xDropTarget.reset(new OApplicationIconControlDropTarget(*this));
}

sal_Int8 OApplicationIconControl::AcceptDrop(const AcceptDropEvent& rEvt)
{
    sal_Int8 nDropOption = DND_ACTION_NONE;
    if (m_pActionListener)
    {
        sal_uInt16 nEntry = GetItemId(rEvt.maPosPixel);
        if (nEntry)
        {
            deselectItems();
            SelectItem(nEntry);
            nDropOption
                = m_pActionListener->queryDrop(rEvt, m_xDropTarget->GetDataFlavorExVector());
        }
    }
    return nDropOption;
}

sal_Int8 OApplicationIconControl::ExecuteDrop(const ExecuteDropEvent& rEvt)
{
    if (m_pActionListener)
        m_pActionListener->executeDrop(rEvt);
    return DND_ACTION_NONE;
}

OApplicationIconControl::~OApplicationIconControl() {}

void OApplicationIconControl::GetFocus()
{
    ThumbnailView::GetFocus();
    Invalidate(); // redraw focus rect
}

void OApplicationIconControl::LoseFocus()
{
    ThumbnailView::LoseFocus();
    Invalidate(); // redraw focus rect
}

tools::Rectangle OApplicationIconControl::GetFocusRect()
{
    if (HasFocus())
    {
        // Get the last selected item in the list
        for (tools::Long i = mFilteredItemList.size() - 1; i >= 0; --i)
        {
            ThumbnailViewItem* pItem = mFilteredItemList[i];
            if (pItem->isSelected())
            {
                tools::Rectangle aRet(pItem->getDrawArea());
                aRet.AdjustLeft(THUMBNAILVIEW_ITEM_CORNER);
                aRet.AdjustTop(1);
                aRet.AdjustRight(-THUMBNAILVIEW_ITEM_CORNER);
                aRet.AdjustBottom(-2);
                return aRet;
            }
        }
    }
    return tools::Rectangle();
}
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
