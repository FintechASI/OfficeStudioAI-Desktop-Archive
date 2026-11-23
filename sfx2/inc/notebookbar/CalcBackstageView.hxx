/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <com/sun/star/frame/XFrame.hpp>
#include <tools/gen.hxx>
#include <tools/link.hxx>
#include <vcl/ctrl.hxx>
#include <vector>

enum class CalcBackstageSection
{
    Recent,
    New,
    Open,
    SaveAs,
    Print,
    Export
};

class CalcBackstageView final : public Control
{
public:
    CalcBackstageView(vcl::Window* pParent,
                      css::uno::Reference<css::frame::XFrame> xFrame,
                      Link<CalcBackstageView*, void> aCloseHdl);
    virtual ~CalcBackstageView() override;
    virtual void dispose() override;

    void RefreshData();
    void SetSection(CalcBackstageSection eSection);

    virtual void Paint(vcl::RenderContext& rRenderContext, const tools::Rectangle& rRect) override;
    virtual void MouseMove(const MouseEvent& rMEvt) override;
    virtual void MouseButtonDown(const MouseEvent& rMEvt) override;
    virtual void KeyInput(const KeyEvent& rKEvt) override;
    virtual void Command(const CommandEvent& rCEvt) override;
    virtual void Resize() override;

private:
    enum class TargetKind
    {
        CloseBackstage,
        SelectSection,
        FilterCalcOnly,
        FilterAllFiles,
        DispatchCommand,
        OpenRecentFile
    };

    struct HitTarget
    {
        int nId = -1;
        TargetKind eKind = TargetKind::CloseBackstage;
        CalcBackstageSection eSection = CalcBackstageSection::Recent;
        tools::Rectangle aRect;
        OUString sCommand;
        OUString sFileUrl;
        bool bReadOnly = false;
    };

    struct RecentEntry
    {
        OUString sName;
        OUString sFolder;
        OUString sExt;
        OUString sURL;
        bool bReadOnly = false;
        bool bPinned = false;
        bool bIsCalc = false;
    };

    void RebuildTargets(const Size& rSize, double fScale);
    void PostDispatchCommand(const OUString& rCommand);
    void PostOpenRecentFile(const OUString& rURL, bool bReadOnly);
    const HitTarget* FindHitTarget(const Point& rPos) const;

    css::uno::Reference<css::frame::XFrame> mxFrame;
    Link<CalcBackstageView*, void> maCloseHdl;
    CalcBackstageSection meSection = CalcBackstageSection::Recent;
    bool mbOnlySpreadsheets = true;
    OUString msDocTitle;
    OUString msDocPath;
    bool mbDocModified = false;
    tools::Long mnScrollY = 0;
    tools::Long mnMaxScrollY = 0;
    tools::Rectangle maRecentScrollBounds;
    int mnHoverId = -1;
    std::vector<HitTarget> maHitTargets;
    std::vector<RecentEntry> maRecentEntries;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */

