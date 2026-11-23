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

#include <chrono>
#include <vcl/builderfactory.hxx>
#include <vcl/commandevent.hxx>
#include <vcl/layout.hxx>
#include <vcl/mnemonic.hxx>
#include <vcl/notebookbar/notebookbar.hxx>
#include <vcl/svapp.hxx>
#include <vcl/tabpage.hxx>
#include <vcl/themecolors.hxx>
#include <vcl/OfficeStudioTheme.hxx>
#include <vcl/weld/weldutils.hxx>
#include <sfx2/app.hxx>
#include <sfx2/event.hxx>
#include <sfx2/objsh.hxx>
#include <sfx2/viewfrm.hxx>
#include <commandpopup/CommandPopup.hxx>
#include <notebookbar/CalcBackstageView.hxx>
#include <notebookbar/NotebookbarTabControl.hxx>
#include <com/sun/star/ui/theModuleUIConfigurationManagerSupplier.hpp>
#include <com/sun/star/ui/ItemType.hpp>
#include <com/sun/star/frame/XModuleManager.hpp>
#include <com/sun/star/frame/ModuleManager.hpp>
#include <com/sun/star/frame/XFrame.hpp>
#include <com/sun/star/uno/Reference.h>
#include <com/sun/star/awt/PopupMenuDirection.hpp>
#include <com/sun/star/awt/XVclWindowPeer.hpp>
#include <com/sun/star/frame/XPopupMenuController.hpp>
#include <comphelper/dispatchcommand.hxx>
#include <comphelper/processfactory.hxx>
#include <comphelper/propertyvalue.hxx>
#include <sidebar/SidebarToolBox.hxx>
#include <toolkit/awt/vclxmenu.hxx>
#include <cppuhelper/implbase.hxx>

#define ICON_SIZE 22
constexpr OUString TOOLBAR_STR = u"private:resource/toolbar/notebookbarshortcuts"_ustr;

using namespace css::uno;
using namespace css::ui;
using namespace css::frame;

class ChangedUIEventListener : public ::cppu::WeakImplHelper<XUIConfigurationListener>
{
    VclPtr<NotebookbarTabControl> m_pParent;

public:
    explicit ChangedUIEventListener(NotebookbarTabControl *p)
    : m_pParent(p)
    {
        try
        {
            if (SfxViewFrame* pViewFrm = SfxViewFrame::Current())
            {
                const Reference<XComponentContext>& xContext = comphelper::getProcessComponentContext();
                const Reference<XModuleManager> xModuleManager  = ModuleManager::create( xContext );
                Reference<XFrame> xFrame = pViewFrm->GetFrame().GetFrameInterface();
                OUString aModuleName = xModuleManager->identify( xFrame );

                Reference<XUIConfigurationManager> m_xConfigManager;
                Reference<XModuleUIConfigurationManagerSupplier > xModuleCfgMgrSupplier(
                    theModuleUIConfigurationManagerSupplier::get( xContext ) );
                m_xConfigManager.set( xModuleCfgMgrSupplier->getUIConfigurationManager( aModuleName ) );
                css::uno::Reference< css::ui::XUIConfiguration > xConfig( m_xConfigManager, css::uno::UNO_QUERY_THROW );
                xConfig->addConfigurationListener( this );
            }
        }
        catch( const css::uno::RuntimeException& ) {}
    }

    // XUIConfigurationListener
    virtual void SAL_CALL elementInserted( const ConfigurationEvent& rEvent ) override
    {
        if( rEvent.ResourceURL == TOOLBAR_STR )
        {
            m_pParent->m_bInvalidate = true;
            m_pParent->StateChanged(StateChangedType::UpdateMode);
        }
    }

    virtual void SAL_CALL elementRemoved( const ConfigurationEvent& rEvent ) override
    {
        elementInserted( rEvent );
    }

    virtual void SAL_CALL elementReplaced( const ConfigurationEvent& rEvent ) override
    {
        elementInserted( rEvent );
    }

    virtual void SAL_CALL disposing(const ::css::lang::EventObject&) override
    {
        try
        {
            if (SfxViewFrame* pViewFrm = SfxViewFrame::Current())
            {
                const Reference<XComponentContext>& xContext = comphelper::getProcessComponentContext();
                const Reference<XModuleManager> xModuleManager  = ModuleManager::create( xContext );
                Reference<XFrame> xFrame = pViewFrm->GetFrame().GetFrameInterface();
                OUString aModuleName = xModuleManager->identify( xFrame );

                Reference<XUIConfigurationManager> m_xConfigManager;
                Reference<XModuleUIConfigurationManagerSupplier > xModuleCfgMgrSupplier(
                    theModuleUIConfigurationManagerSupplier::get( xContext ) );
                m_xConfigManager.set( xModuleCfgMgrSupplier->getUIConfigurationManager( aModuleName ) );
                css::uno::Reference< css::ui::XUIConfiguration > xConfig( m_xConfigManager, css::uno::UNO_QUERY_THROW );
                xConfig->removeConfigurationListener( this );
            }
        }
        catch( const css::uno::RuntimeException& ) {}

        m_pParent.reset();
    }
};

namespace {

class ShortcutsToolBox : public sfx2::sidebar::SidebarToolBox
{
public:
    ShortcutsToolBox( Window* pParent )
    : sfx2::sidebar::SidebarToolBox( pParent )
    {
        mbUseDefaultButtonSize = false;
        mbSideBar = false;
        SetToolboxButtonSize(ToolBoxButtonSize::Small);
    }

    virtual void KeyInput( const KeyEvent& rKEvt ) override
    {
        if ( rKEvt.GetKeyCode().IsMod1() )
        {
            sal_uInt16 nCode( rKEvt.GetKeyCode().GetCode() );
            if ( nCode == KEY_RIGHT || nCode == KEY_LEFT )
            {
                GetParent()->KeyInput( rKEvt );
                return;
            }
        }
        return sfx2::sidebar::SidebarToolBox::KeyInput( rKEvt );
    }
};

}

NotebookbarTabControl::NotebookbarTabControl( Window* pParent )
: NotebookbarTabControlBase( pParent )
, m_bInitialized( false )
, m_bInvalidate( true )
{
}

NotebookbarTabControl::~NotebookbarTabControl()
{
    disposeOnce();
}

void NotebookbarTabControl::dispose()
{
    EndListeningAll();
    if (m_pHostBorderWin && !m_pHostBorderWin->isDisposed())
        m_pHostBorderWin->RemoveEventListener(LINK(this, NotebookbarTabControl, BorderWinEventHdl));
    m_pHostBorderWin.reset();
    m_pBackstage.disposeAndClear();
    m_pCommandPopup.reset();
    NotebookbarTabControlBase::dispose();
}

void NotebookbarTabControl::ArrowStops( sal_uInt16 nCode )
{
    ToolBox* pToolBox( GetToolBox() );
    Control* pOpenMenu( GetOpenMenu() );

    if ( nCode == KEY_LEFT )
    {
        if ( HasFocus() )
        {
            if ( pToolBox )
                pToolBox->GrabFocus();
            else if ( pOpenMenu )
                pOpenMenu->GrabFocus();
        }
        else if ( pToolBox && pToolBox->HasFocus() )
        {
            if ( pOpenMenu )
                pOpenMenu->GrabFocus();
            else
                GrabFocus();
        }
        else if ( pOpenMenu && pOpenMenu->HasFocus() )
        {
            GrabFocus();
        }
    }
    else if ( nCode == KEY_RIGHT )
    {
        if ( HasFocus() )
        {
            if ( pOpenMenu )
                pOpenMenu->GrabFocus();
            else if ( pToolBox )
                pToolBox->GrabFocus();
        }
        else if ( pToolBox && pToolBox->HasFocus() )
        {
            GrabFocus();
        }
        else if ( pOpenMenu && pOpenMenu->HasFocus() )
        {
            if ( pToolBox )
                pToolBox->GrabFocus();
            else
                GrabFocus();
        }
    }
}

void NotebookbarTabControl::KeyInput( const KeyEvent& rKEvt )
{
    if ( rKEvt.GetKeyCode().IsMod1() )
    {
        sal_uInt16 nCode( rKEvt.GetKeyCode().GetCode() );
        if ( nCode == KEY_RIGHT || nCode == KEY_LEFT )
        {
            ArrowStops( nCode );
            return;
        }
    }
    return NotebookbarTabControlBase::KeyInput( rKEvt );
}

void NotebookbarTabControl::Command( const CommandEvent& rCEvt )
{
    if ( rCEvt.GetCommand() == CommandEventId::Wheel )
    {
        const CommandWheelData* pData = rCEvt.GetWheelData();
        if ( pData && !pData->GetModifier()
             && pData->GetMode() == CommandWheelMode::SCROLL
             && pData->GetDelta() != 0 )
        {
            // Debounce: require at least 100 ms between tab switches so that
            // casual scrolling does not rapidly cycle through tabs.
            auto now = std::chrono::steady_clock::now();
            static constexpr auto debounceMs = std::chrono::milliseconds(100);
            if ( now - m_lastTabSwitch >= debounceMs )
            {
                m_lastTabSwitch = now;
                ImplActivateTabPage( pData->GetDelta() < 0 );
            }
            return;
        }
    }
    NotebookbarTabControlBase::Command( rCEvt );
}

bool NotebookbarTabControl::EventNotify( NotifyEvent& rNEvt )
{
    if ( rNEvt.GetType() == NotifyEventType::KEYINPUT )
    {
        const vcl::KeyCode& rKey = rNEvt.GetKeyEvent()->GetKeyCode();
        sal_uInt16 nCode = rKey.GetCode();
        if (m_bIsCalc && vcl::officestudio::IsLightTheme(GetSettings().GetStyleSettings())
            && !ThemeColors::IsThemeEnabled())
        {
            if (rKey.IsMod2() && nCode == KEY_Q)
            {
                OpenHeaderSearch();
                return true;
            }
            if (nCode == KEY_ESCAPE && IsBackstageOpen())
            {
                CloseBackstage();
                return true;
            }
        }
        if ( rKey.IsMod1() && ( nCode == KEY_RIGHT || nCode == KEY_LEFT ) )
        {
            ArrowStops( nCode );
            return true;
        }
    }
    return NotebookbarTabControlBase::EventNotify( rNEvt );
}

void NotebookbarTabControl::Resize()
{
    NotebookbarTabControlBase::Resize();
    if (IsBackstageOpen())
        LayoutBackstage();
}

void NotebookbarTabControl::StateChanged(StateChangedType nStateChange)
{
    SfxViewFrame* pViewFrm = SfxViewFrame::Current();
    if (!m_bInitialized && pViewFrm)
    {
        VclPtr<ShortcutsToolBox> pShortcuts = VclPtr<ShortcutsToolBox>::Create( this );
        pShortcuts->Show();

        SetToolBox(pShortcuts.get());
        SetIconClickHdl( LINK( this, NotebookbarTabControl, OpenNotebookbarPopupMenu ) );

        m_pListener = new ChangedUIEventListener( this );

        m_bInitialized = true;
    }
    if (m_bInitialized && m_bInvalidate && pViewFrm)
    {
        ToolBox* pToolBox = GetToolBox();
        if( !pToolBox )
            return;

        pToolBox->Clear();

        const Reference<XComponentContext>& xContext = comphelper::getProcessComponentContext();
        const Reference<XModuleManager> xModuleManager  = ModuleManager::create( xContext );
        m_xFrame = pViewFrm->GetFrame().GetFrameInterface();
        OUString aModuleName = xModuleManager->identify( m_xFrame );
        m_bIsCalc = (aModuleName == u"com.sun.star.sheet.SpreadsheetDocument"_ustr);

        FillShortcutsToolBox( xContext, m_xFrame, aModuleName, pToolBox );

        if (m_bIsCalc && vcl::officestudio::IsLightTheme(GetSettings().GetStyleSettings())
            && !ThemeColors::IsThemeEnabled())
        {
            pToolBox->SetControlBackground(vcl::officestudio::CalcTopHeaderBg);
            SetFileTabClickHdl(LINK(this, NotebookbarTabControl, OnFileTabClick));
            SetSearchClickHdl(LINK(this, NotebookbarTabControl, OnSearchClick));
            SetTitleClickHdl(LINK(this, NotebookbarTabControl, OnTitleClick));
            SetActivatePageHdl(LINK(this, NotebookbarTabControl, OnActivateTabPage));
            const sal_uInt16 nCurPage = GetCurPageId();
            if (nCurPage)
            {
                const OUString sCurClean = removeMnemonicFromString(GetPageText(nCurPage));
                if (!sCurClean.equalsIgnoreAsciiCase(u"File"))
                    m_nLastNonFilePageId = nCurPage;
                else if (GetPageCount() > 1)
                    m_nLastNonFilePageId = GetPageId(1);
            }
            else if (GetPageCount() > 1)
            {
                m_nLastNonFilePageId = GetPageId(1);
            }
            if (SfxApplication* pApp = SfxGetpApp())
            {
                if (!IsListening(*pApp))
                    StartListening(*pApp);
            }
            if (!m_pHostBorderWin)
            {
                m_pHostBorderWin = GetHostBorderWindow();
                if (m_pHostBorderWin)
                    m_pHostBorderWin->AddEventListener(LINK(this, NotebookbarTabControl, BorderWinEventHdl));
            }
            UpdateDocumentTitle();
        }

        Size aSize( pToolBox->GetOptimalSize() );
        Point aPos( ICON_SIZE + 10, 0 );
        pToolBox->SetPosSizePixel( aPos, aSize );
        ImplPlaceTabs( GetSizePixel().getWidth() );

        m_bInvalidate = false;
    }
    NotebookbarTabControlBase::StateChanged( nStateChange );
}

vcl::Window* NotebookbarTabControl::GetHostBorderWindow() const
{
    for (vcl::Window* pWin = GetParent(); pWin != nullptr; pWin = pWin->GetParent())
    {
        if (dynamic_cast<const NotebookBar*>(pWin) != nullptr)
            return pWin->GetParent();
    }
    return nullptr;
}

void NotebookbarTabControl::UpdateDocumentTitle()
{
    if (!m_bIsCalc)
        return;

    SfxObjectShell* pObjSh = nullptr;
    for (SfxViewFrame* pVF = SfxViewFrame::GetFirst(); pVF != nullptr; pVF = SfxViewFrame::GetNext(*pVF))
    {
        if (pVF->GetFrame().GetFrameInterface() == m_xFrame)
        {
            pObjSh = pVF->GetObjectShell();
            break;
        }
    }
    if (!pObjSh)
    {
        if (SfxViewFrame* pCur = SfxViewFrame::Current())
            pObjSh = pCur->GetObjectShell();
    }
    if (!pObjSh)
        return;

    if (!IsListening(*pObjSh))
        StartListening(*pObjSh);

    OUString sTitle = pObjSh->GetTitle(SFX_TITLE_DETECT);
    if (sTitle.isEmpty())
        sTitle = u"Untitled 1"_ustr;
    SetDocumentTitle(sTitle, pObjSh->IsModified());
}

void NotebookbarTabControl::LayoutBackstage()
{
    if (!m_pBackstage || m_pBackstage->isDisposed())
        return;
    vcl::Window* pHost = m_pHostBorderWin ? m_pHostBorderWin.get() : GetHostBorderWindow();
    if (!pHost)
        return;

    const tools::Long nStripBottom = GetTabStripBottom();
    Point aTopPt = pHost->ScreenToOutputPixel(OutputToScreenPixel(Point(0, nStripBottom)));
    const tools::Long nTopY = std::max<tools::Long>(0, aTopPt.Y());
    const Size aHostSz = pHost->GetOutputSizePixel();
    m_pBackstage->SetPosSizePixel(
        Point(0, nTopY),
        Size(aHostSz.Width(), std::max<tools::Long>(0, aHostSz.Height() - nTopY)));
}

void NotebookbarTabControl::OpenBackstage()
{
    if (!m_bIsCalc || IsBackstageOpen() || !IsReallyVisible() || Application::IsHeadlessModeEnabled())
        return;

    vcl::Window* pHost = m_pHostBorderWin ? m_pHostBorderWin.get() : GetHostBorderWindow();
    if (!pHost)
        return;

    for (vcl::Window* pCh = pHost->GetWindow(GetWindowType::FirstChild); pCh != nullptr;
         pCh = pCh->GetWindow(GetWindowType::Next))
    {
        pCh->EnableClipSiblings();
    }

    if (!m_pBackstage || m_pBackstage->isDisposed())
    {
        m_pBackstage = VclPtr<CalcBackstageView>::Create(
            pHost, m_xFrame, LINK(this, NotebookbarTabControl, OnBackstageClose));
    }
    else
    {
        m_pBackstage->RefreshData();
    }

    if (TabPage* pCurPage = GetTabPage(GetCurPageId()))
        pCurPage->Hide();

    LayoutBackstage();
    m_pBackstage->SetZOrder(nullptr, ZOrderFlags::First);
    m_pBackstage->Show();
    m_pBackstage->GrabFocus();
    SetBackstageOpen(true);
}

void NotebookbarTabControl::CloseBackstage()
{
    if (!IsBackstageOpen())
        return;

    vcl::Window* pBackstageParent = nullptr;
    if (m_pBackstage && !m_pBackstage->isDisposed())
    {
        pBackstageParent = m_pBackstage->GetParent();
        m_pBackstage->Hide();
    }

    SetBackstageOpen(false);
    if (TabPage* pCurPage = GetTabPage(GetCurPageId()))
        pCurPage->Show();

    Resize();
    Invalidate();
    if (pBackstageParent && !pBackstageParent->isDisposed())
        pBackstageParent->Invalidate(InvalidateFlags::Children);
}

void NotebookbarTabControl::OpenHeaderSearch()
{
    if (!m_bIsCalc || !m_xFrame.is())
        return;

    if (IsBackstageOpen())
        CloseBackstage();

    tools::Rectangle aAnchor = GetSearchBoxRect();
    if (aAnchor.IsEmpty())
    {
        const tools::Long nW = std::min<tools::Long>(360, std::max<tools::Long>(200, GetOutputSizePixel().Width() / 2));
        aAnchor = tools::Rectangle(Point((GetOutputSizePixel().Width() - nW) / 2, 4), Size(nW, 26));
    }

    weld::Window* pParent = weld::GetPopupParent(*this, aAnchor);
    if (!pParent)
        return;

    if (!m_pCommandPopup)
        m_pCommandPopup = std::make_unique<CommandPopupHandler>();
    m_pCommandPopup->showPopupAtRect(pParent, m_xFrame, aAnchor, true);
}

void NotebookbarTabControl::Notify(SfxBroadcaster&, const SfxHint& rHint)
{
    if (!m_bIsCalc)
        return;

    const SfxHintId nId = rHint.GetId();
    if (nId == SfxHintId::TitleChanged || nId == SfxHintId::DocChanged)
    {
        UpdateDocumentTitle();
    }
    else if (nId == SfxHintId::ThisIsAnSfxEventHint)
    {
        const SfxEventHintId nEv = static_cast<const SfxEventHint&>(rHint).GetEventId();
        if (nEv == SfxEventHintId::ModifyChanged || nEv == SfxEventHintId::SaveDocDone
            || nEv == SfxEventHintId::SaveAsDocDone || nEv == SfxEventHintId::ActivateDoc
            || nEv == SfxEventHintId::LoadFinished || nEv == SfxEventHintId::DocCreated)
        {
            UpdateDocumentTitle();
        }
    }
}

IMPL_LINK_NOARG(NotebookbarTabControl, OnFileTabClick, NotebookbarTabControlBase*, void)
{
    if (IsBackstageOpen())
        CloseBackstage();
    else
        OpenBackstage();
}

IMPL_LINK_NOARG(NotebookbarTabControl, OnSearchClick, NotebookbarTabControlBase*, void)
{
    OpenHeaderSearch();
}

IMPL_LINK_NOARG(NotebookbarTabControl, OnTitleClick, NotebookbarTabControlBase*, void)
{
    if (!m_bIsCalc)
        return;

    SfxObjectShell* pObjSh = SfxObjectShell::Current();
    if (pObjSh && !pObjSh->HasName())
        comphelper::dispatchCommand(u".uno:SaveAs"_ustr, {});
    else
        comphelper::dispatchCommand(u".uno:SetDocumentProperties"_ustr, {});
}

IMPL_LINK_NOARG(NotebookbarTabControl, OnBackstageClose, CalcBackstageView*, void)
{
    CloseBackstage();
}

IMPL_LINK_NOARG(NotebookbarTabControl, OnActivateTabPage, TabControl*, void)
{
    if (!m_bInitialized || !IsReallyVisible() || !m_bIsCalc
        || !vcl::officestudio::IsLightTheme(GetSettings().GetStyleSettings())
        || ThemeColors::IsThemeEnabled())
        return;

    const sal_uInt16 nCurId = GetCurPageId();
    const OUString sClean = nCurId ? removeMnemonicFromString(GetPageText(nCurId)) : OUString();
    if (sClean.equalsIgnoreAsciiCase(u"File"))
    {
        sal_uInt16 nFallback = m_nLastNonFilePageId;
        if (!nFallback && GetPageCount() > 1)
            nFallback = GetPageId(1);
        if (nFallback && nFallback != nCurId)
            SetCurPageId(nFallback);
        OpenBackstage();
        return;
    }

    m_nLastNonFilePageId = nCurId;
    if (IsBackstageOpen())
        CloseBackstage();
}

IMPL_LINK(NotebookbarTabControl, BorderWinEventHdl, VclWindowEvent&, rEvent, void)
{
    if (rEvent.GetId() == VclEventId::WindowResize && IsBackstageOpen())
        LayoutBackstage();
}

void NotebookbarTabControl::FillShortcutsToolBox(Reference<XComponentContext> const & xContext,
                                          const Reference<XFrame>& xFrame,
                                          const OUString& aModuleName,
                                          ToolBox* pShortcuts
)
{
    Reference<css::container::XIndexAccess> xIndex;

    try
    {
        Reference<XUIConfigurationManager> m_xConfigManager;
        Reference<XModuleUIConfigurationManagerSupplier > xModuleCfgMgrSupplier(
            theModuleUIConfigurationManagerSupplier::get( xContext ) );
        m_xConfigManager.set( xModuleCfgMgrSupplier->getUIConfigurationManager( aModuleName ) );
        xIndex = m_xConfigManager->getSettings( TOOLBAR_STR, false );
    }
    catch( const Exception& ) {}

    if ( !xIndex.is() )
        return;

    Sequence< css::beans::PropertyValue > aPropSequence;
    for ( sal_Int32 i = 0; i < xIndex->getCount(); ++i )
    {
        try
        {
            if ( xIndex->getByIndex( i ) >>= aPropSequence )
            {
                OUString aCommandURL;
                sal_uInt16 nType = ItemType::DEFAULT;
                bool bVisible = true;

                for (const auto& aProp : aPropSequence)
                {
                    if ( aProp.Name == "CommandURL" )
                        aProp.Value >>= aCommandURL;
                    else if ( aProp.Name == "Type" )
                        aProp.Value >>= nType;
                    else if ( aProp.Name == "IsVisible" )
                        aProp.Value >>= bVisible;
                }
                if ( bVisible && ( nType == ItemType::DEFAULT ) )
                    pShortcuts->InsertItem( aCommandURL, xFrame, ToolBoxItemBits::ICON_ONLY, Size( ICON_SIZE, ICON_SIZE ) );
            }
        }
        catch ( const Exception& )
        {
            break;
        }
    }
}

IMPL_LINK(NotebookbarTabControl, OpenNotebookbarPopupMenu, NotebookBar*, pNotebookbar, void)
{
    if (!pNotebookbar || !m_xFrame.is())
        return;

    Sequence<Any> aArgs {
        Any(comphelper::makePropertyValue(u"Value"_ustr, u"notebookbar"_ustr)),
        Any(comphelper::makePropertyValue(u"Frame"_ustr, m_xFrame)) };

    const Reference<XComponentContext>& xContext = comphelper::getProcessComponentContext();
    Reference<XPopupMenuController> xPopupController(
        xContext->getServiceManager()->createInstanceWithArgumentsAndContext(
        u"com.sun.star.comp.framework.ResourceMenuController"_ustr, aArgs, xContext), UNO_QUERY);

    rtl::Reference<VCLXPopupMenu> xPopupMenu = new VCLXPopupMenu();

    if (!xPopupController.is() || !xPopupMenu.is())
        return;

    xPopupController->setPopupMenu(xPopupMenu);
    Control* pOpenMenuButton = GetOpenMenu();
    assert(pOpenMenuButton);
    Point aPos(pOpenMenuButton->GetSizePixel().getWidth(), pOpenMenuButton->GetSizePixel().getHeight());
    xPopupMenu->execute(pOpenMenuButton->GetComponentInterface(),
                        css::awt::Rectangle(aPos.X(), aPos.Y(), 1, 1),
                        css::awt::PopupMenuDirection::EXECUTE_DOWN);

    Reference<css::lang::XComponent> xComponent(xPopupController, UNO_QUERY);
    if (xComponent.is())
        xComponent->dispose();
}

Size NotebookbarTabControl::calculateRequisition() const
{
    Size aSize = NotebookbarTabControlBase::calculateRequisition();

    for (int i = 0; i < GetPageCount(); i++)
    {
        vcl::Window* pChild = GetTabPage(TabControl::GetPageId(i));

        if (pChild)
        {
            Size aChildSize = VclContainer::getLayoutRequisition(*pChild);

            if (aChildSize.getWidth() < aSize.getWidth())
                aSize.setWidth( aChildSize.Width() );
        }
    }

    if (aSize.Width() < 400)
        aSize.setWidth( 400 );

    return aSize;
}

VCL_BUILDER_FACTORY( NotebookbarTabControl )

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
