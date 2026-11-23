/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <commandpopup/CommandPopup.hxx>

#include <sfx2/msgpool.hxx>
#include <sfx2/bindings.hxx>
#include <sfx2/msg.hxx>
#include <sfx2/viewfrm.hxx>

#include <comphelper/processfactory.hxx>
#include <comphelper/dispatchcommand.hxx>
#include <comphelper/propertyvalue.hxx>

#include <com/sun/star/frame/XDispatchProvider.hpp>
#include <com/sun/star/frame/XDispatch.hpp>
#include <com/sun/star/ui/theModuleUIConfigurationManagerSupplier.hpp>
#include <com/sun/star/util/SearchAlgorithms.hpp>
#include <com/sun/star/util/SearchAlgorithms2.hpp>
#include <com/sun/star/util/URL.hpp>
#include <com/sun/star/util/URLTransformer.hpp>
#include <com/sun/star/i18n/CharacterClassification.hpp>

#include <i18nutil/transliteration.hxx>
#include <svl/srchitem.hxx>
#include <vcl/commandinfoprovider.hxx>
#include <vcl/event.hxx>
#include <vcl/svapp.hxx>
#include <i18nlangtag/languagetag.hxx>

using namespace css;

MenuContentHandler::MenuContentHandler(uno::Reference<frame::XFrame> const& xFrame)
    : m_xContext(comphelper::getProcessComponentContext())
    , m_xFrame(xFrame)
    , m_xCharacterClassification(i18n::CharacterClassification::create(m_xContext))
    , m_xURLTransformer(util::URLTransformer::create(m_xContext))
    , m_sModuleLongName(vcl::CommandInfoProvider::GetModuleIdentifier(xFrame))
{
    uno::Reference<ui::XModuleUIConfigurationManagerSupplier> xModuleConfigSupplier;
    xModuleConfigSupplier.set(ui::theModuleUIConfigurationManagerSupplier::get(m_xContext));

    uno::Reference<ui::XUIConfigurationManager> xConfigurationManager;
    xConfigurationManager = xModuleConfigSupplier->getUIConfigurationManager(m_sModuleLongName);

    uno::Reference<container::XIndexAccess> xConfigData;
    xConfigData
        = xConfigurationManager->getSettings(u"private:resource/menubar/menubar"_ustr, false);

    gatherMenuContent(xConfigData, m_aMenuContent);
}

void MenuContentHandler::gatherMenuContent(
    uno::Reference<container::XIndexAccess> const& xIndexAccess, MenuContent& rMenuContent)
{
    std::u16string_view aMenuLabelSeparator = AllSettings::GetLayoutRTL() ? u" ◂ " : u" ▸ ";
    for (sal_Int32 n = 0; n < xIndexAccess->getCount(); n++)
    {
        MenuContent aNewContent;
        uno::Sequence<beans::PropertyValue> aProperties;
        uno::Reference<container::XIndexAccess> xIndexContainer;

        if (!(xIndexAccess->getByIndex(n) >>= aProperties))
            continue;

        bool bIsVisible = true;
        bool bIsEnabled = true;

        for (auto const& rProperty : aProperties)
        {
            OUString aPropertyName = rProperty.Name;
            if (aPropertyName == "CommandURL")
                rProperty.Value >>= aNewContent.m_aCommandURL;
            else if (aPropertyName == "ItemDescriptorContainer")
                rProperty.Value >>= xIndexContainer;
            else if (aPropertyName == "IsVisible")
                rProperty.Value >>= bIsVisible;
            else if (aPropertyName == "Enabled")
                rProperty.Value >>= bIsEnabled;
        }

        if (!bIsEnabled || !bIsVisible)
            continue;

        auto aCommandProperties = vcl::CommandInfoProvider::GetCommandProperties(
            aNewContent.m_aCommandURL, m_sModuleLongName);
        aNewContent.m_aMenuLabel = vcl::CommandInfoProvider::GetLabelForCommand(aCommandProperties);

        if (!rMenuContent.m_aFullLabelWithPath.isEmpty())
            aNewContent.m_aFullLabelWithPath += aMenuLabelSeparator;
        aNewContent.m_aFullLabelWithPath += aNewContent.m_aMenuLabel;
        aNewContent.m_aSearchableMenuLabel = toLower(aNewContent.m_aFullLabelWithPath);

        aNewContent.m_aTooltip = vcl::CommandInfoProvider::GetTooltipForCommand(
            aNewContent.m_aCommandURL, aCommandProperties, m_xFrame);

        if (xIndexContainer.is())
            gatherMenuContent(xIndexContainer, aNewContent);

        rMenuContent.m_aSubMenuContent.push_back(std::move(aNewContent));
    }
}

void MenuContentHandler::findInMenu(OUString const& rText,
                                    std::unique_ptr<weld::TreeView>& rpCommandTreeView,
                                    std::vector<CurrentEntry>& rCommandList)
{
    m_aAdded.clear();

    OUString aLowerCaseText = toLower(rText);

    // find submenus and menu items that start with the searched text
    auto aTextStartCriterium = [](MenuContent const& rMenuContent, OUString const& rSearchText) {
        OUString aSearchText = " / " + rSearchText;
        return rMenuContent.m_aSearchableMenuLabel.indexOf(aSearchText) > 0;
    };

    findInMenuRecursive(m_aMenuContent, aLowerCaseText, rpCommandTreeView, rCommandList,
                        aTextStartCriterium);

    // find submenus and menu items that contain the searched text
    auto aTextAllCriterium = [](MenuContent const& rMenuContent, OUString const& rSearchText) {
        return rMenuContent.m_aSearchableMenuLabel.indexOf(rSearchText) > 0;
    };

    findInMenuRecursive(m_aMenuContent, aLowerCaseText, rpCommandTreeView, rCommandList,
                        aTextAllCriterium);
}

void MenuContentHandler::findInMenuRecursive(
    MenuContent const& rMenuContent, OUString const& rText,
    std::unique_ptr<weld::TreeView>& rpCommandTreeView, std::vector<CurrentEntry>& rCommandList,
    std::function<bool(MenuContent const&, OUString const&)> const& rSearchCriterium)
{
    for (MenuContent const& aSubContent : rMenuContent.m_aSubMenuContent)
    {
        if (rSearchCriterium(aSubContent, rText))
        {
            addCommandIfPossible(aSubContent, rpCommandTreeView, rCommandList);
        }
        findInMenuRecursive(aSubContent, rText, rpCommandTreeView, rCommandList, rSearchCriterium);
    }
}

void MenuContentHandler::addCommandIfPossible(
    MenuContent const& rMenuContent, const std::unique_ptr<weld::TreeView>& rpCommandTreeView,
    std::vector<CurrentEntry>& rCommandList)
{
    if (m_aAdded.find(rMenuContent.m_aFullLabelWithPath) != m_aAdded.end())
        return;

    OUString sCommandURL = rMenuContent.m_aCommandURL;
    util::URL aCommandURL;
    aCommandURL.Complete = sCommandURL;

    if (!m_xURLTransformer->parseStrict(aCommandURL))
        return;

    auto* pViewFrame = SfxViewFrame::Current();
    if (!pViewFrame)
        return;

    SfxSlotPool& rSlotPool = SfxSlotPool::GetSlotPool(pViewFrame);
    const SfxSlot* pSlot = rSlotPool.GetUnoSlot(aCommandURL.Path);
    if (!pSlot)
        return;

    std::unique_ptr<SfxPoolItem> pState;
    SfxItemState eState = pViewFrame->GetBindings().QueryState(pSlot->GetSlotId(), pState);
    if (eState == SfxItemState::DISABLED)
        return;

    auto xGraphic = vcl::CommandInfoProvider::GetXGraphicForCommand(sCommandURL, m_xFrame);
    rCommandList.emplace_back(sCommandURL, rMenuContent.m_aTooltip);

    auto pIter = rpCommandTreeView->make_iterator();
    rpCommandTreeView->insert(nullptr, -1, &rMenuContent.m_aFullLabelWithPath, nullptr, nullptr,
                              nullptr, false, pIter.get());
    rpCommandTreeView->set_image(*pIter, xGraphic);
    m_aAdded.insert(rMenuContent.m_aFullLabelWithPath);
}

OUString MenuContentHandler::toLower(OUString const& rString)
{
    const css::lang::Locale& rLocale = Application::GetSettings().GetUILanguageTag().getLocale();

    return m_xCharacterClassification->toLower(rString, 0, rString.getLength(), rLocale);
}

CommandListBox::CommandListBox(weld::Window* pParent, uno::Reference<frame::XFrame> const& xFrame,
                               const tools::Rectangle* pAnchorRect, bool bIncludeSheetFind)
    : mxBuilder(Application::CreateBuilder(pParent, u"sfx/ui/commandpopup.ui"_ustr))
    , mxPopover(mxBuilder->weld_popover(u"CommandPopup"_ustr))
    , mpEntry(mxBuilder->weld_entry(u"command_entry"_ustr))
    , mpCommandTreeView(mxBuilder->weld_tree_view(u"command_treeview"_ustr))
    , mxFrame(xFrame)
    , mpMenuContentHandler(std::make_unique<MenuContentHandler>(xFrame))
    , mbIncludeSheetFind(bIncludeSheetFind)
{
    mpEntry->connect_changed(LINK(this, CommandListBox, ModifyHdl));
    mpEntry->connect_key_press(LINK(this, CommandListBox, TreeViewKeyPress));
    mpCommandTreeView->connect_query_tooltip(LINK(this, CommandListBox, QueryTooltip));
    mpCommandTreeView->connect_item_activated(LINK(this, CommandListBox, RowActivated));

    if (mbIncludeSheetFind)
        mpEntry->set_placeholder_text(u"Type a command or text to find in sheet..."_ustr);

    Size aFrameSize = pParent->get_size();
    tools::Long nWidth = pAnchorRect
                             ? std::max<tools::Long>(pAnchorRect->GetWidth(), 400)
                             : std::max(tools::Long(400), aFrameSize.Width() / 3);
    tools::Long nHeight = pAnchorRect ? 250 : 400;
    mpCommandTreeView->set_size_request(nWidth, nHeight);

    if (mbIncludeSheetFind)
        populateDefaultActions();

    tools::Rectangle aRect;
    if (pAnchorRect)
    {
        const tools::Long nCenteredLeft = std::max<tools::Long>(
            8, pAnchorRect->Left() - (nWidth - pAnchorRect->GetWidth()) / 2);
        aRect = tools::Rectangle(Point(nCenteredLeft, pAnchorRect->Top()),
                                 Size(nWidth, pAnchorRect->GetHeight() + 4));
    }
    else
    {
        aRect = tools::Rectangle(Point(aFrameSize.Width() / 2, 0), Size(0, 0));
    }
    mxPopover->popup_at_rect(pParent, aRect, weld::Placement::Under);
    mpEntry->grab_focus();
}

void CommandListBox::populateDefaultActions()
{
    struct QuickAction
    {
        const char16_t* pLabel;
        const char16_t* pCmd;
    };
    static constexpr QuickAction aActions[] = {
        { u"Find and Replace... (Ctrl+H)", u".uno:SearchDialog" },
        { u"Sort...", u".uno:DataSort" },
        { u"Insert Chart...", u".uno:InsertObjectChart" },
        { u"Freeze Rows and Columns", u".uno:FreezePanes" },
        { u"Export as PDF...", u".uno:ExportToPDF" },
        { u"Spreadsheet Options...", u".uno:OptionsTreeDialog" },
    };

    const OUString sModule = vcl::CommandInfoProvider::GetModuleIdentifier(mxFrame);
    mpCommandTreeView->freeze();
    for (const auto& rAct : aActions)
    {
        const OUString sCmd(rAct.pCmd);
        const OUString sLabel(rAct.pLabel);
        auto aProps = vcl::CommandInfoProvider::GetCommandProperties(sCmd, sModule);
        const OUString sTooltip = vcl::CommandInfoProvider::GetTooltipForCommand(sCmd, aProps, mxFrame);
        auto xGraphic = vcl::CommandInfoProvider::GetXGraphicForCommand(sCmd, mxFrame);
        maCommandList.emplace_back(sCmd, sTooltip);
        auto pIter = mpCommandTreeView->make_iterator();
        mpCommandTreeView->insert(nullptr, -1, &sLabel, nullptr, nullptr, nullptr, false, pIter.get());
        mpCommandTreeView->set_image(*pIter, xGraphic);
    }
    mpCommandTreeView->thaw();

    if (mpCommandTreeView->n_children() > 0)
    {
        mpCommandTreeView->set_cursor(0);
        mpCommandTreeView->select(0);
    }
}

IMPL_LINK_NOARG(CommandListBox, QueryTooltip, const weld::TreeIter&, OUString)
{
    size_t nSelected = mpCommandTreeView->get_selected_index();
    if (nSelected < maCommandList.size())
    {
        auto const& rCurrent = maCommandList[nSelected];
        return rCurrent.m_aTooltip;
    }
    return OUString();
}

void CommandListBox::DispatchSelectedCommand()
{
    OUString aCommandURL;
    int nSelected = mpCommandTreeView->get_selected_index();
    if (nSelected != -1 && nSelected < int(maCommandList.size()))
    {
        auto const& rCurrent = maCommandList[nSelected];
        aCommandURL = rCurrent.m_aCommandURL;
    }
    dispatchCommandAndClose(aCommandURL);
}

IMPL_LINK_NOARG(CommandListBox, RowActivated, const weld::TreeIter&, bool)
{
    DispatchSelectedCommand();
    return true;
}

IMPL_LINK(CommandListBox, TreeViewKeyPress, const KeyEvent&, rKeyEvent, bool)
{
    if (rKeyEvent.GetKeyCode().GetCode() == KEY_DOWN || rKeyEvent.GetKeyCode().GetCode() == KEY_UP)
    {
        int nDirection = rKeyEvent.GetKeyCode().GetCode() == KEY_DOWN ? 1 : -1;
        int nNewIndex = mpCommandTreeView->get_selected_index() + nDirection;
        nNewIndex = std::clamp(nNewIndex, 0, mpCommandTreeView->n_children() - 1);
        mpCommandTreeView->select(nNewIndex);
        mpCommandTreeView->set_cursor(nNewIndex);
        return true;
    }
    else if (rKeyEvent.GetKeyCode().GetCode() == KEY_RETURN)
    {
        DispatchSelectedCommand();
        return true;
    }

    return false;
}

IMPL_LINK_NOARG(CommandListBox, ModifyHdl, weld::Entry&, void)
{
    mpCommandTreeView->clear();
    maCommandList.clear();

    OUString sText = mpEntry->get_text();
    if (sText.isEmpty())
    {
        if (mbIncludeSheetFind)
            populateDefaultActions();
        return;
    }

    mpCommandTreeView->freeze();
    if (mbIncludeSheetFind)
    {
        const OUString sFindLabel = u"Find \"" + sText + u"\" in Sheet";
        const OUString sFindCmd = u"calc-find:" + sText;
        auto xGraphic = vcl::CommandInfoProvider::GetXGraphicForCommand(u".uno:SearchDialog"_ustr, mxFrame);
        maCommandList.emplace_back(sFindCmd, u"Search for matching cell values in the active spreadsheet"_ustr);
        auto pIter = mpCommandTreeView->make_iterator();
        mpCommandTreeView->insert(nullptr, -1, &sFindLabel, nullptr, nullptr, nullptr, false, pIter.get());
        mpCommandTreeView->set_image(*pIter, xGraphic);
    }
    mpMenuContentHandler->findInMenu(sText, mpCommandTreeView, maCommandList);
    mpCommandTreeView->thaw();

    if (mpCommandTreeView->n_children() > 0)
    {
        mpCommandTreeView->set_cursor(0);
        mpCommandTreeView->select(0);
    }

    mpEntry->grab_focus();
}

void CommandListBox::dispatchCommandAndClose(OUString const& rCommand)
{
    // Copy members before popdown() since PopupModeEnd resets mpListBox
    const OUString sCommand = rCommand;
    uno::Reference<frame::XFrame> xFrame = mxFrame;
    mxPopover->popdown();

    if (sCommand.isEmpty())
        return;

    if (sCommand.startsWith(u"calc-find:"))
    {
        const OUString sQuery = sCommand.copy(10);
        if (!sQuery.isEmpty() && xFrame.is())
        {
            try
            {
                auto xTrans = util::URLTransformer::create(comphelper::getProcessComponentContext());
                util::URL aURL;
                aURL.Complete = u".uno:ExecuteSearch"_ustr;
                xTrans->parseStrict(aURL);

                uno::Sequence<beans::PropertyValue> aArgs{
                    comphelper::makePropertyValue(u"SearchItem.SearchString"_ustr, sQuery),
                    comphelper::makePropertyValue(u"SearchItem.CellType"_ustr, sal_Int16(SvxSearchCellType::VALUE)),
                    comphelper::makePropertyValue(u"SearchItem.Backward"_ustr, false),
                    comphelper::makePropertyValue(u"SearchItem.SearchFlags"_ustr, sal_Int32(0)),
                    comphelper::makePropertyValue(u"SearchItem.TransliterateFlags"_ustr,
                                                  static_cast<sal_Int32>(TransliterationFlags::IGNORE_CASE)),
                    comphelper::makePropertyValue(u"SearchItem.Command"_ustr,
                                                  static_cast<sal_Int16>(SvxSearchCmd::FIND)),
                    comphelper::makePropertyValue(u"SearchItem.AlgorithmType"_ustr,
                                                  sal_Int16(util::SearchAlgorithms_ABSOLUTE)),
                    comphelper::makePropertyValue(u"SearchItem.AlgorithmType2"_ustr,
                                                  sal_Int16(util::SearchAlgorithms2::ABSOLUTE)),
                    comphelper::makePropertyValue(u"SearchItem.SearchFormatted"_ustr, false),
                    comphelper::makePropertyValue(u"UseAttrItemList"_ustr, false),
                };

                uno::Reference<frame::XDispatchProvider> xProvider(xFrame, uno::UNO_QUERY);
                if (xProvider.is())
                {
                    uno::Reference<frame::XDispatch> xDispatch = xProvider->queryDispatch(aURL, OUString(), 0);
                    if (xDispatch.is())
                        xDispatch->dispatch(aURL, aArgs);
                }
            }
            catch (const uno::Exception&)
            {
            }
        }
        return;
    }

    comphelper::dispatchCommand(sCommand, uno::Sequence<beans::PropertyValue>());
}

void CommandPopupHandler::showPopup(weld::Window* pParent,
                                    css::uno::Reference<css::frame::XFrame> const& xFrame)
{
    auto pCommandListBox = std::make_unique<CommandListBox>(pParent, xFrame);
    pCommandListBox->connect_closed(LINK(this, CommandPopupHandler, PopupModeEnd));
    mpListBox = std::move(pCommandListBox);
}

void CommandPopupHandler::showPopupAtRect(weld::Window* pParent,
                                          css::uno::Reference<css::frame::XFrame> const& xFrame,
                                          const tools::Rectangle& rAnchorRect,
                                          bool bIncludeSheetFind)
{
    auto pCommandListBox = std::make_unique<CommandListBox>(pParent, xFrame, &rAnchorRect, bIncludeSheetFind);
    pCommandListBox->connect_closed(LINK(this, CommandPopupHandler, PopupModeEnd));
    mpListBox = std::move(pCommandListBox);
}

IMPL_LINK_NOARG(CommandPopupHandler, PopupModeEnd, weld::Popover&, void) { mpListBox.reset(); }

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
