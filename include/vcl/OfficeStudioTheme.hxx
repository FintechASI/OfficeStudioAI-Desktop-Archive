/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <string_view>

#include <tools/color.hxx>
#include <tools/gen.hxx>
#include <vcl/dllapi.h>
#include <vcl/settings.hxx>

class Image;
class ToolBox;
namespace vcl { class Window; }

namespace vcl::officestudio
{

// Approved 10-color OfficeStudio AI colorful icon & accent palette
inline constexpr Color RoyalBlue       { 0x25, 0x63, 0xEB };
inline constexpr Color RoyalBlueTile   { 0xDB, 0xEA, 0xFE };
inline constexpr Color RoyalBlueDark   { 0x1D, 0x4E, 0xD8 };

inline constexpr Color Orange          { 0xEA, 0x58, 0x0C };
inline constexpr Color OrangeTile      { 0xFF, 0xED, 0xD5 };
inline constexpr Color OrangeDark      { 0xC2, 0x41, 0x0C };

inline constexpr Color Purple          { 0x7C, 0x3A, 0xED };
inline constexpr Color PurpleTile      { 0xED, 0xE9, 0xFE };
inline constexpr Color PurpleDark      { 0x6D, 0x28, 0xD9 };

inline constexpr Color Emerald         { 0x16, 0xA3, 0x4A };
inline constexpr Color EmeraldTile     { 0xDC, 0xFC, 0xE7 };
inline constexpr Color EmeraldDark     { 0x15, 0x80, 0x3D };

inline constexpr Color Red             { 0xDC, 0x26, 0x26 };
inline constexpr Color RedTile         { 0xFE, 0xE2, 0xE2 };
inline constexpr Color RedDark         { 0xB9, 0x1C, 0x1C };

inline constexpr Color Teal            { 0x0D, 0x94, 0x88 };
inline constexpr Color TealTile        { 0xCC, 0xFB, 0xF1 };
inline constexpr Color TealDark        { 0x0F, 0x76, 0x6E };

inline constexpr Color SkyBlue         { 0x02, 0x84, 0xC7 };
inline constexpr Color SkyBlueTile     { 0xE0, 0xF2, 0xFE };
inline constexpr Color SkyBlueDark     { 0x03, 0x69, 0xA1 };

inline constexpr Color Amber           { 0xD9, 0x77, 0x06 };
inline constexpr Color AmberTile       { 0xFE, 0xF3, 0xC7 };
inline constexpr Color AmberDark       { 0xB4, 0x53, 0x09 };

inline constexpr Color Rose            { 0xE1, 0x1D, 0x48 };
inline constexpr Color RoseTile        { 0xFF, 0xE4, 0xE6 };
inline constexpr Color RoseDark        { 0xBE, 0x12, 0x3C };

inline constexpr Color Indigo          { 0x4F, 0x46, 0xE5 };
inline constexpr Color IndigoTile      { 0xE0, 0xE7, 0xFF };
inline constexpr Color IndigoDark      { 0x43, 0x38, 0xCA };

// Neutral slate palette for icon details and Microsoft 365 Fluent Light UI
inline constexpr Color Slate900        { 0x0F, 0x17, 0x2A };
inline constexpr Color Slate700        { 0x33, 0x41, 0x55 };
inline constexpr Color Slate600        { 0x47, 0x55, 0x69 };
inline constexpr Color Slate500        { 0x64, 0x74, 0x8B };
inline constexpr Color Slate400        { 0x94, 0xA3, 0xB8 };
inline constexpr Color Slate300        { 0xCB, 0xD5, 0xE1 };
inline constexpr Color Slate200        { 0xE2, 0xE8, 0xF0 };

// Microsoft Office 2021 Light chrome tokens (neutral surfaces + crisp borders)
inline constexpr Color RibbonHeaderBg  { 0xEC, 0xEF, 0xF2 };
inline constexpr Color RibbonCardBg    { 0xFF, 0xFF, 0xFF };
inline constexpr Color RibbonCardBorder{ 0xD6, 0xD6, 0xD6 };
inline constexpr Color RibbonCardShadow{ 0xE4, 0xE4, 0xE4 };
inline constexpr Color CanvasBg        { 0xEA, 0xEC, 0xEE };
inline constexpr Color PageShadow      { 0xC8, 0xC6, 0xC4 };
inline constexpr Color SurfaceSubtle   { 0xF6, 0xF6, 0xF6 };
inline constexpr Color Divider         { 0xDF, 0xDF, 0xDF };
inline constexpr Color BorderSubtle    { 0xD1, 0xD1, 0xD1 };
inline constexpr Color HoverBg         { 0xE6, 0xE6, 0xE6 };
inline constexpr Color TabHoverBg      { 0xE1, 0xE1, 0xE1 };
inline constexpr Color CheckedBg       { 0xE0, 0xEB, 0xF8 };
inline constexpr Color CheckedBorder   { 0xB3, 0xCD, 0xEE };
inline constexpr Color PressedBg       { 0xD2, 0xD2, 0xD2 };
inline constexpr Color PressedBorder   { 0xB8, 0xB8, 0xB8 };
inline constexpr Color MenuHoverBg     { 0xED, 0xEB, 0xE9 };
inline constexpr Color TextPrimary     { 0x25, 0x24, 0x23 };
inline constexpr Color TextSecondary   { 0x48, 0x46, 0x44 };
inline constexpr Color TextMuted       { 0x60, 0x5E, 0x5C };

// OfficeStudio Docs (Word 2021-inspired) blue accent tokens
inline constexpr Color DocsAccent      { 0x18, 0x5A, 0xBD };
inline constexpr Color DocsAccentHover { 0x14, 0x4B, 0xA1 };
inline constexpr Color DocsAccentDark  { 0x10, 0x3F, 0x8A };
inline constexpr Color DocsAccentSoft  { 0xE8, 0xF0, 0xFA };
inline constexpr Color DocsHeadingBlue { 0x2F, 0x54, 0x96 };

// OfficeStudio Spreadsheet (Excel 2021-inspired) neutral chrome + #107C41 green accent
inline constexpr Color CalcAccent       { 0x10, 0x7C, 0x41 };
inline constexpr Color CalcAccentHover  { 0x0E, 0x6B, 0x37 };
inline constexpr Color CalcAccentDark   { 0x0B, 0x5A, 0x2E };
inline constexpr Color CalcAccentSoft   { 0xDF, 0xEE, 0xE5 };
inline constexpr Color CalcAccentBorder { 0xA6, 0xD4, 0xB8 };
inline constexpr Color CalcGridLine     { 0xD9, 0xD9, 0xD9 };
inline constexpr Color CalcHeaderBg     { 0xF0, 0xF0, 0xF0 };
inline constexpr Color CalcHeaderSelBg  { 0xDF, 0xEA, 0xE3 };
inline constexpr Color CalcHeaderBorder { 0xD4, 0xD4, 0xD4 };
inline constexpr Color CalcTabStripBg   { 0xF0, 0xF0, 0xF0 };

// OfficeStudio Spreadsheet refined Office 2021 chrome & File/Backstage tokens
inline constexpr Color CalcTopHeaderBg     { 0xF0, 0xF0, 0xF0 };
inline constexpr Color CalcTabHoverBg      { 0xE1, 0xE1, 0xE1 };
inline constexpr Color CalcFileNormalBg    { 0xF0, 0xF0, 0xF0 };
inline constexpr Color CalcFileNormalBorder{ 0xF0, 0xF0, 0xF0 };
inline constexpr Color CalcFileHoverBg     { 0xE1, 0xE1, 0xE1 };
inline constexpr Color CalcBtnHoverBg      { 0xE6, 0xE6, 0xE6 };
inline constexpr Color CalcBtnHoverBorder  { 0xD2, 0xD0, 0xCE };
inline constexpr Color CalcBtnPressedBg    { 0xD2, 0xD2, 0xD2 };
inline constexpr Color CalcSearchBorder    { 0xD1, 0xD1, 0xD1 };
inline constexpr Color CalcSearchHoverBdr  { 0x99, 0x99, 0x99 };
inline constexpr Color CalcBackstageRailBg { 0x10, 0x7C, 0x41 };
inline constexpr Color CalcBackstageHover  { 0x16, 0x8A, 0x4B };
inline constexpr Color CalcBackstageActive { 0x0B, 0x5A, 0x2E };

// Office 2021 icon monoline & selective color palette
inline constexpr Color O21Ink        { 0x3B, 0x3A, 0x39 };
inline constexpr Color O21InkSoft    { 0x60, 0x5E, 0x5C };
inline constexpr Color O21Steel      { 0x48, 0x69, 0x91 };
inline constexpr Color O21Blue       { 0x2B, 0x7C, 0xD3 };
inline constexpr Color O21BlueSoft   { 0xD6, 0xE7, 0xF8 };
inline constexpr Color O21Amber      { 0xC8, 0x8A, 0x3B };
inline constexpr Color O21AmberFill  { 0xF4, 0xE1, 0xC1 };
inline constexpr Color O21Gold       { 0xD9, 0x9B, 0x26 };
inline constexpr Color O21GoldLight  { 0xF7, 0xDC, 0x88 };
inline constexpr Color O21Red        { 0xC4, 0x2B, 0x1C };
inline constexpr Color O21RoseSoft   { 0xF4, 0xB8, 0xBD };
inline constexpr Color O21Magenta    { 0xB4, 0x42, 0x86 };
inline constexpr Color O21MagentaSoft{ 0xF3, 0xD6, 0xE6 };
inline constexpr Color O21Yellow     { 0xFC, 0xE1, 0x00 };
inline constexpr Color O21Green      { 0x10, 0x7C, 0x41 };
inline constexpr Color O21GreenSoft  { 0xC6, 0xEF, 0xCE };

inline bool IsLightTheme(const StyleSettings& rStyle)
{
    return !rStyle.GetHighContrastMode() && !rStyle.GetWindowColor().IsDark();
}

// Reusable OfficeStudio Ribbon icon sizing & layout standards.
// Currently enabled ONLY for Docs and Spreadsheet via IsEnhancedRibbonModule();
// SlideShows, Design, Database, and Math can opt in here later.
inline constexpr sal_Int32 RibbonSmallIconScalePct = 119; // 16px -> 19px (+18.75%)
inline constexpr sal_Int32 RibbonLargeIconScalePct = 108; // 24px -> 26px (+8.3%)

inline bool IsCalcModule(std::u16string_view sModule)
{
    return sModule == u"com.sun.star.sheet.SpreadsheetDocument";
}

inline bool IsWriterModule(std::u16string_view sModule)
{
    return sModule == u"com.sun.star.text.TextDocument";
}

inline bool IsEnhancedRibbonModule(std::u16string_view sModule)
{
    return IsWriterModule(sModule) || IsCalcModule(sModule);
}

inline sal_Int32 ComputeEffectiveDisplayScalePct(sal_Int32 nDPIScalePct, sal_Int32 nDPIY)
{
    const sal_Int32 nPctFromDPI = (nDPIY > 96)
        ? static_cast<sal_Int32>(std::lround(nDPIY * 100.0 / 96.0))
        : 100;
    return std::max(std::max<sal_Int32>(100, nDPIScalePct), nPctFromDPI);
}

inline sal_Int32 ComputeRibbonIconScalePct(sal_Int32 nBaseScalePct, sal_Int32 nDisplayScalePct)
{
    return std::max<sal_Int32>(100,
        static_cast<sal_Int32>(std::lround(nBaseScalePct * nDisplayScalePct / 100.0)));
}

inline tools::Long ComputeScaledMetric(double fBasePx, sal_Int32 nDisplayScalePct)
{
    return static_cast<tools::Long>(std::lround(fBasePx * nDisplayScalePct / 100.0));
}

VCL_DLLPUBLIC bool IsEnhancedRibbonToolBox(const ToolBox& rToolBox);
VCL_DLLPUBLIC sal_Int32 GetRibbonDisplayScalePct(const ToolBox& rToolBox);
VCL_DLLPUBLIC Image ResolveRibbonToolBoxImage(const ToolBox& rToolBox, const Image& rImage);
VCL_DLLPUBLIC bool IsEnhancedRibbonWindow(const vcl::Window& rWindow);
VCL_DLLPUBLIC bool IsCalcRibbonWindow(const vcl::Window& rWindow);
VCL_DLLPUBLIC sal_Int32 GetWindowEffectiveDisplayScalePct(const vcl::Window& rWindow);

} // namespace vcl::officestudio

