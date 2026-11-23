/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <chrono>
#include <memory>
#include <svl/lstner.hxx>
#include <vcl/tabctrl.hxx>
#include <vcl/vclevent.hxx>

#include <com/sun/star/frame/XFrame.hpp>

namespace com::sun::star::uno { class XComponentContext; }
class ChangedUIEventListener;
class CalcBackstageView;
class CommandPopupHandler;

class NotebookbarTabControl final : public NotebookbarTabControlBase, public SfxListener
{
friend class ChangedUIEventListener;

public:
    NotebookbarTabControl( Window* pParent );
    ~NotebookbarTabControl() override;
    virtual void dispose() override;

    virtual void KeyInput( const KeyEvent& rKEvt ) override;
    virtual void Command( const CommandEvent& rCEvt ) override;
    virtual bool EventNotify( NotifyEvent& rNEvt ) override;
    virtual void StateChanged(StateChangedType nStateChange) override;
    virtual void Resize() override;
    virtual Size calculateRequisition() const override;
    virtual void Notify( SfxBroadcaster& rBC, const SfxHint& rHint ) override;

private:
    // time-based debounce: track when the last tab switch via scroll happened
    std::chrono::steady_clock::time_point m_lastTabSwitch{};
    static void FillShortcutsToolBox(css::uno::Reference<css::uno::XComponentContext> const & xContext,
                                          const css::uno::Reference<css::frame::XFrame>& xFrame,
                                          const OUString& aModuleName,
                                          ToolBox* pShortcuts
                             );
    void ArrowStops( sal_uInt16 nCode );
    void UpdateDocumentTitle();
    void LayoutBackstage();
    void OpenBackstage();
    void CloseBackstage();
    void OpenHeaderSearch();
    vcl::Window* GetHostBorderWindow() const;

    DECL_LINK(OpenNotebookbarPopupMenu, NotebookBar*, void);
    DECL_LINK(OnFileTabClick, NotebookbarTabControlBase*, void);
    DECL_LINK(OnSearchClick, NotebookbarTabControlBase*, void);
    DECL_LINK(OnTitleClick, NotebookbarTabControlBase*, void);
    DECL_LINK(OnBackstageClose, CalcBackstageView*, void);
    DECL_LINK(OnActivateTabPage, TabControl*, void);
    DECL_LINK(BorderWinEventHdl, VclWindowEvent&, void);

    rtl::Reference<ChangedUIEventListener> m_pListener;
    css::uno::Reference<css::frame::XFrame> m_xFrame;
    VclPtr<CalcBackstageView> m_pBackstage;
    VclPtr<vcl::Window> m_pHostBorderWin;
    std::unique_ptr<CommandPopupHandler> m_pCommandPopup;
    sal_uInt16 m_nLastNonFilePageId = 0;
    bool m_bIsCalc = false;
    bool m_bInitialized;
    bool m_bInvalidate;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
