/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <OfficeStudioIcons.hxx>

#include <algorithm>
#include <cmath>
#include <vector>

#include <basegfx/polygon/b2dpolygon.hxx>
#include <basegfx/polygon/b2dpolygontools.hxx>
#include <basegfx/range/b2drectangle.hxx>
#include <o3tl/string_view.hxx>
#include <vcl/OfficeStudioTheme.hxx>
#include <vcl/font.hxx>
#include <vcl/rendercontext/AntialiasingFlags.hxx>
#include <vcl/rendercontext/DrawTextFlags.hxx>
#include <vcl/settings.hxx>
#include <vcl/svapp.hxx>
#include <vcl/virdev.hxx>

namespace vcl::officestudio
{
namespace
{

enum class PaletteId
{
    RoyalBlue,
    Orange,
    Purple,
    Emerald,
    Red,
    Teal,
    SkyBlue,
    Amber,
    Rose,
    Indigo,
    Slate
};

struct PalettePair
{
    Color fg;
    Color tileBg;
    Color dark;
};

PalettePair GetPalette(PaletteId eId)
{
    switch (eId)
    {
        case PaletteId::RoyalBlue: return { RoyalBlue, RoyalBlueTile, RoyalBlueDark };
        case PaletteId::Orange:    return { Orange, OrangeTile, OrangeDark };
        case PaletteId::Purple:    return { Purple, PurpleTile, PurpleDark };
        case PaletteId::Emerald:   return { Emerald, EmeraldTile, EmeraldDark };
        case PaletteId::Red:       return { Red, RedTile, RedDark };
        case PaletteId::Teal:      return { Teal, TealTile, TealDark };
        case PaletteId::SkyBlue:   return { SkyBlue, SkyBlueTile, SkyBlueDark };
        case PaletteId::Amber:     return { Amber, AmberTile, AmberDark };
        case PaletteId::Rose:      return { Rose, RoseTile, RoseDark };
        case PaletteId::Indigo:    return { Indigo, IndigoTile, IndigoDark };
        case PaletteId::Slate:     return { Slate700, SurfaceSubtle, Slate900 };
    }
    return { RoyalBlue, RoyalBlueTile, RoyalBlueDark };
}

class Canvas
{
public:
    VirtualDevice& mrDev;
    double mfUnit;
    double mfScale;
    double mfStroke;

    Canvas(VirtualDevice& rDev, tools::Long nSz, bool bTiled,
           double fScaleOverride = -1.0, double fStrokeOverride = -1.0)
        : mrDev(rDev)
        , mfUnit(static_cast<double>(nSz) / 24.0)
        , mfScale(fScaleOverride > 0.0 ? fScaleOverride : (bTiled ? 1.0 : 1.24))
        , mfStroke((fStrokeOverride > 0.0 ? fStrokeOverride : (bTiled ? 1.65 : 1.85)) * mfUnit)
    {
    }

    double X(double x) const { return (12.0 + (x - 12.0) * mfScale) * mfUnit; }
    double Y(double y) const { return (12.0 + (y - 12.0) * mfScale) * mfUnit; }
    double S(double v) const { return v * mfScale * mfUnit; }

    void FillTile(Color aBg, Color aBorder)
    {
        double m = 1.0 * mfUnit;
        double w = 22.0 * mfUnit;
        double rad = std::clamp((2.0 * 5.2 * mfUnit) / std::max(1.0, w), 0.0, 1.0);
        basegfx::B2DPolygon aPoly = basegfx::utils::createPolygonFromRect(
            basegfx::B2DRectangle(m, m, 24.0 * mfUnit - m, 24.0 * mfUnit - m), rad, rad);
        mrDev.SetLineColor();
        mrDev.SetFillColor(aBg);
        mrDev.DrawPolygon(aPoly);
        mrDev.SetFillColor();
        mrDev.SetLineColor(aBorder);
        mrDev.DrawPolyLine(aPoly, 1.0 * mfUnit, basegfx::B2DLineJoin::Round);
    }

    void RoundRect(double x1, double y1, double x2, double y2, double r,
                   Color aFill, bool bFill, Color aStroke, double fStrokeMult = 1.0)
    {
        double rx1 = X(x1), ry1 = Y(y1), rx2 = X(x2), ry2 = Y(y2);
        double w = std::max(0.5, rx2 - rx1);
        double h = std::max(0.5, ry2 - ry1);
        double rx = std::clamp((2.0 * S(r)) / w, 0.0, 1.0);
        double ry = std::clamp((2.0 * S(r)) / h, 0.0, 1.0);
        basegfx::B2DPolygon aPoly = basegfx::utils::createPolygonFromRect(
            basegfx::B2DRectangle(rx1, ry1, rx2, ry2), rx, ry);
        if (bFill)
        {
            mrDev.SetLineColor();
            mrDev.SetFillColor(aFill);
            mrDev.DrawPolygon(aPoly);
        }
        if (fStrokeMult > 0.0)
        {
            mrDev.SetFillColor();
            mrDev.SetLineColor(aStroke);
            mrDev.DrawPolyLine(aPoly, mfStroke * fStrokeMult, basegfx::B2DLineJoin::Round,
                               css::drawing::LineCap_ROUND);
        }
    }

    void Poly(const std::vector<std::pair<double, double>>& rPts, bool bClosed,
              Color aColor, double fStrokeMult = 1.0, bool bFill = false, Color aFillColor = COL_WHITE)
    {
        basegfx::B2DPolygon aPoly;
        for (const auto& pt : rPts)
            aPoly.append(basegfx::B2DPoint(X(pt.first), Y(pt.second)));
        aPoly.setClosed(bClosed);
        if (bFill && bClosed)
        {
            mrDev.SetLineColor();
            mrDev.SetFillColor(aFillColor);
            mrDev.DrawPolygon(aPoly);
        }
        if (fStrokeMult > 0.0)
        {
            mrDev.SetFillColor();
            mrDev.SetLineColor(aColor);
            mrDev.DrawPolyLine(aPoly, mfStroke * fStrokeMult, basegfx::B2DLineJoin::Round,
                               css::drawing::LineCap_ROUND);
        }
    }

    void Line(double x1, double y1, double x2, double y2, Color aColor, double fStrokeMult = 1.0)
    {
        Poly({ { x1, y1 }, { x2, y2 } }, false, aColor, fStrokeMult);
    }

    void Circle(double cx, double cy, double r, Color aStroke, bool bFill = false,
                Color aFillColor = COL_WHITE, double fStrokeMult = 1.0)
    {
        basegfx::B2DPolygon aPoly = basegfx::utils::createPolygonFromCircle(
            basegfx::B2DPoint(X(cx), Y(cy)), S(r));
        if (bFill)
        {
            mrDev.SetLineColor();
            mrDev.SetFillColor(aFillColor);
            mrDev.DrawPolygon(aPoly);
        }
        if (fStrokeMult > 0.0)
        {
            mrDev.SetFillColor();
            mrDev.SetLineColor(aStroke);
            mrDev.DrawPolyLine(aPoly, mfStroke * fStrokeMult, basegfx::B2DLineJoin::Round);
        }
    }

    void Label(const OUString& rText, double cx, double cy, double fPtH, Color aColor,
               FontWeight eWeight = WEIGHT_BOLD, FontItalic eItalic = ITALIC_NONE,
               const OUString& rFamily = u"Segoe UI"_ustr)
    {
        vcl::Font aFont(Application::GetSettings().GetStyleSettings().GetLabelFont());
        aFont.SetFamilyName(rFamily);
        aFont.SetFontHeight(static_cast<tools::Long>(std::lround(S(fPtH))));
        aFont.SetWeight(eWeight);
        aFont.SetItalic(eItalic);
        aFont.SetTransparent(true);
        mrDev.SetFont(aFont);
        mrDev.SetTextColor(aColor);
        const double half = S(9.0);
        tools::Rectangle aRect(
            static_cast<tools::Long>(std::lround(X(cx) - half)),
            static_cast<tools::Long>(std::lround(Y(cy) - half)),
            static_cast<tools::Long>(std::lround(X(cx) + half)),
            static_cast<tools::Long>(std::lround(Y(cy) + half)));
        mrDev.DrawText(aRect, rText, DrawTextFlags::Center | DrawTextFlags::VCenter);
    }
};

enum class IconKind
{
    None,
    // File & QAT
    Save, SaveAs, Open, NewDoc, Template, Print, PrintPreview, Pdf, Export,
    DocProps, Signature, Undo, Redo, Menubar, Close, Mail,
    // Clipboard
    Paste, Cut, Copy, Paintbrush, ClearFormat, Eraser,
    // Font
    Bold, Italic, Underline, Strikeout, Shadowed, SubScript, SuperScript,
    GrowFont, ShrinkFont, ChangeCase, FontColor, HighlightColor, Spacing, FontDialog,
    // Paragraph
    Bullets, Numbering, Outline, AlignLeft, AlignCenter, AlignRight, AlignJustify,
    IndentInc, IndentDec, HangingIndent, SpaceInc, SpaceDec, LineSpacing, Shading,
    Pilcrow, Ltr, Rtl, Borders,
    // Editing / Navigation
    Find, Replace, SelectPointer, SelectAll, GotoPage, Navigator, Sidebar,
    // Insert
    PageBreak, TitlePage, Section, Table, Picture, Gallery, Media, Chart,
    ObjectOle, FormulaMath, Hyperlink, Bookmark, CrossRef, Comment,
    Header, Footer, PageNumber, DateField, Field, TextBox, VerticalText,
    Fontwork, Symbol, Glossary, Frame, InsertFile,
    // Draw & Shapes
    SelectArrow, Freehand, Line, ArrowLine, Rect, Ellipse, BasicShapes,
    ArrowShapes, FlowchartShapes, CalloutShapes, StarShapes, SymbolShapes,
    ShapeFill, ShapeLine, EditPoints,
    // Design & Layout
    Themes, Watermark, PageSetup, PageBorder, StylesPane, LoadStyles, UpdateStyle, NewStyle,
    AutoFormat, Margins, Orientation, PageSize, Columns, Hyphenation, LineNumbers,
    Wrap, BringFront, SendBack, ForwardOne, BackOne, Group,
    // References
    Toc, IndexEntry, UpdateIndex, Footnote, Endnote, Caption, Bibliography,
    // Mailings
    Envelope, Labels, MailMerge, DataSource, FirstRec, PrevRec, NextRec, LastRec,
    MergeSave, MergePrint, MergeEmail,
    // Review
    Spelling, Thesaurus, WordCount, ReplyComment, DeleteComment,
    TrackChanges, ShowChanges, PrevChange, NextChange, AcceptChange, RejectChange,
    AcceptAll, RejectAll, Protect, Compare,
    // View & Help & Tools
    PrintLayout, WebLayout, Whitespace, ZoomOptimal, Zoom100, ZoomPage, ZoomWidth, ZoomDialog,
    FullScreen, Ruler, Grid, Guides, Help, CommandSearch, Options, Macro, SafeMode, About,
    // Table & Image contextual
    RowsAbove, RowsBelow, ColsBefore, ColsAfter, DeleteRows, DeleteCols, DeleteTable,
    MergeCells, SplitCell, SplitTable, SelectTable, VertTop, VertCenter, VertBottom,
    Sort, Sum, NumberFormat, CompressImg, Crop, RotateRight, FlipV, FlipH, Filters,
    // Calc / Spreadsheet specific
    WrapText, Currency, Percent, CommaNumber, IncDecimal, DecDecimal,
    ConditionalFormat, FormatTable, InsertCells, DeleteCells, FormatCells, FillDown,
    SortAsc, SortDesc, SortFilter, Filter,
    PivotTable, Sparkline, Clock, PrintArea, Headers, FunctionFx, DefinedName,
    TracePrecedents, TraceDependents, ErrorCheck, Calculate, WhatIf, DataRefresh,
    TextToColumns, RemoveDuplicates, DataValidation, OutlineGroup, NormalView,
    FreezePanes, FormulaEqual, FormulaOk, FormulaCancel
};

struct CommandMapEntry
{
    const char* name;
    IconKind kind;
    PaletteId palette;
};

const CommandMapEntry gCommandTable[] = {
    // File & QAT
    { "save", IconKind::Save, PaletteId::RoyalBlue },
    { "saveas", IconKind::SaveAs, PaletteId::RoyalBlue },
    { "saveacopy", IconKind::SaveAs, PaletteId::Teal },
    { "saveall", IconKind::Save, PaletteId::Indigo },
    { "open", IconKind::Open, PaletteId::Amber },
    { "openfromwriter", IconKind::Open, PaletteId::Amber },
    { "openreadonly", IconKind::Open, PaletteId::Teal },
    { "adddirect", IconKind::NewDoc, PaletteId::Emerald },
    { "newdoc", IconKind::NewDoc, PaletteId::Emerald },
    { "saveastemplate", IconKind::Template, PaletteId::Purple },
    { "print", IconKind::Print, PaletteId::Indigo },
    { "printdefault", IconKind::Print, PaletteId::RoyalBlue },
    { "printpreview", IconKind::PrintPreview, PaletteId::SkyBlue },
    { "printersetup", IconKind::Print, PaletteId::Slate },
    { "exportdirecttopdf", IconKind::Pdf, PaletteId::Red },
    { "exporttopdf", IconKind::Pdf, PaletteId::Red },
    { "exportto", IconKind::Export, PaletteId::Teal },
    { "exportdirecttoepub", IconKind::Export, PaletteId::Emerald },
    { "exporttoepub", IconKind::Export, PaletteId::Emerald },
    { "setdocumentproperties", IconKind::DocProps, PaletteId::SkyBlue },
    { "signature", IconKind::Signature, PaletteId::Purple },
    { "signpdf", IconKind::Signature, PaletteId::Red },
    { "insertsignatureline", IconKind::Signature, PaletteId::Indigo },
    { "undo", IconKind::Undo, PaletteId::SkyBlue },
    { "redo", IconKind::Redo, PaletteId::SkyBlue },
    { "menubar", IconKind::Menubar, PaletteId::Slate },
    { "closedoc", IconKind::Close, PaletteId::Rose },
    { "closepreview", IconKind::Close, PaletteId::Rose },
    { "sendmail", IconKind::Mail, PaletteId::SkyBlue },
    { "sendmaildocaspdf", IconKind::Pdf, PaletteId::Red },

    // Clipboard
    { "paste", IconKind::Paste, PaletteId::Orange },
    { "pastespecial", IconKind::Paste, PaletteId::Amber },
    { "pasteunformatted", IconKind::Paste, PaletteId::Teal },
    { "cut", IconKind::Cut, PaletteId::Red },
    { "copy", IconKind::Copy, PaletteId::RoyalBlue },
    { "formatpaintbrush", IconKind::Paintbrush, PaletteId::Amber },
    { "resetattributes", IconKind::ClearFormat, PaletteId::Rose },

    // Font
    { "bold", IconKind::Bold, PaletteId::Slate },
    { "italic", IconKind::Italic, PaletteId::Slate },
    { "underline", IconKind::Underline, PaletteId::RoyalBlue },
    { "overline", IconKind::Underline, PaletteId::Indigo },
    { "strikeout", IconKind::Strikeout, PaletteId::Rose },
    { "shadowed", IconKind::Shadowed, PaletteId::Purple },
    { "outlinefont", IconKind::Shadowed, PaletteId::Teal },
    { "subscript", IconKind::SubScript, PaletteId::SkyBlue },
    { "superscript", IconKind::SuperScript, PaletteId::SkyBlue },
    { "grow", IconKind::GrowFont, PaletteId::RoyalBlue },
    { "shrink", IconKind::ShrinkFont, PaletteId::Slate },
    { "color", IconKind::FontColor, PaletteId::Red },
    { "charbackcolor", IconKind::HighlightColor, PaletteId::Amber },
    { "spacing", IconKind::Spacing, PaletteId::Teal },
    { "fontdialog", IconKind::FontDialog, PaletteId::RoyalBlue },
    { "changecaserotatecase", IconKind::ChangeCase, PaletteId::Slate },
    { "changecasetotitlecase", IconKind::ChangeCase, PaletteId::Slate },
    { "changecasetoupper", IconKind::ChangeCase, PaletteId::Slate },
    { "changecasetolower", IconKind::ChangeCase, PaletteId::Slate },
    { "changecasetosentencecase", IconKind::ChangeCase, PaletteId::Slate },
    { "changecasetotogglecase", IconKind::ChangeCase, PaletteId::Slate },

    // Paragraph
    { "defaultbullet", IconKind::Bullets, PaletteId::RoyalBlue },
    { "defaultnumbering", IconKind::Numbering, PaletteId::Teal },
    { "setoutline", IconKind::Outline, PaletteId::Indigo },
    { "outlinebullet", IconKind::Outline, PaletteId::Purple },
    { "leftpara", IconKind::AlignLeft, PaletteId::Slate },
    { "startpara", IconKind::AlignLeft, PaletteId::Slate },
    { "centerpara", IconKind::AlignCenter, PaletteId::RoyalBlue },
    { "rightpara", IconKind::AlignRight, PaletteId::Slate },
    { "endpara", IconKind::AlignRight, PaletteId::Slate },
    { "justifypara", IconKind::AlignJustify, PaletteId::Slate },
    { "incrementindent", IconKind::IndentInc, PaletteId::RoyalBlue },
    { "decrementindent", IconKind::IndentDec, PaletteId::Slate },
    { "hangingindent", IconKind::HangingIndent, PaletteId::Teal },
    { "paraspaceincrease", IconKind::SpaceInc, PaletteId::SkyBlue },
    { "paraspacedecrease", IconKind::SpaceDec, PaletteId::Slate },
    { "linespacing", IconKind::LineSpacing, PaletteId::RoyalBlue },
    { "backgroundcolor", IconKind::Shading, PaletteId::Amber },
    { "controlcodes", IconKind::Pilcrow, PaletteId::Purple },
    { "paralefttoright", IconKind::Ltr, PaletteId::Teal },
    { "pararighttoleft", IconKind::Rtl, PaletteId::Orange },
    { "setborderstyle", IconKind::Borders, PaletteId::SkyBlue },
    { "paragraphdialog", IconKind::LineSpacing, PaletteId::Indigo },

    // Editing & Navigation
    { "searchdialog", IconKind::Find, PaletteId::RoyalBlue },
    { "searchreplace", IconKind::Replace, PaletteId::RoyalBlue },
    { "selectall", IconKind::SelectAll, PaletteId::Teal },
    { "selectobject", IconKind::SelectPointer, PaletteId::Slate },
    { "gotopage", IconKind::GotoPage, PaletteId::Orange },
    { "jumptospecificpage", IconKind::GotoPage, PaletteId::Orange },
    { "navigator", IconKind::Navigator, PaletteId::Orange },
    { "sidebar", IconKind::Sidebar, PaletteId::RoyalBlue },

    // Insert
    { "insertpagebreak", IconKind::PageBreak, PaletteId::RoyalBlue },
    { "insertbreak", IconKind::PageBreak, PaletteId::Indigo },
    { "titlepagedialog", IconKind::TitlePage, PaletteId::Indigo },
    { "insertsection", IconKind::Section, PaletteId::Teal },
    { "inserttable", IconKind::Table, PaletteId::Emerald },
    { "insertgraphic", IconKind::Picture, PaletteId::Orange },
    { "changepicture", IconKind::Picture, PaletteId::Orange },
    { "gallery", IconKind::Gallery, PaletteId::Emerald },
    { "insertavmedia", IconKind::Media, PaletteId::Purple },
    { "insertobjectchart", IconKind::Chart, PaletteId::RoyalBlue },
    { "insertobject", IconKind::ObjectOle, PaletteId::Teal },
    { "insertobjectstarmath", IconKind::FormulaMath, PaletteId::Purple },
    { "hyperlinkdialog", IconKind::Hyperlink, PaletteId::SkyBlue },
    { "inserthyperlink", IconKind::Hyperlink, PaletteId::SkyBlue },
    { "insertbookmark", IconKind::Bookmark, PaletteId::Amber },
    { "insertreferencefield", IconKind::CrossRef, PaletteId::Teal },
    { "insertannotation", IconKind::Comment, PaletteId::Amber },
    { "insertpageheader", IconKind::Header, PaletteId::RoyalBlue },
    { "insertpagefooter", IconKind::Footer, PaletteId::Indigo },
    { "useheaderfootermenu", IconKind::Header, PaletteId::RoyalBlue },
    { "insertpagenumberfield", IconKind::PageNumber, PaletteId::Teal },
    { "insertpagecountfield", IconKind::PageNumber, PaletteId::Indigo },
    { "pagenumberwizard", IconKind::PageNumber, PaletteId::RoyalBlue },
    { "insertdatefield", IconKind::DateField, PaletteId::Orange },
    { "insertfieldctrl", IconKind::Field, PaletteId::Purple },
    { "insertfield", IconKind::Field, PaletteId::Purple },
    { "inserttitlefield", IconKind::Field, PaletteId::SkyBlue },
    { "drawtext", IconKind::TextBox, PaletteId::RoyalBlue },
    { "verticaltext", IconKind::VerticalText, PaletteId::Indigo },
    { "fontworkgalleryfloater", IconKind::Fontwork, PaletteId::Purple },
    { "charmapcontrol", IconKind::Symbol, PaletteId::Purple },
    { "insertsymbol", IconKind::Symbol, PaletteId::Purple },
    { "editglossary", IconKind::Glossary, PaletteId::Teal },
    { "insertframe", IconKind::Frame, PaletteId::SkyBlue },
    { "insertframeinteract", IconKind::Frame, PaletteId::SkyBlue },
    { "insertdoc", IconKind::InsertFile, PaletteId::Emerald },
    { "insertqrcode", IconKind::Grid, PaletteId::Slate },

    // Draw & Shapes
    { "selectobject", IconKind::SelectArrow, PaletteId::Slate },
    { "freeline_unfilled", IconKind::Freehand, PaletteId::Rose },
    { "insertdraw", IconKind::Freehand, PaletteId::Orange },
    { "line", IconKind::Line, PaletteId::RoyalBlue },
    { "linetoolbox", IconKind::Line, PaletteId::RoyalBlue },
    { "linearrowend", IconKind::ArrowLine, PaletteId::Orange },
    { "arrowstoolbox", IconKind::ArrowLine, PaletteId::Orange },
    { "connectortoolbox", IconKind::ArrowShapes, PaletteId::Teal },
    { "rect", IconKind::Rect, PaletteId::RoyalBlue },
    { "basicshapes.rectangle", IconKind::Rect, PaletteId::RoyalBlue },
    { "ellipse", IconKind::Ellipse, PaletteId::Emerald },
    { "basicshapes.ellipse", IconKind::Ellipse, PaletteId::Emerald },
    { "basicshapes", IconKind::BasicShapes, PaletteId::Purple },
    { "symbolshapes", IconKind::SymbolShapes, PaletteId::Amber },
    { "arrowshapes", IconKind::ArrowShapes, PaletteId::Orange },
    { "flowchartshapes", IconKind::FlowchartShapes, PaletteId::Teal },
    { "calloutshapes", IconKind::CalloutShapes, PaletteId::SkyBlue },
    { "drawcaption", IconKind::CalloutShapes, PaletteId::SkyBlue },
    { "starshapes", IconKind::StarShapes, PaletteId::Amber },
    { "formatarea", IconKind::ShapeFill, PaletteId::Orange },
    { "fillstyle", IconKind::ShapeFill, PaletteId::Amber },
    { "formatline", IconKind::ShapeLine, PaletteId::RoyalBlue },
    { "xlinestyle", IconKind::ShapeLine, PaletteId::Indigo },
    { "xlinecolor", IconKind::ShapeLine, PaletteId::Rose },
    { "toggleobjectbeziermode", IconKind::EditPoints, PaletteId::Purple },

    // Design & Layout
    { "themedialog", IconKind::Themes, PaletteId::Purple },
    { "watermark", IconKind::Watermark, PaletteId::SkyBlue },
    { "pagedialog", IconKind::PageSetup, PaletteId::Indigo },
    { "borderdialog", IconKind::PageBorder, PaletteId::RoyalBlue },
    { "designerdialog", IconKind::StylesPane, PaletteId::Purple },
    { "loadstyles", IconKind::LoadStyles, PaletteId::Teal },
    { "styleupdatebyexample", IconKind::UpdateStyle, PaletteId::Emerald },
    { "stylenewbyexample", IconKind::NewStyle, PaletteId::Orange },
    { "autoformatapply", IconKind::AutoFormat, PaletteId::Amber },
    { "autoformatmenu", IconKind::AutoFormat, PaletteId::Amber },
    { "pagemargin", IconKind::Margins, PaletteId::RoyalBlue },
    { "orientation", IconKind::Orientation, PaletteId::Orange },
    { "attributepagesize", IconKind::PageSize, PaletteId::Teal },
    { "pagecolumntype", IconKind::Columns, PaletteId::Indigo },
    { "pagecolumndialog", IconKind::Columns, PaletteId::Indigo },
    { "hyphenate", IconKind::Hyphenation, PaletteId::SkyBlue },
    { "linenumberingdialog", IconKind::LineNumbers, PaletteId::Teal },
    { "wrapoff", IconKind::Wrap, PaletteId::Slate },
    { "wrapon", IconKind::Wrap, PaletteId::RoyalBlue },
    { "wrapideal", IconKind::Wrap, PaletteId::Emerald },
    { "wrapleft", IconKind::Wrap, PaletteId::Teal },
    { "wrapright", IconKind::Wrap, PaletteId::Teal },
    { "wrapthrough", IconKind::Wrap, PaletteId::Purple },
    { "textwrap", IconKind::Wrap, PaletteId::RoyalBlue },
    { "bringtofront", IconKind::BringFront, PaletteId::Orange },
    { "sendtoback", IconKind::SendBack, PaletteId::Slate },
    { "objectforwardone", IconKind::ForwardOne, PaletteId::SkyBlue },
    { "objectbackone", IconKind::BackOne, PaletteId::Slate },
    { "formatgroup", IconKind::Group, PaletteId::Purple },
    { "entergroup", IconKind::Group, PaletteId::Teal },
    { "leavegroup", IconKind::Group, PaletteId::Orange },

    // References
    { "insertmultiindex", IconKind::Toc, PaletteId::RoyalBlue },
    { "insertindexesentry", IconKind::IndexEntry, PaletteId::Teal },
    { "updatecurindex", IconKind::UpdateIndex, PaletteId::Emerald },
    { "updateallindexes", IconKind::UpdateIndex, PaletteId::Emerald },
    { "updateall", IconKind::UpdateIndex, PaletteId::RoyalBlue },
    { "insertfootnote", IconKind::Footnote, PaletteId::Indigo },
    { "insertendnote", IconKind::Endnote, PaletteId::Purple },
    { "insertcaptiondialog", IconKind::Caption, PaletteId::Orange },
    { "insertauthoritiesentry", IconKind::Bibliography, PaletteId::Amber },
    { "bibliographycomponent", IconKind::Bibliography, PaletteId::Amber },

    // Mailings
    { "insertenvelope", IconKind::Envelope, PaletteId::SkyBlue },
    { "insertlabels", IconKind::Labels, PaletteId::Orange },
    { "mailmergewizard", IconKind::MailMerge, PaletteId::RoyalBlue },
    { "viewdatasourcebrowser", IconKind::DataSource, PaletteId::Emerald },
    { "addressbooksource", IconKind::DataSource, PaletteId::Teal },
    { "mailmergefirstentry", IconKind::FirstRec, PaletteId::Slate },
    { "mailmergepreventry", IconKind::PrevRec, PaletteId::Slate },
    { "mailmergenextentry", IconKind::NextRec, PaletteId::RoyalBlue },
    { "mailmergelastentry", IconKind::LastRec, PaletteId::Slate },
    { "mailmergecreatedocuments", IconKind::MergeSave, PaletteId::Indigo },
    { "mailmergesavedocuments", IconKind::MergeSave, PaletteId::Emerald },
    { "mailmergeprintdocuments", IconKind::MergePrint, PaletteId::Orange },
    { "mailmergeemaildocuments", IconKind::MergeEmail, PaletteId::SkyBlue },

    // Review
    { "spellingandgrammardialog", IconKind::Spelling, PaletteId::Emerald },
    { "thesaurusdialog", IconKind::Thesaurus, PaletteId::Purple },
    { "spellonline", IconKind::Spelling, PaletteId::RoyalBlue },
    { "wordcountdialog", IconKind::WordCount, PaletteId::SkyBlue },
    { "replycomment", IconKind::ReplyComment, PaletteId::Teal },
    { "deletecomment", IconKind::DeleteComment, PaletteId::Red },
    { "trackchanges", IconKind::TrackChanges, PaletteId::Rose },
    { "showtrackedchanges", IconKind::ShowChanges, PaletteId::Orange },
    { "previoustrackedchange", IconKind::PrevChange, PaletteId::Slate },
    { "nexttrackedchange", IconKind::NextChange, PaletteId::RoyalBlue },
    { "accepttrackedchange", IconKind::AcceptChange, PaletteId::Emerald },
    { "accepttrackedchangetonext", IconKind::AcceptChange, PaletteId::Emerald },
    { "acceptalltrackedchanges", IconKind::AcceptAll, PaletteId::Emerald },
    { "accepttrackedchanges", IconKind::TrackChanges, PaletteId::Emerald },
    { "rejecttrackedchange", IconKind::RejectChange, PaletteId::Red },
    { "rejecttrackedchangetonext", IconKind::RejectChange, PaletteId::Red },
    { "rejectalltrackedchanges", IconKind::RejectAll, PaletteId::Red },
    { "protect", IconKind::Protect, PaletteId::Indigo },
    { "comparedocuments", IconKind::Compare, PaletteId::Purple },

    // View, Help & Tools
    { "printlayout", IconKind::PrintLayout, PaletteId::RoyalBlue },
    { "browseview", IconKind::WebLayout, PaletteId::SkyBlue },
    { "showwhitespace", IconKind::Whitespace, PaletteId::Teal },
    { "zoomoptimal", IconKind::ZoomOptimal, PaletteId::Teal },
    { "zoom100percent", IconKind::Zoom100, PaletteId::RoyalBlue },
    { "zoompage", IconKind::ZoomPage, PaletteId::Indigo },
    { "zoompagewidth", IconKind::ZoomWidth, PaletteId::Orange },
    { "zoom", IconKind::ZoomDialog, PaletteId::RoyalBlue },
    { "fullscreen", IconKind::FullScreen, PaletteId::Purple },
    { "ruler", IconKind::Ruler, PaletteId::Amber },
    { "gridvisible", IconKind::Grid, PaletteId::Teal },
    { "griduse", IconKind::Grid, PaletteId::Indigo },
    { "helplinesmove", IconKind::Guides, PaletteId::Orange },
    { "changetheme", IconKind::Themes, PaletteId::Purple },
    { "helpindex", IconKind::Help, PaletteId::RoyalBlue },
    { "extendedhelp", IconKind::Help, PaletteId::SkyBlue },
    { "commandpopup", IconKind::CommandSearch, PaletteId::Indigo },
    { "optionstreedialog", IconKind::Options, PaletteId::Slate },
    { "configuredialog", IconKind::Options, PaletteId::RoyalBlue },
    { "runmacro", IconKind::Macro, PaletteId::Emerald },
    { "basicideappear", IconKind::Macro, PaletteId::Purple },
    { "macrodialog", IconKind::Macro, PaletteId::Orange },
    { "safemode", IconKind::SafeMode, PaletteId::Amber },
    { "showlicense", IconKind::About, PaletteId::Teal },
    { "about", IconKind::About, PaletteId::RoyalBlue },

    // Table & Image Contextual
    { "insertrowsbefore", IconKind::RowsAbove, PaletteId::Emerald },
    { "insertrowsafter", IconKind::RowsBelow, PaletteId::Emerald },
    { "insertcolumnsbefore", IconKind::ColsBefore, PaletteId::RoyalBlue },
    { "insertcolumnsafter", IconKind::ColsAfter, PaletteId::RoyalBlue },
    { "deleterows", IconKind::DeleteRows, PaletteId::Red },
    { "deletecolumns", IconKind::DeleteCols, PaletteId::Red },
    { "deletetable", IconKind::DeleteTable, PaletteId::Red },
    { "mergecells", IconKind::MergeCells, PaletteId::Purple },
    { "splitcell", IconKind::SplitCell, PaletteId::Orange },
    { "splittable", IconKind::SplitTable, PaletteId::Orange },
    { "selecttable", IconKind::SelectTable, PaletteId::RoyalBlue },
    { "entirecell", IconKind::SelectTable, PaletteId::Teal },
    { "entirerow", IconKind::RowsBelow, PaletteId::Teal },
    { "entirecolumn", IconKind::ColsAfter, PaletteId::Teal },
    { "autoformat", IconKind::AutoFormat, PaletteId::Purple },
    { "tablecellbackgroundcolor", IconKind::Shading, PaletteId::Amber },
    { "optimizetable", IconKind::AutoFormat, PaletteId::Teal },
    { "cellverttop", IconKind::VertTop, PaletteId::Slate },
    { "cellvertcenter", IconKind::VertCenter, PaletteId::RoyalBlue },
    { "cellvertbottom", IconKind::VertBottom, PaletteId::Slate },
    { "tablesort", IconKind::Sort, PaletteId::Indigo },
    { "insertformula", IconKind::Sum, PaletteId::Purple },
    { "autosum", IconKind::Sum, PaletteId::Orange },
    { "tablenumberformatdialog", IconKind::NumberFormat, PaletteId::Teal },
    { "tabledialog", IconKind::Table, PaletteId::RoyalBlue },
    { "compressgraphic", IconKind::CompressImg, PaletteId::Teal },
    { "crop", IconKind::Crop, PaletteId::Orange },
    { "rotateright", IconKind::RotateRight, PaletteId::RoyalBlue },
    { "toggleobjectrotatemode", IconKind::RotateRight, PaletteId::Orange },
    { "flipvertical", IconKind::FlipV, PaletteId::Purple },
    { "fliphorizontal", IconKind::FlipH, PaletteId::Teal },
    { "graphicfiltertoolbox", IconKind::Filters, PaletteId::Purple },
    { "colorsettings", IconKind::Filters, PaletteId::Orange },
    { "savegraphic", IconKind::Save, PaletteId::Emerald },
    { "graphicdialog", IconKind::Picture, PaletteId::RoyalBlue },

    // Calc / Spreadsheet specific commands
    { "openfromcalc", IconKind::Open, PaletteId::Amber },
    { "aligntop", IconKind::VertTop, PaletteId::Slate },
    { "alignvcenter", IconKind::VertCenter, PaletteId::RoyalBlue },
    { "alignbottom", IconKind::VertBottom, PaletteId::Slate },
    { "alignleft", IconKind::AlignLeft, PaletteId::Slate },
    { "alignhorizontalcenter", IconKind::AlignCenter, PaletteId::RoyalBlue },
    { "alignright", IconKind::AlignRight, PaletteId::Slate },
    { "alignblock", IconKind::AlignJustify, PaletteId::Slate },
    { "wraptext", IconKind::WrapText, PaletteId::Emerald },
    { "togglemergecells", IconKind::MergeCells, PaletteId::Purple },
    { "numberformatcurrency", IconKind::Currency, PaletteId::Emerald },
    { "numberformatpercent", IconKind::Percent, PaletteId::RoyalBlue },
    { "numberformatdecimal", IconKind::CommaNumber, PaletteId::Teal },
    { "numberformatincdecimals", IconKind::IncDecimal, PaletteId::Teal },
    { "numberformatdecdecimals", IconKind::DecDecimal, PaletteId::Slate },
    { "numberformatdate", IconKind::DateField, PaletteId::Orange },
    { "numberformatstandard", IconKind::NumberFormat, PaletteId::Teal },
    { "numberformattype", IconKind::NumberFormat, PaletteId::Teal },
    { "conditionalformatmenu", IconKind::ConditionalFormat, PaletteId::Orange },
    { "insertcalctable", IconKind::FormatTable, PaletteId::Emerald },
    { "choosedesign", IconKind::Themes, PaletteId::Purple },
    { "databasesettings", IconKind::FormatTable, PaletteId::Teal },
    { "removecalctable", IconKind::DeleteTable, PaletteId::Red },
    { "insertcell", IconKind::InsertCells, PaletteId::RoyalBlue },
    { "deletecell", IconKind::DeleteCells, PaletteId::Red },
    { "rowoperations", IconKind::RowsBelow, PaletteId::Emerald },
    { "columnoperations", IconKind::ColsAfter, PaletteId::RoyalBlue },
    { "formatcelldialog", IconKind::FormatCells, PaletteId::RoyalBlue },
    { "insert", IconKind::NewDoc, PaletteId::Emerald },
    { "remove", IconKind::DeleteTable, PaletteId::Red },
    { "renametable", IconKind::DefinedName, PaletteId::SkyBlue },
    { "filldown", IconKind::FillDown, PaletteId::RoyalBlue },
    { "fillright", IconKind::FillDown, PaletteId::Teal },
    { "fillseries", IconKind::FillDown, PaletteId::Indigo },
    { "clearcontents", IconKind::Eraser, PaletteId::Rose },
    { "delete", IconKind::Eraser, PaletteId::Rose },
    { "sortascending", IconKind::SortAsc, PaletteId::RoyalBlue },
    { "sortdescending", IconKind::SortDesc, PaletteId::Orange },
    { "datasort", IconKind::Sort, PaletteId::RoyalBlue },
    { "sortdialog", IconKind::Sort, PaletteId::RoyalBlue },
    { "datafilterautofilter", IconKind::Filter, PaletteId::Purple },
    { "datafilterstandardfilter", IconKind::Filter, PaletteId::SkyBlue },
    { "datafilterspecialfilter", IconKind::Filter, PaletteId::Indigo },
    { "datafilterremovefilter", IconKind::ClearFormat, PaletteId::Rose },
    { "datafilterhideautofilter", IconKind::Filter, PaletteId::Slate },
    { "datadatapilotrun", IconKind::PivotTable, PaletteId::Teal },
    { "insertpivottable", IconKind::PivotTable, PaletteId::Teal },
    { "recalcpivottable", IconKind::DataRefresh, PaletteId::Emerald },
    { "deletepivottable", IconKind::DeleteTable, PaletteId::Red },
    { "insertcalculation", IconKind::FunctionFx, PaletteId::Purple },
    { "datapilotfilter", IconKind::Filter, PaletteId::SkyBlue },
    { "insertsparkline", IconKind::Sparkline, PaletteId::Emerald },
    { "editsparkline", IconKind::Sparkline, PaletteId::Teal },
    { "deletesparkline", IconKind::DeleteTable, PaletteId::Red },
    { "editheaderandfooter", IconKind::Header, PaletteId::RoyalBlue },
    { "insertcurrentdate", IconKind::DateField, PaletteId::Orange },
    { "insertcurrenttime", IconKind::Clock, PaletteId::SkyBlue },
    { "pageformatdialog", IconKind::PageSetup, PaletteId::Indigo },
    { "defineprintarea", IconKind::PrintArea, PaletteId::RoyalBlue },
    { "editprintarea", IconKind::PrintArea, PaletteId::Teal },
    { "addprintarea", IconKind::PrintArea, PaletteId::Emerald },
    { "deleteprintarea", IconKind::ClearFormat, PaletteId::Rose },
    { "margins", IconKind::Margins, PaletteId::RoyalBlue },
    { "insertrowbreak", IconKind::PageBreak, PaletteId::RoyalBlue },
    { "insertcolumnbreak", IconKind::PageBreak, PaletteId::Indigo },
    { "deleteallbreaks", IconKind::ClearFormat, PaletteId::Rose },
    { "togglesheetgrid", IconKind::Grid, PaletteId::Teal },
    { "viewrowcolumnheaders", IconKind::Headers, PaletteId::Emerald },
    { "sheetrighttoleft", IconKind::Rtl, PaletteId::Orange },
    { "objectalign", IconKind::AlignCenter, PaletteId::RoyalBlue },
    { "objectalignleft", IconKind::AlignLeft, PaletteId::Slate },
    { "aligncenter", IconKind::AlignCenter, PaletteId::RoyalBlue },
    { "objectalignright", IconKind::AlignRight, PaletteId::Slate },
    { "alignup", IconKind::VertTop, PaletteId::Slate },
    { "alignmiddle", IconKind::VertCenter, PaletteId::RoyalBlue },
    { "aligndown", IconKind::VertBottom, PaletteId::Slate },
    { "functiondialog", IconKind::FunctionFx, PaletteId::Emerald },
    { "insertfunction", IconKind::FunctionFx, PaletteId::Emerald },
    { "functionbox", IconKind::FunctionFx, PaletteId::Teal },
    { "addname", IconKind::DefinedName, PaletteId::Indigo },
    { "definename", IconKind::DefinedName, PaletteId::RoyalBlue },
    { "createnames", IconKind::DefinedName, PaletteId::Teal },
    { "insertname", IconKind::DefinedName, PaletteId::Purple },
    { "convertformulatovalue", IconKind::NumberFormat, PaletteId::Amber },
    { "showprecedents", IconKind::TracePrecedents, PaletteId::RoyalBlue },
    { "showdependents", IconKind::TraceDependents, PaletteId::Orange },
    { "cleararrows", IconKind::ClearFormat, PaletteId::Rose },
    { "cleararrowprecedents", IconKind::ClearFormat, PaletteId::Rose },
    { "cleararrowdependents", IconKind::ClearFormat, PaletteId::Rose },
    { "toggleformula", IconKind::FunctionFx, PaletteId::Purple },
    { "showerrors", IconKind::ErrorCheck, PaletteId::Rose },
    { "showinvalid", IconKind::ErrorCheck, PaletteId::Amber },
    { "auditingfillmode", IconKind::Find, PaletteId::Teal },
    { "calculate", IconKind::Calculate, PaletteId::Emerald },
    { "calculatehard", IconKind::DataRefresh, PaletteId::Teal },
    { "automaticcalculation", IconKind::Calculate, PaletteId::RoyalBlue },
    { "goalseekdialog", IconKind::WhatIf, PaletteId::Purple },
    { "solverdialog", IconKind::WhatIf, PaletteId::Indigo },
    { "insertexternaldatasource", IconKind::DataSource, PaletteId::Emerald },
    { "dataprovider", IconKind::DataSource, PaletteId::RoyalBlue },
    { "dataproviderrefresh", IconKind::DataRefresh, PaletteId::Teal },
    { "dataarearefresh", IconKind::DataRefresh, PaletteId::Emerald },
    { "managexmlsource", IconKind::DataSource, PaletteId::Orange },
    { "datastreams", IconKind::DataSource, PaletteId::SkyBlue },
    { "editlinks", IconKind::Hyperlink, PaletteId::SkyBlue },
    { "definedbname", IconKind::DefinedName, PaletteId::Teal },
    { "texttocolumns", IconKind::TextToColumns, PaletteId::RoyalBlue },
    { "handleduplicaterecords", IconKind::RemoveDuplicates, PaletteId::Rose },
    { "validation", IconKind::DataValidation, PaletteId::Emerald },
    { "dataconsolidate", IconKind::Table, PaletteId::Indigo },
    { "dataform", IconKind::Table, PaletteId::Teal },
    { "datasubtotals", IconKind::Sum, PaletteId::Orange },
    { "group", IconKind::OutlineGroup, PaletteId::SkyBlue },
    { "ungroup", IconKind::OutlineGroup, PaletteId::Orange },
    { "showdetail", IconKind::OutlineGroup, PaletteId::Emerald },
    { "hidedetail", IconKind::OutlineGroup, PaletteId::Slate },
    { "spelldialog", IconKind::Spelling, PaletteId::Emerald },
    { "editannotation", IconKind::Comment, PaletteId::Amber },
    { "showallnotes", IconKind::Comment, PaletteId::SkyBlue },
    { "hideallnotes", IconKind::Comment, PaletteId::Slate },
    { "deletenote", IconKind::DeleteComment, PaletteId::Red },
    { "deleteallnotes", IconKind::DeleteComment, PaletteId::Red },
    { "toolprotectiondocument", IconKind::Protect, PaletteId::Indigo },
    { "sharedocument", IconKind::Protect, PaletteId::Teal },
    { "tracechangemode", IconKind::TrackChanges, PaletteId::Rose },
    { "acceptchanges", IconKind::AcceptChange, PaletteId::Emerald },
    { "mergedocuments", IconKind::Compare, PaletteId::Purple },
    { "redactdoc", IconKind::Protect, PaletteId::Amber },
    { "normalviewmode", IconKind::NormalView, PaletteId::Emerald },
    { "pagebreakmode", IconKind::PageBreak, PaletteId::Indigo },
    { "inputlinevisible", IconKind::FunctionFx, PaletteId::Teal },
    { "freezepanes", IconKind::FreezePanes, PaletteId::SkyBlue },
    { "freezepanesrow", IconKind::FreezePanes, PaletteId::RoyalBlue },
    { "freezepanescolumn", IconKind::FreezePanes, PaletteId::Teal },
    { "splitwindow", IconKind::SplitTable, PaletteId::Orange },
    { "newwindow", IconKind::NewDoc, PaletteId::RoyalBlue },
    { "viewvaluehighlighting", IconKind::HighlightColor, PaletteId::Amber },
    { "viewcolumnrowhighlighting", IconKind::Headers, PaletteId::SkyBlue },
    { "viewhiddencolrow", IconKind::Headers, PaletteId::Orange },
    { "originalsize", IconKind::Zoom100, PaletteId::Teal },
    { "transformdialog", IconKind::PageSetup, PaletteId::RoyalBlue },
    { "zoomminus", IconKind::ShrinkFont, PaletteId::Slate },
    { "zoomplus", IconKind::GrowFont, PaletteId::RoyalBlue },
    { "firstpage", IconKind::FirstRec, PaletteId::Slate },
    { "previouspage", IconKind::PrevRec, PaletteId::Slate },
    { "nextpage", IconKind::NextRec, PaletteId::RoyalBlue },
    { "lastpage", IconKind::LastRec, PaletteId::Slate },
    { "assignmacro", IconKind::Macro, PaletteId::Purple },
    { "avmediaplayer", IconKind::Media, PaletteId::Purple },
    { "avmediatoolbox", IconKind::Media, PaletteId::Indigo }
};

void DrawPageSheet(Canvas& c, Color aBorder, bool bLines = true)
{
    c.Poly({ { 6.5, 4.5 }, { 14.5, 4.5 }, { 17.5, 7.5 }, { 17.5, 19.5 }, { 6.5, 19.5 } },
           true, aBorder, 0.95, true, COL_WHITE);
    c.Poly({ { 14.5, 4.5 }, { 14.5, 7.5 }, { 17.5, 7.5 } }, false, aBorder, 0.85);
    if (bLines)
    {
        c.Line(8.8, 10.5, 15.2, 10.5, Slate300, 0.75);
        c.Line(8.8, 13.5, 15.2, 13.5, Slate300, 0.75);
        c.Line(8.8, 16.5, 13.2, 16.5, Slate300, 0.75);
    }
}

void DrawIconArtwork(Canvas& c, IconKind eKind, const PalettePair& rPal)
{
    const Color fg = rPal.fg;
    const Color dk = rPal.dark;

    switch (eKind)
    {
        case IconKind::Save:
        case IconKind::SaveAs:
        {
            c.RoundRect(5.2, 5.2, 18.8, 18.8, 2.0, fg, true, dk, 0.7);
            c.RoundRect(7.8, 5.4, 15.2, 10.2, 0.8, COL_WHITE, true, COL_WHITE, 0.0);
            c.RoundRect(12.2, 6.2, 14.0, 9.2, 0.4, dk, true, dk, 0.0);
            c.RoundRect(7.4, 12.6, 16.6, 18.2, 1.0, COL_WHITE, true, COL_WHITE, 0.0);
            c.Line(9.0, 14.8, 15.0, 14.8, Slate400, 0.65);
            c.Line(9.0, 16.6, 13.5, 16.6, Slate400, 0.65);
            if (eKind == IconKind::SaveAs)
            {
                c.Circle(16.8, 16.8, 3.0, COL_WHITE, true, Amber, 0.6);
            }
            break;
        }
        case IconKind::Open:
        {
            c.Poly({ { 4.8, 7.0 }, { 9.5, 7.0 }, { 11.2, 8.8 }, { 18.6, 8.8 }, { 18.6, 17.8 }, { 4.8, 17.8 } },
                   true, dk, 0.8, true, fg);
            c.Poly({ { 4.8, 17.8 }, { 6.5, 10.8 }, { 19.6, 10.8 }, { 18.2, 17.8 } },
                   true, dk, 0.75, true, AmberTile);
            break;
        }
        case IconKind::NewDoc:
        {
            DrawPageSheet(c, fg, false);
            c.Circle(16.2, 16.2, 3.3, COL_WHITE, true, Emerald, 0.65);
            c.Line(16.2, 14.5, 16.2, 17.9, COL_WHITE, 0.9);
            c.Line(14.5, 16.2, 17.9, 16.2, COL_WHITE, 0.9);
            break;
        }
        case IconKind::Template:
        {
            DrawPageSheet(c, fg, false);
            c.RoundRect(8.5, 9.5, 15.5, 13.5, 0.8, PurpleTile, true, fg, 0.6);
            c.Line(8.5, 15.6, 15.5, 15.6, fg, 0.75);
            c.Line(8.5, 17.4, 13.0, 17.4, Slate400, 0.75);
            break;
        }
        case IconKind::Print:
        case IconKind::MergePrint:
        {
            c.RoundRect(7.5, 5.0, 16.5, 9.2, 0.8, COL_WHITE, true, Slate600, 0.75);
            c.RoundRect(4.8, 8.6, 19.2, 15.0, 1.6, Slate700, true, Slate900, 0.7);
            c.Circle(16.8, 11.0, 0.9, Emerald, true, Emerald, 0.0);
            c.RoundRect(7.4, 12.6, 16.6, 18.8, 0.8, COL_WHITE, true, fg, 0.75);
            c.Line(9.0, 14.8, 15.0, 14.8, fg, 0.7);
            c.Line(9.0, 16.8, 13.8, 16.8, Slate400, 0.7);
            break;
        }
        case IconKind::PrintPreview:
        case IconKind::ZoomPage:
        case IconKind::ZoomOptimal:
        {
            DrawPageSheet(c, Slate500, true);
            c.Circle(14.8, 14.8, 3.5, fg, true, COL_WHITE, 0.95);
            c.Line(17.3, 17.3, 19.5, 19.5, fg, 1.25);
            break;
        }
        case IconKind::Pdf:
        {
            c.RoundRect(5.2, 5.0, 18.8, 19.0, 2.2, fg, true, dk, 0.7);
            c.Label(u"PDF"_ustr, 12.0, 12.2, 7.2, COL_WHITE, WEIGHT_BOLD);
            break;
        }
        case IconKind::Export:
        {
            DrawPageSheet(c, Slate500, false);
            c.Line(11.0, 12.5, 19.0, 12.5, fg, 1.05);
            c.Poly({ { 16.2, 9.8 }, { 19.2, 12.5 }, { 16.2, 15.2 } }, false, fg, 1.05);
            break;
        }
        case IconKind::DocProps:
        case IconKind::About:
        {
            c.Circle(12.0, 12.0, 7.0, dk, true, fg, 0.7);
            c.Circle(12.0, 8.3, 1.0, COL_WHITE, true, COL_WHITE, 0.0);
            c.Line(12.0, 11.2, 12.0, 16.2, COL_WHITE, 1.25);
            break;
        }
        case IconKind::Signature:
        {
            c.Line(5.5, 17.5, 18.5, 17.5, Slate400, 0.85);
            c.Poly({ { 6.2, 14.5 }, { 8.5, 10.5 }, { 10.5, 15.2 }, { 13.0, 11.8 }, { 15.2, 14.6 } },
                   false, fg, 1.0);
            c.Poly({ { 13.5, 12.5 }, { 18.2, 6.2 }, { 19.6, 7.6 }, { 14.9, 13.9 } },
                   true, dk, 0.65, true, Amber);
            break;
        }
        case IconKind::Undo:
        {
            c.Poly({ { 17.5, 16.5 }, { 17.5, 12.0 }, { 15.2, 9.5 }, { 7.5, 9.5 } }, false, fg, 1.25);
            c.Poly({ { 10.5, 6.2 }, { 6.5, 9.5 }, { 10.5, 12.8 } }, false, fg, 1.25);
            break;
        }
        case IconKind::Redo:
        {
            c.Poly({ { 6.5, 16.5 }, { 6.5, 12.0 }, { 8.8, 9.5 }, { 16.5, 9.5 } }, false, fg, 1.25);
            c.Poly({ { 13.5, 6.2 }, { 17.5, 9.5 }, { 13.5, 12.8 } }, false, fg, 1.25);
            break;
        }
        case IconKind::Menubar:
        {
            c.Line(5.5, 7.2, 18.5, 7.2, fg, 1.15);
            c.Line(5.5, 12.0, 18.5, 12.0, fg, 1.15);
            c.Line(5.5, 16.8, 18.5, 16.8, fg, 1.15);
            break;
        }
        case IconKind::Close:
        case IconKind::RejectChange:
        case IconKind::RejectAll:
        {
            c.Line(6.8, 6.8, 17.2, 17.2, fg, 1.3);
            c.Line(17.2, 6.8, 6.8, 17.2, fg, 1.3);
            break;
        }
        case IconKind::Mail:
        case IconKind::Envelope:
        case IconKind::MergeEmail:
        {
            c.RoundRect(4.8, 6.8, 19.2, 17.2, 1.6, COL_WHITE, true, fg, 0.95);
            c.Poly({ { 5.4, 7.8 }, { 12.0, 12.8 }, { 18.6, 7.8 } }, false, fg, 0.95);
            break;
        }
        case IconKind::Paste:
        {
            c.RoundRect(5.6, 6.2, 15.6, 17.6, 1.4, fg, true, dk, 0.75);
            c.RoundRect(8.6, 4.6, 12.6, 7.2, 0.8, Amber, true, AmberDark, 0.6);
            c.RoundRect(10.2, 9.2, 18.4, 19.2, 1.1, COL_WHITE, true, RoyalBlue, 0.85);
            c.Line(12.0, 12.2, 16.6, 12.2, Slate500, 0.75);
            c.Line(12.0, 14.8, 16.6, 14.8, Slate500, 0.75);
            c.Line(12.0, 17.2, 15.0, 17.2, Slate400, 0.75);
            break;
        }
        case IconKind::Cut:
        {
            c.Circle(8.0, 16.5, 2.3, fg, false, COL_WHITE, 1.0);
            c.Circle(16.0, 16.5, 2.3, fg, false, COL_WHITE, 1.0);
            c.Line(9.5, 14.8, 16.2, 5.8, Slate700, 1.05);
            c.Line(14.5, 14.8, 7.8, 5.8, Slate700, 1.05);
            break;
        }
        case IconKind::Copy:
        {
            c.RoundRect(5.8, 5.0, 14.2, 15.2, 1.2, SkyBlueTile, true, Slate500, 0.8);
            c.RoundRect(9.8, 8.8, 18.2, 19.0, 1.2, COL_WHITE, true, fg, 0.95);
            c.Line(11.6, 12.0, 16.4, 12.0, Slate500, 0.75);
            c.Line(11.6, 14.6, 16.4, 14.6, Slate500, 0.75);
            break;
        }
        case IconKind::Paintbrush:
        {
            c.RoundRect(9.8, 4.8, 14.2, 10.5, 0.8, Amber, true, AmberDark, 0.7);
            c.RoundRect(6.2, 10.5, 17.8, 14.0, 0.8, Slate700, true, Slate900, 0.65);
            c.Poly({ { 6.5, 14.0 }, { 17.5, 14.0 }, { 16.2, 19.2 }, { 7.8, 19.2 } },
                   true, AmberDark, 0.75, true, AmberTile);
            break;
        }
        case IconKind::ClearFormat:
        case IconKind::Eraser:
        {
            if (eKind == IconKind::ClearFormat)
                c.Label(u"A"_ustr, 9.5, 10.0, 13.0, Slate700, WEIGHT_BOLD);
            c.Poly({ { 12.5, 16.2 }, { 17.2, 11.5 }, { 19.6, 13.9 }, { 14.9, 18.6 } },
                   true, dk, 0.75, true, fg);
            break;
        }
        case IconKind::ChangeCase:
        {
            c.Label(u"Aa"_ustr, 12.0, 12.0, 12.5, Slate900, WEIGHT_BOLD);
            break;
        }
        case IconKind::Bold:
            c.Label(u"B"_ustr, 12.0, 12.0, 15.5, Slate900, WEIGHT_ULTRABOLD);
            break;
        case IconKind::Italic:
            c.Label(u"I"_ustr, 12.0, 12.0, 15.5, Slate900, WEIGHT_BOLD, ITALIC_NORMAL);
            break;
        case IconKind::Underline:
        {
            c.Label(u"U"_ustr, 12.0, 10.5, 14.0, Slate900, WEIGHT_BOLD);
            c.Line(6.8, 18.2, 17.2, 18.2, fg, 1.35);
            break;
        }
        case IconKind::Strikeout:
        {
            c.Label(u"S"_ustr, 12.0, 12.0, 14.5, Slate700, WEIGHT_BOLD);
            c.Line(5.5, 12.2, 18.5, 12.2, fg, 1.2);
            break;
        }
        case IconKind::Shadowed:
        {
            c.Label(u"A"_ustr, 13.0, 12.8, 14.5, Slate300, WEIGHT_BOLD);
            c.Label(u"A"_ustr, 11.2, 11.2, 14.5, fg, WEIGHT_BOLD);
            break;
        }
        case IconKind::SubScript:
        {
            c.Label(u"x"_ustr, 9.5, 11.0, 14.0, Slate900, WEIGHT_BOLD);
            c.Label(u"2"_ustr, 16.8, 15.2, 8.8, fg, WEIGHT_BOLD);
            break;
        }
        case IconKind::SuperScript:
        {
            c.Label(u"x"_ustr, 9.5, 13.0, 14.0, Slate900, WEIGHT_BOLD);
            c.Label(u"2"_ustr, 16.8, 8.4, 8.8, fg, WEIGHT_BOLD);
            break;
        }
        case IconKind::GrowFont:
        {
            c.Label(u"A"_ustr, 10.0, 12.8, 14.5, Slate900, WEIGHT_BOLD);
            c.Poly({ { 15.2, 10.2 }, { 17.5, 7.2 }, { 19.8, 10.2 } }, true, fg, 0.5, true, fg);
            break;
        }
        case IconKind::ShrinkFont:
        {
            c.Label(u"A"_ustr, 10.0, 12.8, 13.0, Slate700, WEIGHT_BOLD);
            c.Poly({ { 15.2, 9.5 }, { 17.5, 12.5 }, { 19.8, 9.5 } }, true, fg, 0.5, true, fg);
            break;
        }
        case IconKind::FontColor:
        {
            c.Label(u"A"_ustr, 12.0, 9.5, 13.5, Slate900, WEIGHT_BOLD);
            c.RoundRect(5.2, 16.5, 18.8, 19.4, 0.6, fg, true, dk, 0.0);
            break;
        }
        case IconKind::HighlightColor:
        {
            c.Poly({ { 8.5, 14.8 }, { 14.8, 6.2 }, { 18.0, 8.6 }, { 11.7, 17.2 } },
                   true, Slate700, 0.75, true, fg);
            c.RoundRect(5.2, 16.8, 18.8, 19.5, 0.6, Color(0xFE, 0xF0, 0x8A), true, Amber, 0.6);
            break;
        }
        case IconKind::Spacing:
        case IconKind::FontDialog:
        {
            c.Label(u"AV"_ustr, 12.0, 9.5, 11.0, fg, WEIGHT_BOLD);
            c.Line(6.0, 16.5, 18.0, 16.5, Slate700, 0.95);
            c.Poly({ { 8.0, 14.5 }, { 5.8, 16.5 }, { 8.0, 18.5 } }, false, Slate700, 0.9);
            c.Poly({ { 16.0, 14.5 }, { 18.2, 16.5 }, { 16.0, 18.5 } }, false, Slate700, 0.9);
            break;
        }
        case IconKind::Bullets:
        {
            for (int i = 0; i < 3; ++i)
            {
                double yy = 7.2 + i * 4.8;
                c.Circle(6.5, yy, 1.35, fg, true, fg, 0.0);
                c.Line(10.0, yy, 18.2, yy, Slate700, 1.05);
            }
            break;
        }
        case IconKind::Numbering:
        case IconKind::Outline:
        {
            c.Label(u"1"_ustr, 6.6, 7.2, 6.5, fg, WEIGHT_BOLD);
            c.Label(u"2"_ustr, 6.6, 12.2, 6.5, fg, WEIGHT_BOLD);
            c.Label(u"3"_ustr, 6.6, 17.2, 6.5, fg, WEIGHT_BOLD);
            c.Line(10.2, 7.2, 18.2, 7.2, Slate700, 1.0);
            c.Line(10.2, 12.2, 18.2, 12.2, Slate700, 1.0);
            c.Line(10.2, 17.2, 18.2, 17.2, Slate700, 1.0);
            break;
        }
        case IconKind::AlignLeft:
        case IconKind::AlignCenter:
        case IconKind::AlignRight:
        case IconKind::AlignJustify:
        {
            Color lc = (eKind == IconKind::AlignCenter) ? RoyalBlue : Slate700;
            for (int i = 0; i < 4; ++i)
            {
                double yy = 6.5 + i * 3.6;
                double len = (eKind == IconKind::AlignJustify) ? 13.0 : (i % 2 == 0 ? 13.0 : 8.5);
                double x1 = 5.5;
                if (eKind == IconKind::AlignCenter)
                    x1 = 12.0 - len * 0.5;
                else if (eKind == IconKind::AlignRight)
                    x1 = 18.5 - len;
                c.Line(x1, yy, x1 + len, yy, lc, 1.05);
            }
            break;
        }
        case IconKind::IndentInc:
        case IconKind::IndentDec:
        case IconKind::HangingIndent:
        {
            for (int i = 0; i < 3; ++i)
            {
                double yy = 7.0 + i * 4.8;
                c.Line(10.5, yy, 18.5, yy, Slate700, 1.0);
            }
            if (eKind == IconKind::IndentDec)
                c.Poly({ { 8.8, 9.2 }, { 5.5, 12.0 }, { 8.8, 14.8 } }, true, fg, 0.5, true, fg);
            else
                c.Poly({ { 5.5, 9.2 }, { 8.8, 12.0 }, { 5.5, 14.8 } }, true, fg, 0.5, true, fg);
            break;
        }
        case IconKind::SpaceInc:
        case IconKind::SpaceDec:
        case IconKind::LineSpacing:
        {
            c.Line(11.0, 7.0, 18.5, 7.0, Slate700, 1.0);
            c.Line(11.0, 12.0, 18.5, 12.0, Slate700, 1.0);
            c.Line(11.0, 17.0, 18.5, 17.0, Slate700, 1.0);
            c.Line(7.0, 7.0, 7.0, 17.0, fg, 1.0);
            c.Poly({ { 5.2, 8.8 }, { 7.0, 6.5 }, { 8.8, 8.8 } }, false, fg, 0.9);
            c.Poly({ { 5.2, 15.2 }, { 7.0, 17.5 }, { 8.8, 15.2 } }, false, fg, 0.9);
            break;
        }
        case IconKind::Shading:
        {
            c.Poly({ { 7.2, 11.2 }, { 12.0, 6.4 }, { 16.8, 11.2 }, { 12.0, 16.0 } },
                   true, Slate700, 0.85, true, RoyalBlueTile);
            c.Circle(17.5, 14.8, 1.5, AmberDark, true, fg, 0.0);
            c.RoundRect(5.2, 17.4, 18.8, 19.6, 0.6, fg, true, dk, 0.0);
            break;
        }
        case IconKind::Pilcrow:
        {
            c.Label(u"\x00B6"_ustr, 12.0, 12.0, 15.0, fg, WEIGHT_BOLD);
            break;
        }
        case IconKind::Ltr:
        case IconKind::Rtl:
        {
            c.Label(u"\x00B6"_ustr, 12.0, 8.5, 10.5, Slate700, WEIGHT_BOLD);
            if (eKind == IconKind::Rtl)
            {
                c.Line(6.2, 16.5, 17.8, 16.5, fg, 1.05);
                c.Poly({ { 9.0, 14.2 }, { 6.0, 16.5 }, { 9.0, 18.8 } }, false, fg, 1.0);
            }
            else
            {
                c.Line(6.2, 16.5, 17.8, 16.5, fg, 1.05);
                c.Poly({ { 15.0, 14.2 }, { 18.0, 16.5 }, { 15.0, 18.8 } }, false, fg, 1.0);
            }
            break;
        }
        case IconKind::Borders:
        case IconKind::PageBorder:
        {
            c.RoundRect(5.8, 5.8, 18.2, 18.2, 1.0, COL_WHITE, true, fg, 0.95);
            c.Line(12.0, 5.8, 12.0, 18.2, Slate400, 0.75);
            c.Line(5.8, 12.0, 18.2, 12.0, Slate400, 0.75);
            break;
        }
        case IconKind::Find:
        case IconKind::CommandSearch:
        case IconKind::ZoomDialog:
        {
            c.Circle(10.5, 10.5, 4.8, fg, true, COL_WHITE, 1.15);
            c.Line(14.0, 14.0, 18.6, 18.6, dk, 1.5);
            break;
        }
        case IconKind::Replace:
        {
            c.Label(u"ab"_ustr, 8.6, 8.2, 8.5, Slate700, WEIGHT_BOLD);
            c.Label(u"ac"_ustr, 15.2, 15.8, 8.5, fg, WEIGHT_BOLD);
            c.Poly({ { 14.0, 7.6 }, { 17.4, 7.6 }, { 17.4, 11.4 } }, false, fg, 1.0);
            break;
        }
        case IconKind::SelectPointer:
        {
            c.Poly({ { 7.5, 5.2 }, { 17.4, 12.8 }, { 12.6, 13.6 }, { 14.8, 18.0 }, { 12.4, 19.0 }, { 10.2, 14.6 }, { 7.5, 17.2 } },
                   true, Slate900, 0.85, true, COL_WHITE);
            break;
        }
        case IconKind::SelectAll:
        {
            c.RoundRect(5.6, 5.6, 18.4, 18.4, 1.2, RoyalBlueTile, true, fg, 0.95);
            c.Line(8.5, 9.5, 15.5, 9.5, dk, 0.85);
            c.Line(8.5, 12.2, 15.5, 12.2, dk, 0.85);
            c.Line(8.5, 14.9, 13.5, 14.9, dk, 0.85);
            break;
        }
        case IconKind::GotoPage:
        {
            DrawPageSheet(c, Slate500, false);
            c.Line(5.5, 13.0, 14.5, 13.0, fg, 1.15);
            c.Poly({ { 11.8, 10.2 }, { 14.8, 13.0 }, { 11.8, 15.8 } }, false, fg, 1.15);
            break;
        }
        case IconKind::Navigator:
        {
            c.Circle(12.0, 12.0, 6.6, fg, true, COL_WHITE, 1.0);
            c.Poly({ { 9.0, 15.0 }, { 10.8, 10.8 }, { 15.0, 9.0 }, { 13.2, 13.2 } },
                   true, dk, 0.7, true, fg);
            break;
        }
        case IconKind::Sidebar:
        {
            c.RoundRect(5.2, 5.8, 18.8, 18.2, 1.4, COL_WHITE, true, Slate500, 0.85);
            c.RoundRect(13.8, 6.2, 18.5, 17.8, 0.6, RoyalBlueTile, true, fg, 0.75);
            break;
        }
        case IconKind::PageBreak:
        {
            c.Poly({ { 6.5, 4.8 }, { 17.5, 4.8 }, { 17.5, 9.8 }, { 6.5, 9.8 } }, true, Slate500, 0.85, true, COL_WHITE);
            c.Poly({ { 6.5, 14.2 }, { 17.5, 14.2 }, { 17.5, 19.2 }, { 6.5, 19.2 } }, true, Slate500, 0.85, true, COL_WHITE);
            c.Line(5.2, 12.0, 8.2, 12.0, fg, 1.1);
            c.Line(10.5, 12.0, 13.5, 12.0, fg, 1.1);
            c.Line(15.8, 12.0, 18.8, 12.0, fg, 1.1);
            break;
        }
        case IconKind::TitlePage:
        case IconKind::PageSetup:
        case IconKind::PageSize:
        case IconKind::PrintLayout:
        {
            DrawPageSheet(c, fg, true);
            break;
        }
        case IconKind::Section:
        {
            c.RoundRect(6.0, 5.2, 18.0, 11.2, 1.0, TealTile, true, fg, 0.85);
            c.RoundRect(6.0, 12.8, 18.0, 18.8, 1.0, COL_WHITE, true, Slate500, 0.85);
            break;
        }
        case IconKind::Table:
        {
            c.RoundRect(5.2, 5.6, 18.8, 18.4, 1.4, COL_WHITE, true, fg, 0.9);
            c.RoundRect(5.4, 5.8, 18.6, 9.2, 0.8, fg, true, fg, 0.0);
            c.Line(9.8, 9.2, 9.8, 18.4, Emerald, 0.75);
            c.Line(14.2, 9.2, 14.2, 18.4, Emerald, 0.75);
            c.Line(5.4, 13.8, 18.6, 13.8, Emerald, 0.75);
            break;
        }
        case IconKind::Picture:
        case IconKind::Gallery:
        {
            c.RoundRect(5.0, 5.8, 19.0, 18.2, 1.6, COL_WHITE, true, fg, 0.95);
            c.Poly({ { 6.2, 16.8 }, { 10.2, 11.4 }, { 13.2, 14.8 }, { 15.6, 12.2 }, { 17.8, 16.8 } },
                   true, dk, 0.7, true, fg);
            c.Circle(15.2, 9.2, 1.5, Amber, true, Amber, 0.0);
            break;
        }
        case IconKind::Media:
        {
            c.RoundRect(5.2, 5.8, 18.8, 18.2, 1.8, PurpleTile, true, fg, 0.95);
            c.Poly({ { 10.0, 9.0 }, { 16.0, 12.0 }, { 10.0, 15.0 } }, true, dk, 0.7, true, fg);
            break;
        }
        case IconKind::Chart:
        {
            c.RoundRect(6.0, 13.0, 8.6, 18.2, 0.6, RoyalBlue, true, RoyalBlueDark, 0.5);
            c.RoundRect(10.7, 8.5, 13.3, 18.2, 0.6, Emerald, true, EmeraldDark, 0.5);
            c.RoundRect(15.4, 5.8, 18.0, 18.2, 0.6, Orange, true, OrangeDark, 0.5);
            c.Line(5.0, 18.6, 19.0, 18.6, Slate600, 0.85);
            break;
        }
        case IconKind::ObjectOle:
        {
            c.RoundRect(6.2, 6.2, 17.8, 17.8, 1.6, TealTile, true, fg, 0.95);
            c.Line(6.2, 10.5, 17.8, 10.5, fg, 0.75);
            c.Line(12.0, 10.5, 12.0, 17.8, fg, 0.75);
            break;
        }
        case IconKind::FormulaMath:
        case IconKind::Sum:
        {
            c.Poly({ { 16.8, 6.4 }, { 7.6, 6.4 }, { 12.8, 12.0 }, { 7.6, 17.6 }, { 16.8, 17.6 } },
                   false, fg, 1.25);
            break;
        }
        case IconKind::Hyperlink:
        {
            c.RoundRect(5.2, 9.5, 13.0, 14.5, 2.5, SkyBlueTile, false, fg, 1.1);
            c.RoundRect(11.0, 9.5, 18.8, 14.5, 2.5, SkyBlueTile, false, RoyalBlue, 1.1);
            break;
        }
        case IconKind::Bookmark:
        {
            c.Poly({ { 7.5, 5.4 }, { 16.5, 5.4 }, { 16.5, 18.8 }, { 12.0, 15.2 }, { 7.5, 18.8 } },
                   true, dk, 0.85, true, fg);
            break;
        }
        case IconKind::CrossRef:
        case IconKind::Glossary:
        {
            DrawPageSheet(c, Slate500, false);
            c.Line(8.0, 12.0, 16.5, 12.0, fg, 1.0);
            c.Poly({ { 14.0, 9.5 }, { 16.8, 12.0 }, { 14.0, 14.5 } }, false, fg, 1.0);
            break;
        }
        case IconKind::Comment:
        case IconKind::ReplyComment:
        {
            c.Poly({ { 5.4, 6.0 }, { 18.6, 6.0 }, { 18.6, 15.2 }, { 10.5, 15.2 }, { 7.2, 18.5 }, { 7.2, 15.2 }, { 5.4, 15.2 } },
                   true, dk, 0.85, true, AmberTile);
            c.Line(8.2, 9.6, 15.8, 9.6, fg, 0.9);
            c.Line(8.2, 12.2, 13.5, 12.2, fg, 0.9);
            break;
        }
        case IconKind::DeleteComment:
        {
            c.Poly({ { 5.4, 6.0 }, { 18.6, 6.0 }, { 18.6, 15.2 }, { 10.5, 15.2 }, { 7.2, 18.5 }, { 7.2, 15.2 }, { 5.4, 15.2 } },
                   true, Slate400, 0.8, true, COL_WHITE);
            c.Line(10.0, 8.8, 14.4, 13.2, Red, 1.05);
            c.Line(14.4, 8.8, 10.0, 13.2, Red, 1.05);
            break;
        }
        case IconKind::Header:
        {
            DrawPageSheet(c, Slate500, false);
            c.RoundRect(7.5, 6.0, 13.8, 9.2, 0.5, fg, true, fg, 0.0);
            break;
        }
        case IconKind::Footer:
        {
            DrawPageSheet(c, Slate500, false);
            c.RoundRect(7.5, 14.8, 16.5, 18.0, 0.5, fg, true, fg, 0.0);
            break;
        }
        case IconKind::PageNumber:
        case IconKind::NumberFormat:
        {
            DrawPageSheet(c, Slate500, false);
            c.Label(u"#"_ustr, 12.0, 13.2, 10.0, fg, WEIGHT_BOLD);
            break;
        }
        case IconKind::DateField:
        {
            c.RoundRect(5.5, 6.2, 18.5, 18.2, 1.4, COL_WHITE, true, fg, 0.9);
            c.RoundRect(5.5, 6.2, 18.5, 10.0, 0.8, fg, true, fg, 0.0);
            c.Label(u"15"_ustr, 12.0, 14.2, 7.6, Slate900, WEIGHT_BOLD);
            break;
        }
        case IconKind::Field:
        {
            c.RoundRect(5.4, 7.5, 18.6, 16.5, 1.4, PurpleTile, true, fg, 0.9);
            c.Label(u"{}"_ustr, 12.0, 12.0, 9.5, dk, WEIGHT_BOLD);
            break;
        }
        case IconKind::TextBox:
        case IconKind::VerticalText:
        case IconKind::Frame:
        {
            c.RoundRect(5.4, 5.8, 18.6, 18.2, 1.2, COL_WHITE, true, fg, 0.95);
            c.Label(u"A"_ustr, 10.0, 12.0, 11.0, dk, WEIGHT_BOLD);
            c.Line(13.5, 9.8, 16.8, 9.8, Slate500, 0.8);
            c.Line(13.5, 12.4, 16.8, 12.4, Slate500, 0.8);
            c.Line(7.5, 15.2, 16.8, 15.2, Slate500, 0.8);
            break;
        }
        case IconKind::Fontwork:
        {
            c.Label(u"A"_ustr, 12.0, 12.0, 14.5, fg, WEIGHT_ULTRABOLD, ITALIC_NORMAL);
            break;
        }
        case IconKind::Symbol:
        {
            c.Label(u"\x03A9"_ustr, 12.0, 12.0, 14.0, fg, WEIGHT_BOLD);
            break;
        }
        case IconKind::InsertFile:
        {
            DrawPageSheet(c, fg, true);
            break;
        }
        case IconKind::SelectArrow:
        {
            c.Poly({ { 7.0, 5.5 }, { 17.5, 13.0 }, { 12.5, 13.8 }, { 14.8, 18.2 }, { 12.4, 19.2 }, { 10.2, 14.8 }, { 7.0, 17.5 } },
                   true, Slate900, 0.8, true, COL_WHITE);
            break;
        }
        case IconKind::Freehand:
        {
            c.Poly({ { 5.8, 16.2 }, { 9.2, 8.5 }, { 13.8, 15.5 }, { 18.2, 7.8 } }, false, fg, 1.25);
            break;
        }
        case IconKind::Line:
            c.Line(5.8, 18.2, 18.2, 5.8, fg, 1.25);
            break;
        case IconKind::ArrowLine:
        {
            c.Line(5.8, 18.2, 17.5, 6.5, fg, 1.2);
            c.Poly({ { 12.8, 6.2 }, { 18.0, 6.0 }, { 17.8, 11.2 } }, false, fg, 1.15);
            break;
        }
        case IconKind::Rect:
            c.RoundRect(5.8, 6.8, 18.2, 17.2, 1.0, RoyalBlueTile, true, fg, 1.05);
            break;
        case IconKind::Ellipse:
            c.Circle(12.0, 12.0, 6.2, fg, true, EmeraldTile, 1.05);
            break;
        case IconKind::BasicShapes:
        {
            c.Circle(9.0, 9.2, 3.6, fg, true, PurpleTile, 0.9);
            c.RoundRect(11.5, 11.5, 18.5, 18.5, 0.8, OrangeTile, true, Orange, 0.9);
            break;
        }
        case IconKind::ArrowShapes:
        {
            c.Poly({ { 5.5, 10.2 }, { 13.2, 10.2 }, { 13.2, 7.0 }, { 18.8, 12.0 }, { 13.2, 17.0 }, { 13.2, 13.8 }, { 5.5, 13.8 } },
                   true, dk, 0.85, true, OrangeTile);
            break;
        }
        case IconKind::FlowchartShapes:
        {
            c.RoundRect(8.5, 5.4, 15.5, 9.2, 0.8, TealTile, true, fg, 0.85);
            c.Line(12.0, 9.2, 12.0, 12.2, fg, 0.85);
            c.Line(7.5, 12.2, 16.5, 12.2, fg, 0.85);
            c.RoundRect(5.2, 14.5, 10.8, 18.5, 0.8, COL_WHITE, true, fg, 0.85);
            c.RoundRect(13.2, 14.5, 18.8, 18.5, 0.8, COL_WHITE, true, fg, 0.85);
            break;
        }
        case IconKind::CalloutShapes:
        {
            c.Poly({ { 5.4, 6.2 }, { 18.6, 6.2 }, { 18.6, 15.0 }, { 11.5, 15.0 }, { 8.0, 18.4 }, { 8.0, 15.0 }, { 5.4, 15.0 } },
                   true, fg, 0.95, true, SkyBlueTile);
            break;
        }
        case IconKind::StarShapes:
        {
            c.Poly({ { 12.0, 5.2 }, { 14.1, 9.6 }, { 18.8, 10.2 }, { 15.3, 13.5 },
                     { 16.2, 18.2 }, { 12.0, 15.8 }, { 7.8, 18.2 }, { 8.7, 13.5 },
                     { 5.2, 10.2 }, { 9.9, 9.6 } },
                   true, AmberDark, 0.85, true, AmberTile);
            break;
        }
        case IconKind::SymbolShapes:
        {
            c.Circle(12.0, 12.0, 6.4, fg, true, AmberTile, 0.95);
            c.Circle(9.8, 10.5, 0.8, dk, true, dk, 0.0);
            c.Circle(14.2, 10.5, 0.8, dk, true, dk, 0.0);
            c.Poly({ { 9.2, 13.8 }, { 12.0, 15.2 }, { 14.8, 13.8 } }, false, dk, 0.9);
            break;
        }
        case IconKind::ShapeFill:
        {
            c.RoundRect(6.0, 6.0, 18.0, 18.0, 2.0, fg, true, dk, 0.85);
            break;
        }
        case IconKind::ShapeLine:
        {
            c.Line(5.8, 16.5, 18.2, 7.5, fg, 1.3);
            c.Circle(6.2, 16.2, 1.5, dk, true, COL_WHITE, 0.7);
            c.Circle(17.8, 7.8, 1.5, dk, true, COL_WHITE, 0.7);
            break;
        }
        case IconKind::EditPoints:
        {
            c.Poly({ { 6.2, 16.8 }, { 12.0, 7.5 }, { 17.8, 16.8 } }, false, fg, 1.0);
            c.RoundRect(10.4, 5.8, 13.6, 9.0, 0.5, COL_WHITE, true, dk, 0.9);
            c.RoundRect(4.8, 15.2, 8.0, 18.4, 0.5, COL_WHITE, true, dk, 0.9);
            c.RoundRect(16.0, 15.2, 19.2, 18.4, 0.5, COL_WHITE, true, dk, 0.9);
            break;
        }
        case IconKind::Themes:
        case IconKind::StylesPane:
        {
            c.RoundRect(5.2, 5.8, 18.8, 18.2, 1.6, COL_WHITE, true, Slate500, 0.85);
            c.RoundRect(6.4, 7.0, 9.8, 17.0, 0.6, RoyalBlue, true, RoyalBlue, 0.0);
            c.RoundRect(10.6, 7.0, 14.0, 17.0, 0.6, Purple, true, Purple, 0.0);
            c.RoundRect(14.8, 7.0, 17.6, 17.0, 0.6, Emerald, true, Emerald, 0.0);
            break;
        }
        case IconKind::Watermark:
        {
            DrawPageSheet(c, Slate500, false);
            c.Line(7.8, 15.5, 16.2, 8.5, fg, 1.25);
            break;
        }
        case IconKind::Margins:
        {
            DrawPageSheet(c, Slate600, false);
            c.Line(9.0, 5.0, 9.0, 19.0, fg, 0.8);
            c.Line(15.0, 5.0, 15.0, 19.0, fg, 0.8);
            c.Line(6.8, 8.2, 17.2, 8.2, fg, 0.8);
            c.Line(6.8, 15.8, 17.2, 15.8, fg, 0.8);
            break;
        }
        case IconKind::Orientation:
        {
            c.RoundRect(6.2, 5.2, 14.0, 16.8, 1.0, OrangeTile, true, fg, 0.85);
            c.RoundRect(9.2, 10.2, 18.6, 18.2, 1.0, COL_WHITE, true, RoyalBlue, 0.9);
            break;
        }
        case IconKind::Columns:
        {
            c.RoundRect(5.6, 5.6, 18.4, 18.4, 1.2, COL_WHITE, true, Slate500, 0.85);
            for (int i = 0; i < 3; ++i)
            {
                double yy = 8.4 + i * 3.6;
                c.Line(7.4, yy, 11.0, yy, fg, 0.95);
                c.Line(13.0, yy, 16.6, yy, fg, 0.95);
            }
            break;
        }
        case IconKind::Hyphenation:
            c.Label(u"a-b"_ustr, 12.0, 12.0, 9.5, fg, WEIGHT_BOLD);
            break;
        case IconKind::LineNumbers:
        {
            c.Label(u"1"_ustr, 7.2, 8.5, 6.8, fg, WEIGHT_BOLD);
            c.Label(u"2"_ustr, 7.2, 15.2, 6.8, fg, WEIGHT_BOLD);
            c.Line(10.8, 8.5, 17.8, 8.5, Slate600, 0.95);
            c.Line(10.8, 15.2, 17.8, 15.2, Slate600, 0.95);
            break;
        }
        case IconKind::Wrap:
        {
            c.RoundRect(8.4, 8.4, 15.6, 15.6, 1.0, RoyalBlueTile, true, fg, 0.85);
            c.Line(5.5, 6.0, 18.5, 6.0, Slate600, 0.85);
            c.Line(5.5, 18.0, 18.5, 18.0, Slate600, 0.85);
            break;
        }
        case IconKind::BringFront:
        case IconKind::ForwardOne:
        {
            c.RoundRect(5.4, 9.4, 14.4, 18.4, 1.2, COL_WHITE, true, Slate500, 0.85);
            c.RoundRect(9.4, 5.4, 18.4, 14.4, 1.2, OrangeTile, true, fg, 0.95);
            break;
        }
        case IconKind::SendBack:
        case IconKind::BackOne:
        {
            c.RoundRect(9.4, 5.4, 18.4, 14.4, 1.2, RoyalBlueTile, true, RoyalBlue, 0.85);
            c.RoundRect(5.4, 9.4, 14.4, 18.4, 1.2, COL_WHITE, true, Slate600, 0.95);
            break;
        }
        case IconKind::Group:
        {
            c.RoundRect(6.2, 6.2, 13.8, 13.8, 1.0, PurpleTile, true, fg, 0.85);
            c.RoundRect(10.2, 10.2, 17.8, 17.8, 1.0, OrangeTile, true, Orange, 0.85);
            break;
        }
        case IconKind::Toc:
        case IconKind::Bibliography:
        {
            DrawPageSheet(c, fg, false);
            c.Line(8.2, 9.0, 15.8, 9.0, fg, 0.9);
            c.Line(9.5, 12.2, 15.8, 12.2, Slate500, 0.85);
            c.Line(9.5, 15.4, 15.8, 15.4, Slate500, 0.85);
            break;
        }
        case IconKind::IndexEntry:
        case IconKind::Footnote:
        case IconKind::Endnote:
        case IconKind::Caption:
        {
            DrawPageSheet(c, Slate500, false);
            c.Label(u"1"_ustr, 12.0, 12.4, 9.5, fg, WEIGHT_BOLD);
            break;
        }
        case IconKind::UpdateIndex:
        case IconKind::LoadStyles:
        {
            c.Circle(12.0, 12.0, 5.8, fg, false, COL_WHITE, 1.15);
            c.Poly({ { 15.0, 6.5 }, { 18.0, 9.2 }, { 14.5, 10.5 } }, true, fg, 0.6, true, fg);
            break;
        }
        case IconKind::UpdateStyle:
        case IconKind::NewStyle:
        case IconKind::AutoFormat:
        {
            c.Label(u"A+"_ustr, 12.0, 12.0, 10.5, fg, WEIGHT_BOLD);
            break;
        }
        case IconKind::Labels:
        {
            c.RoundRect(5.2, 6.0, 11.2, 11.2, 1.0, OrangeTile, true, fg, 0.85);
            c.RoundRect(12.8, 6.0, 18.8, 11.2, 1.0, COL_WHITE, true, fg, 0.85);
            c.RoundRect(5.2, 12.8, 11.2, 18.0, 1.0, COL_WHITE, true, fg, 0.85);
            c.RoundRect(12.8, 12.8, 18.8, 18.0, 1.0, OrangeTile, true, fg, 0.85);
            break;
        }
        case IconKind::MailMerge:
        case IconKind::MergeSave:
        {
            c.RoundRect(5.0, 5.4, 14.5, 14.5, 1.2, RoyalBlueTile, true, fg, 0.85);
            c.RoundRect(8.5, 9.5, 19.0, 18.2, 1.2, COL_WHITE, true, RoyalBlueDark, 0.95);
            c.Poly({ { 9.2, 10.5 }, { 13.8, 14.2 }, { 18.4, 10.5 } }, false, fg, 0.9);
            break;
        }
        case IconKind::DataSource:
        {
            c.RoundRect(6.2, 5.8, 17.8, 18.2, 2.0, EmeraldTile, true, fg, 0.95);
            c.Line(6.2, 10.0, 17.8, 10.0, fg, 0.85);
            c.Line(6.2, 14.0, 17.8, 14.0, fg, 0.85);
            break;
        }
        case IconKind::FirstRec:
        case IconKind::PrevRec:
            c.Poly({ { 15.0, 7.5 }, { 8.5, 12.0 }, { 15.0, 16.5 } }, true, fg, 0.8, true, fg);
            break;
        case IconKind::NextRec:
        case IconKind::LastRec:
            c.Poly({ { 9.0, 7.5 }, { 15.5, 12.0 }, { 9.0, 16.5 } }, true, fg, 0.8, true, fg);
            break;
        case IconKind::Spelling:
        case IconKind::AcceptChange:
        case IconKind::AcceptAll:
        {
            if (eKind == IconKind::Spelling)
            {
                c.Label(u"ABC"_ustr, 12.0, 8.4, 7.5, Slate900, WEIGHT_BOLD);
            }
            c.Poly({ { 7.0, 14.5 }, { 10.6, 18.0 }, { 17.5, 9.8 } }, false, fg, 1.35);
            break;
        }
        case IconKind::Thesaurus:
        {
            c.RoundRect(6.2, 5.2, 17.8, 18.8, 1.4, PurpleTile, true, fg, 0.95);
            c.Line(9.5, 5.2, 9.5, 18.8, fg, 0.9);
            c.Label(u"A"_ustr, 13.8, 11.8, 8.5, dk, WEIGHT_BOLD);
            break;
        }
        case IconKind::WordCount:
        {
            c.RoundRect(5.2, 5.8, 18.8, 18.2, 1.4, SkyBlueTile, true, fg, 0.9);
            c.Label(u"123"_ustr, 12.0, 12.0, 7.8, dk, WEIGHT_BOLD);
            break;
        }
        case IconKind::TrackChanges:
        case IconKind::ShowChanges:
        case IconKind::Compare:
        {
            DrawPageSheet(c, Slate500, false);
            c.Line(8.2, 10.2, 15.5, 10.2, Red, 1.0);
            c.Line(8.2, 13.5, 15.5, 13.5, Emerald, 1.0);
            c.Line(8.2, 16.8, 13.0, 16.8, RoyalBlue, 1.0);
            break;
        }
        case IconKind::PrevChange:
            c.Poly({ { 14.5, 7.5 }, { 9.0, 12.0 }, { 14.5, 16.5 } }, false, fg, 1.3);
            break;
        case IconKind::NextChange:
            c.Poly({ { 9.5, 7.5 }, { 15.0, 12.0 }, { 9.5, 16.5 } }, false, fg, 1.3);
            break;
        case IconKind::Protect:
        {
            c.RoundRect(8.5, 5.5, 15.5, 12.0, 3.2, COL_WHITE, false, fg, 1.05);
            c.RoundRect(6.4, 10.5, 17.6, 18.6, 1.4, IndigoTile, true, fg, 0.95);
            break;
        }
        case IconKind::WebLayout:
        {
            c.Circle(12.0, 12.0, 6.6, fg, true, SkyBlueTile, 1.0);
            c.Line(5.8, 12.0, 18.2, 12.0, fg, 0.85);
            c.Line(12.0, 5.6, 12.0, 18.4, fg, 0.85);
            break;
        }
        case IconKind::Whitespace:
        {
            c.Line(6.5, 6.2, 17.5, 6.2, fg, 1.0);
            c.Line(6.5, 17.8, 17.5, 17.8, fg, 1.0);
            c.Line(12.0, 8.8, 12.0, 15.2, fg, 0.95);
            break;
        }
        case IconKind::Zoom100:
        {
            c.Circle(12.0, 12.0, 6.6, fg, true, COL_WHITE, 1.0);
            c.Label(u"1:1"_ustr, 12.0, 12.0, 7.4, dk, WEIGHT_BOLD);
            break;
        }
        case IconKind::ZoomWidth:
        {
            DrawPageSheet(c, Slate500, false);
            c.Line(5.2, 12.5, 18.8, 12.5, fg, 1.15);
            c.Poly({ { 7.4, 10.2 }, { 5.0, 12.5 }, { 7.4, 14.8 } }, false, fg, 1.05);
            c.Poly({ { 16.6, 10.2 }, { 19.0, 12.5 }, { 16.6, 14.8 } }, false, fg, 1.05);
            break;
        }
        case IconKind::FullScreen:
        {
            c.Poly({ { 6.0, 9.5 }, { 6.0, 6.0 }, { 9.5, 6.0 } }, false, fg, 1.15);
            c.Poly({ { 14.5, 6.0 }, { 18.0, 6.0 }, { 18.0, 9.5 } }, false, fg, 1.15);
            c.Poly({ { 6.0, 14.5 }, { 6.0, 18.0 }, { 9.5, 18.0 } }, false, fg, 1.15);
            c.Poly({ { 14.5, 18.0 }, { 18.0, 18.0 }, { 18.0, 14.5 } }, false, fg, 1.15);
            break;
        }
        case IconKind::Ruler:
        {
            c.RoundRect(5.2, 8.2, 18.8, 15.8, 1.0, AmberTile, true, fg, 0.95);
            c.Line(8.5, 8.2, 8.5, 11.4, dk, 0.85);
            c.Line(12.0, 8.2, 12.0, 12.4, dk, 0.85);
            c.Line(15.5, 8.2, 15.5, 11.4, dk, 0.85);
            break;
        }
        case IconKind::Grid:
        case IconKind::Guides:
        {
            c.RoundRect(5.4, 5.4, 18.6, 18.6, 1.0, COL_WHITE, true, fg, 0.85);
            c.Line(10.0, 5.6, 10.0, 18.4, fg, 0.7);
            c.Line(14.0, 5.6, 14.0, 18.4, fg, 0.7);
            c.Line(5.6, 10.0, 18.4, 10.0, fg, 0.7);
            c.Line(5.6, 14.0, 18.4, 14.0, fg, 0.7);
            break;
        }
        case IconKind::Help:
        {
            c.Circle(12.0, 12.0, 6.8, dk, true, fg, 0.7);
            c.Label(u"?"_ustr, 12.0, 12.0, 11.5, COL_WHITE, WEIGHT_BOLD);
            break;
        }
        case IconKind::Options:
        case IconKind::SafeMode:
        {
            c.Circle(12.0, 12.0, 5.5, fg, false, COL_WHITE, 1.4);
            c.Circle(12.0, 12.0, 2.2, dk, true, fg, 0.0);
            break;
        }
        case IconKind::Macro:
        {
            c.RoundRect(5.4, 5.8, 18.6, 18.2, 1.6, EmeraldTile, true, fg, 0.9);
            c.Poly({ { 9.8, 8.8 }, { 15.6, 12.0 }, { 9.8, 15.2 } }, true, dk, 0.7, true, fg);
            break;
        }
        case IconKind::RowsAbove:
        case IconKind::RowsBelow:
        case IconKind::ColsBefore:
        case IconKind::ColsAfter:
        case IconKind::DeleteRows:
        case IconKind::DeleteCols:
        case IconKind::DeleteTable:
        case IconKind::MergeCells:
        case IconKind::SplitCell:
        case IconKind::SplitTable:
        case IconKind::SelectTable:
        {
            c.RoundRect(5.2, 5.6, 18.8, 18.4, 1.2, COL_WHITE, true, Slate500, 0.85);
            c.Line(5.4, 10.0, 18.6, 10.0, Slate400, 0.7);
            c.Line(5.4, 14.2, 18.6, 14.2, Slate400, 0.7);
            c.Line(10.0, 5.8, 10.0, 18.2, Slate400, 0.7);
            c.Line(14.2, 5.8, 14.2, 18.2, Slate400, 0.7);
            if (eKind == IconKind::RowsAbove || eKind == IconKind::DeleteRows)
                c.RoundRect(5.4, 5.8, 18.6, 9.8, 0.5, fg, true, fg, 0.0);
            else if (eKind == IconKind::RowsBelow)
                c.RoundRect(5.4, 14.4, 18.6, 18.2, 0.5, fg, true, fg, 0.0);
            else if (eKind == IconKind::ColsBefore || eKind == IconKind::DeleteCols)
                c.RoundRect(5.4, 5.8, 9.8, 18.2, 0.5, fg, true, fg, 0.0);
            else if (eKind == IconKind::ColsAfter)
                c.RoundRect(14.4, 5.8, 18.6, 18.2, 0.5, fg, true, fg, 0.0);
            else
                c.RoundRect(5.4, 5.8, 18.6, 18.2, 0.8, fg, false, fg, 1.0);
            break;
        }
        case IconKind::VertTop:
        case IconKind::VertCenter:
        case IconKind::VertBottom:
        {
            c.RoundRect(6.0, 5.6, 18.0, 18.4, 1.0, COL_WHITE, true, Slate500, 0.85);
            double yy = (eKind == IconKind::VertTop) ? 8.5 : (eKind == IconKind::VertCenter ? 12.0 : 15.5);
            c.Line(8.2, yy, 15.8, yy, fg, 1.25);
            break;
        }
        case IconKind::Sort:
        {
            c.Line(8.5, 6.2, 8.5, 17.8, RoyalBlue, 1.15);
            c.Poly({ { 6.0, 14.8 }, { 8.5, 18.0 }, { 11.0, 14.8 } }, false, RoyalBlue, 1.05);
            c.Line(15.5, 6.2, 15.5, 17.8, Orange, 1.15);
            c.Poly({ { 13.0, 9.2 }, { 15.5, 6.0 }, { 18.0, 9.2 } }, false, Orange, 1.05);
            break;
        }
        case IconKind::CompressImg:
        case IconKind::Crop:
        {
            c.Poly({ { 8.0, 5.5 }, { 8.0, 16.0 }, { 18.5, 16.0 } }, false, fg, 1.15);
            c.Poly({ { 5.5, 8.5 }, { 15.5, 8.5 }, { 15.5, 18.5 } }, false, dk, 1.15);
            break;
        }
        case IconKind::RotateRight:
        {
            c.Circle(12.0, 12.0, 5.5, fg, false, COL_WHITE, 1.15);
            c.Poly({ { 14.5, 6.0 }, { 17.8, 9.0 }, { 14.0, 10.5 } }, true, fg, 0.5, true, fg);
            break;
        }
        case IconKind::FlipV:
        {
            c.Poly({ { 7.0, 6.5 }, { 17.0, 6.5 }, { 12.0, 10.2 } }, true, fg, 0.7, true, fg);
            c.Line(5.5, 12.0, 18.5, 12.0, Slate600, 0.85);
            c.Poly({ { 7.0, 17.5 }, { 17.0, 17.5 }, { 12.0, 13.8 } }, true, Slate500, 0.7, true, COL_WHITE);
            break;
        }
        case IconKind::FlipH:
        {
            c.Poly({ { 6.5, 7.0 }, { 6.5, 17.0 }, { 10.2, 12.0 } }, true, fg, 0.7, true, fg);
            c.Line(12.0, 5.5, 12.0, 18.5, Slate600, 0.85);
            c.Poly({ { 17.5, 7.0 }, { 17.5, 17.0 }, { 13.8, 12.0 } }, true, Slate500, 0.7, true, COL_WHITE);
            break;
        }
        case IconKind::Filters:
        {
            c.Circle(9.5, 10.0, 3.8, Purple, true, PurpleTile, 0.9);
            c.Circle(14.5, 10.0, 3.8, Orange, true, OrangeTile, 0.9);
            c.Circle(12.0, 14.5, 3.8, Teal, true, TealTile, 0.9);
            break;
        }
        case IconKind::WrapText:
        {
            c.Line(5.5, 7.2, 18.5, 7.2, Slate700, 1.0);
            c.Line(5.5, 11.6, 13.5, 11.6, Slate700, 1.0);
            c.Line(5.5, 16.0, 10.5, 16.0, Slate700, 1.0);
            c.Poly({ { 14.5, 11.6 }, { 17.8, 11.6 }, { 17.8, 16.8 }, { 13.2, 16.8 } }, false, fg, 1.05);
            c.Poly({ { 15.2, 14.8 }, { 12.8, 16.8 }, { 15.2, 18.8 } }, false, fg, 1.05);
            break;
        }
        case IconKind::Currency:
        {
            c.Label(u"$"_ustr, 12.0, 12.0, 15.5, fg, WEIGHT_BOLD);
            break;
        }
        case IconKind::Percent:
        {
            c.Label(u"%"_ustr, 12.0, 12.0, 14.0, fg, WEIGHT_BOLD);
            break;
        }
        case IconKind::CommaNumber:
        {
            c.Label(u"0,0"_ustr, 12.0, 12.0, 10.5, fg, WEIGHT_BOLD);
            break;
        }
        case IconKind::IncDecimal:
        {
            c.Label(u".00"_ustr, 12.0, 9.5, 9.2, Slate700, WEIGHT_BOLD);
            c.Line(6.5, 16.5, 17.5, 16.5, fg, 1.1);
            c.Poly({ { 14.8, 14.2 }, { 17.8, 16.5 }, { 14.8, 18.8 } }, false, fg, 1.1);
            break;
        }
        case IconKind::DecDecimal:
        {
            c.Label(u".0"_ustr, 12.0, 9.5, 9.8, Slate700, WEIGHT_BOLD);
            c.Line(6.5, 16.5, 17.5, 16.5, fg, 1.1);
            c.Poly({ { 9.2, 14.2 }, { 6.2, 16.5 }, { 9.2, 18.8 } }, false, fg, 1.1);
            break;
        }
        case IconKind::ConditionalFormat:
        {
            c.RoundRect(5.2, 5.4, 18.8, 18.6, 1.2, COL_WHITE, true, Slate500, 0.85);
            c.RoundRect(6.8, 7.0, 17.2, 9.5, 0.6, Emerald, true, Emerald, 0.0);
            c.RoundRect(6.8, 10.7, 14.2, 13.3, 0.6, Amber, true, Amber, 0.0);
            c.RoundRect(6.8, 14.5, 11.8, 17.0, 0.6, Rose, true, Rose, 0.0);
            break;
        }
        case IconKind::FormatTable:
        {
            c.RoundRect(5.2, 5.6, 18.8, 18.4, 1.2, COL_WHITE, true, fg, 0.9);
            c.RoundRect(5.4, 5.8, 18.6, 9.4, 0.6, fg, true, fg, 0.0);
            c.RoundRect(5.4, 12.6, 18.6, 15.2, 0.0, EmeraldTile, true, EmeraldTile, 0.0);
            c.Line(10.0, 9.4, 10.0, 18.2, Slate300, 0.7);
            c.Line(14.2, 9.4, 14.2, 18.2, Slate300, 0.7);
            break;
        }
        case IconKind::FormatCells:
        case IconKind::InsertCells:
        case IconKind::DeleteCells:
        {
            c.RoundRect(5.4, 5.6, 18.6, 18.4, 1.2, COL_WHITE, true, fg, 0.9);
            c.RoundRect(8.4, 9.0, 15.6, 15.0, 0.6, IndigoTile, true, fg, 0.85);
            c.Line(12.0, 5.8, 12.0, 18.2, Slate300, 0.7);
            c.Line(5.6, 12.0, 18.4, 12.0, Slate300, 0.7);
            break;
        }
        case IconKind::FillDown:
        {
            c.RoundRect(6.0, 5.4, 18.0, 9.4, 0.8, SkyBlueTile, true, fg, 0.85);
            c.RoundRect(6.0, 10.4, 18.0, 18.4, 0.8, COL_WHITE, true, Slate400, 0.8);
            c.Line(12.0, 10.5, 12.0, 17.2, fg, 1.15);
            c.Poly({ { 9.2, 14.4 }, { 12.0, 17.4 }, { 14.8, 14.4 } }, false, fg, 1.15);
            break;
        }
        case IconKind::SortAsc:
        {
            c.Label(u"A"_ustr, 8.2, 8.2, 8.5, RoyalBlue, WEIGHT_BOLD);
            c.Label(u"Z"_ustr, 8.2, 15.8, 8.5, Slate600, WEIGHT_BOLD);
            c.Line(15.6, 6.2, 15.6, 17.8, RoyalBlue, 1.15);
            c.Poly({ { 13.0, 14.8 }, { 15.6, 18.0 }, { 18.2, 14.8 } }, false, RoyalBlue, 1.1);
            break;
        }
        case IconKind::SortDesc:
        {
            c.Label(u"Z"_ustr, 8.2, 8.2, 8.5, Orange, WEIGHT_BOLD);
            c.Label(u"A"_ustr, 8.2, 15.8, 8.5, Slate600, WEIGHT_BOLD);
            c.Line(15.6, 6.2, 15.6, 17.8, Orange, 1.15);
            c.Poly({ { 13.0, 14.8 }, { 15.6, 18.0 }, { 18.2, 14.8 } }, false, Orange, 1.1);
            break;
        }
        case IconKind::Filter:
        case IconKind::SortFilter:
        {
            c.Poly({ { 5.4, 6.4 }, { 18.6, 6.4 }, { 13.6, 12.2 }, { 13.6, 17.8 }, { 10.4, 19.0 }, { 10.4, 12.2 } },
                   true, dk, 0.9, true, PurpleTile);
            break;
        }
        case IconKind::PivotTable:
        {
            c.RoundRect(5.2, 5.4, 18.8, 18.6, 1.2, COL_WHITE, true, Slate500, 0.85);
            c.RoundRect(5.6, 5.8, 18.4, 9.0, 0.5, TealTile, true, fg, 0.7);
            c.RoundRect(5.6, 9.6, 9.2, 18.2, 0.5, EmeraldTile, true, Emerald, 0.7);
            c.Poly({ { 16.0, 12.0 }, { 16.0, 16.0 }, { 11.8, 16.0 } }, false, fg, 1.15);
            c.Poly({ { 14.2, 13.6 }, { 16.0, 11.4 }, { 17.8, 13.6 } }, false, fg, 1.0);
            break;
        }
        case IconKind::Sparkline:
        {
            c.RoundRect(5.2, 6.2, 18.8, 17.8, 1.2, COL_WHITE, true, Slate400, 0.85);
            c.Poly({ { 6.6, 14.8 }, { 9.8, 9.8 }, { 12.8, 13.4 }, { 16.8, 8.6 } }, false, fg, 1.25);
            c.Circle(16.8, 8.6, 1.4, Red, true, Red, 0.0);
            break;
        }
        case IconKind::Clock:
        {
            c.Circle(12.0, 12.0, 6.6, fg, true, SkyBlueTile, 1.0);
            c.Poly({ { 12.0, 8.2 }, { 12.0, 12.2 }, { 15.2, 13.8 } }, false, dk, 1.05);
            break;
        }
        case IconKind::PrintArea:
        {
            c.RoundRect(5.2, 5.6, 18.8, 18.4, 1.2, COL_WHITE, true, Slate400, 0.85);
            c.Line(9.8, 5.8, 9.8, 18.2, Slate300, 0.7);
            c.Line(14.2, 5.8, 14.2, 18.2, Slate300, 0.7);
            c.Line(5.4, 10.0, 18.6, 10.0, Slate300, 0.7);
            c.Line(5.4, 14.2, 18.6, 14.2, Slate300, 0.7);
            c.RoundRect(6.2, 6.6, 14.2, 14.2, 0.8, RoyalBlueTile, false, fg, 1.15);
            break;
        }
        case IconKind::Headers:
        {
            c.RoundRect(5.4, 5.6, 18.6, 18.4, 1.2, COL_WHITE, true, Slate500, 0.85);
            c.RoundRect(5.6, 5.8, 18.4, 9.8, 0.5, EmeraldTile, true, fg, 0.75);
            c.RoundRect(5.6, 9.8, 9.6, 18.2, 0.5, EmeraldTile, true, fg, 0.75);
            break;
        }
        case IconKind::FunctionFx:
        {
            c.Label(u"fx"_ustr, 12.0, 12.0, 13.5, fg, WEIGHT_BOLD, ITALIC_NORMAL);
            break;
        }
        case IconKind::DefinedName:
        {
            c.Poly({ { 5.4, 8.2 }, { 14.2, 8.2 }, { 18.8, 12.0 }, { 14.2, 15.8 }, { 5.4, 15.8 } },
                   true, dk, 0.9, true, IndigoTile);
            c.Circle(14.2, 12.0, 1.2, fg, true, fg, 0.0);
            break;
        }
        case IconKind::TracePrecedents:
        {
            c.RoundRect(5.2, 5.6, 9.8, 9.8, 0.8, RoyalBlueTile, true, fg, 0.85);
            c.RoundRect(5.2, 14.2, 9.8, 18.4, 0.8, RoyalBlueTile, true, fg, 0.85);
            c.RoundRect(14.2, 9.6, 18.8, 14.4, 0.8, COL_WHITE, true, Slate600, 0.9);
            c.Line(9.8, 7.8, 14.2, 11.4, fg, 1.0);
            c.Line(9.8, 16.2, 14.2, 12.6, fg, 1.0);
            c.Circle(14.2, 12.0, 1.3, fg, true, fg, 0.0);
            break;
        }
        case IconKind::TraceDependents:
        {
            c.RoundRect(5.2, 9.6, 9.8, 14.4, 0.8, COL_WHITE, true, Slate600, 0.9);
            c.RoundRect(14.2, 5.6, 18.8, 9.8, 0.8, OrangeTile, true, fg, 0.85);
            c.RoundRect(14.2, 14.2, 18.8, 18.4, 0.8, OrangeTile, true, fg, 0.85);
            c.Line(9.8, 11.4, 14.2, 7.8, fg, 1.0);
            c.Line(9.8, 12.6, 14.2, 16.2, fg, 1.0);
            c.Circle(14.2, 7.8, 1.2, fg, true, fg, 0.0);
            c.Circle(14.2, 16.2, 1.2, fg, true, fg, 0.0);
            break;
        }
        case IconKind::ErrorCheck:
        {
            c.Poly({ { 12.0, 5.2 }, { 19.0, 18.2 }, { 5.0, 18.2 } }, true, dk, 0.9, true, RoseTile);
            c.Line(12.0, 9.8, 12.0, 13.8, dk, 1.2);
            c.Circle(12.0, 16.0, 0.9, dk, true, dk, 0.0);
            break;
        }
        case IconKind::Calculate:
        {
            c.RoundRect(6.2, 5.2, 17.8, 18.8, 1.4, COL_WHITE, true, fg, 0.95);
            c.RoundRect(8.0, 6.8, 16.0, 10.0, 0.6, EmeraldTile, true, fg, 0.7);
            c.Circle(9.2, 12.6, 1.0, Slate600, true, Slate600, 0.0);
            c.Circle(12.0, 12.6, 1.0, Slate600, true, Slate600, 0.0);
            c.Circle(14.8, 12.6, 1.0, fg, true, fg, 0.0);
            c.Circle(9.2, 15.8, 1.0, Slate600, true, Slate600, 0.0);
            c.Circle(12.0, 15.8, 1.0, Slate600, true, Slate600, 0.0);
            c.Circle(14.8, 15.8, 1.0, fg, true, fg, 0.0);
            break;
        }
        case IconKind::WhatIf:
        {
            c.Circle(12.0, 12.0, 6.8, fg, true, PurpleTile, 0.95);
            c.Circle(12.0, 12.0, 3.8, dk, true, COL_WHITE, 0.9);
            c.Circle(12.0, 12.0, 1.4, fg, true, fg, 0.0);
            break;
        }
        case IconKind::DataRefresh:
        {
            c.Circle(12.0, 12.0, 5.8, fg, false, COL_WHITE, 1.2);
            c.Poly({ { 15.2, 6.2 }, { 18.2, 9.2 }, { 14.5, 10.4 } }, true, fg, 0.5, true, fg);
            c.Poly({ { 8.8, 17.8 }, { 5.8, 14.8 }, { 9.5, 13.6 } }, true, dk, 0.5, true, dk);
            break;
        }
        case IconKind::TextToColumns:
        {
            c.RoundRect(5.2, 6.0, 10.8, 18.0, 0.9, RoyalBlueTile, true, fg, 0.85);
            c.RoundRect(13.2, 6.0, 18.8, 18.0, 0.9, EmeraldTile, true, Emerald, 0.85);
            c.Line(7.0, 9.4, 9.2, 9.4, dk, 0.8);
            c.Line(7.0, 12.2, 9.2, 12.2, dk, 0.8);
            c.Line(14.8, 9.4, 17.0, 9.4, EmeraldDark, 0.8);
            c.Line(14.8, 12.2, 17.0, 12.2, EmeraldDark, 0.8);
            break;
        }
        case IconKind::RemoveDuplicates:
        {
            c.RoundRect(5.2, 5.8, 17.2, 9.4, 0.7, EmeraldTile, true, Emerald, 0.8);
            c.RoundRect(5.2, 10.6, 17.2, 14.2, 0.7, RoseTile, true, fg, 0.8);
            c.Line(14.5, 14.5, 18.6, 18.6, Red, 1.25);
            c.Line(18.6, 14.5, 14.5, 18.6, Red, 1.25);
            break;
        }
        case IconKind::DataValidation:
        {
            c.RoundRect(5.4, 5.8, 14.0, 11.4, 1.0, EmeraldTile, true, Emerald, 0.85);
            c.Poly({ { 7.6, 8.6 }, { 9.2, 10.0 }, { 12.0, 7.2 } }, false, EmeraldDark, 1.0);
            c.Circle(15.6, 15.6, 3.4, Red, true, RoseTile, 0.9);
            c.Line(13.6, 13.6, 17.6, 17.6, Red, 0.95);
            break;
        }
        case IconKind::OutlineGroup:
        {
            c.RoundRect(5.4, 5.4, 11.4, 11.4, 1.0, COL_WHITE, true, fg, 0.95);
            c.Line(7.2, 8.4, 9.6, 8.4, dk, 1.0);
            c.Line(8.4, 7.2, 8.4, 9.6, dk, 1.0);
            c.Line(13.2, 8.4, 18.4, 8.4, Slate600, 0.95);
            c.Line(13.2, 12.6, 18.4, 12.6, Slate600, 0.95);
            c.Line(13.2, 16.8, 18.4, 16.8, Slate600, 0.95);
            break;
        }
        case IconKind::NormalView:
        {
            c.RoundRect(5.4, 5.6, 18.6, 18.4, 1.2, COL_WHITE, true, fg, 0.9);
            c.Line(10.0, 5.8, 10.0, 18.2, Emerald, 0.75);
            c.Line(14.2, 5.8, 14.2, 18.2, Emerald, 0.75);
            c.Line(5.6, 10.0, 18.4, 10.0, Emerald, 0.75);
            c.Line(5.6, 14.2, 18.4, 14.2, Emerald, 0.75);
            break;
        }
        case IconKind::FreezePanes:
        {
            c.RoundRect(5.4, 5.6, 18.6, 18.4, 1.2, COL_WHITE, true, Slate500, 0.85);
            c.RoundRect(5.6, 5.8, 18.4, 9.8, 0.5, SkyBlueTile, true, fg, 0.8);
            c.RoundRect(5.6, 5.8, 9.8, 18.2, 0.5, SkyBlueTile, true, fg, 0.8);
            c.Line(5.6, 10.0, 18.4, 10.0, fg, 1.15);
            c.Line(10.0, 5.8, 10.0, 18.2, fg, 1.15);
            break;
        }
        case IconKind::FormulaEqual:
        {
            c.Line(6.4, 9.5, 17.6, 9.5, fg, 1.45);
            c.Line(6.4, 14.5, 17.6, 14.5, fg, 1.45);
            break;
        }
        case IconKind::FormulaOk:
        {
            c.Poly({ { 6.2, 12.6 }, { 10.2, 16.8 }, { 17.8, 7.6 } }, false, fg, 1.5);
            break;
        }
        case IconKind::FormulaCancel:
        {
            c.Line(6.8, 6.8, 17.2, 17.2, fg, 1.45);
            c.Line(17.2, 6.8, 6.8, 17.2, fg, 1.45);
            break;
        }
        case IconKind::None:
            break;
    }
}

bool RenderStyledIcon(IconKind eKind, PaletteId ePalette, tools::Long nBaseSize,
                      sal_Int32 nScalePercentage, bool bTiled, Bitmap& rBitmap)
{
    if (eKind == IconKind::None)
        return false;

    const sal_Int32 nPct = std::max<sal_Int32>(100, nScalePercentage);
    const tools::Long nSz = std::max<tools::Long>(16, std::lround(nBaseSize * nPct / 100.0));
    const PalettePair aPal = GetPalette(ePalette);

    ScopedVclPtrInstance<VirtualDevice> pVDev(DeviceFormat::WITH_ALPHA);
    pVDev->SetOutputSizePixel(Size(nSz, nSz), true, true);
    pVDev->SetAntialiasing(AntialiasingFlags::Enable | AntialiasingFlags::PixelSnapHairline);

    Canvas aCanvas(*pVDev, nSz, bTiled);
    if (bTiled)
    {
        Color aBorder(aPal.tileBg);
        aBorder.Merge(aPal.fg, 200);
        aCanvas.FillTile(aPal.tileBg, aBorder);
    }

    DrawIconArtwork(aCanvas, eKind, aPal);
    rBitmap = pVDev->GetBitmap(Point(0, 0), Size(nSz, nSz));
    return !rBitmap.IsEmpty();
}

PalettePair HarmonizeOffice2021Palette(PaletteId eId)
{
    switch (eId)
    {
        case PaletteId::RoyalBlue:
        case PaletteId::SkyBlue:
        case PaletteId::Indigo:
            return { O21Blue, O21BlueSoft, DocsAccentDark };
        case PaletteId::Emerald:
        case PaletteId::Teal:
            return { O21Green, O21GreenSoft, CalcAccentDark };
        case PaletteId::Orange:
        case PaletteId::Amber:
            return { O21Amber, O21AmberFill, AmberDark };
        case PaletteId::Red:
            return { O21Red, O21RoseSoft, RedDark };
        case PaletteId::Purple:
        case PaletteId::Rose:
            return { O21Magenta, O21MagentaSoft, RoseDark };
        case PaletteId::Slate:
            return { O21Ink, SurfaceSubtle, O21Ink };
    }
    return { O21Ink, SurfaceSubtle, O21Ink };
}

// Refined Office 2021 monoline grays, accents, and clear highlights for airy crisp icons
constexpr Color O21LineGray    { 0x61, 0x61, 0x61 };
constexpr Color O21GridLight   { 0xA6, 0xA6, 0xA6 };
constexpr Color O21FontInk     { 0x44, 0x44, 0x44 };
constexpr Color O21ClipBorder  { 0xD4, 0x9B, 0x48 };
constexpr Color O21ClipFill    { 0xFF, 0xF8, 0xEA };
constexpr Color O21CoralFill   { 0xF3, 0x7A, 0x78 };
constexpr Color O21CoralStroke { 0xD9, 0x46, 0x45 };
constexpr Color O21SkyFill     { 0x82, 0xB8, 0xE8 };
constexpr Color O21SkyStroke   { 0x2B, 0x7C, 0xD3 };
constexpr Color O21SkyGlow     { 0x98, 0xCA, 0xF4 };
constexpr Color O21EraserPink  { 0xE2, 0x92, 0xD0 };
constexpr Color O21EraserPale  { 0xF8, 0xE8, 0xF5 };
constexpr Color O21EraserLine  { 0x9B, 0x4E, 0x92 };

void DrawOffice2021Artwork(Canvas& c, IconKind eKind, PaletteId ePal, tools::Long nBaseSize)
{
    switch (eKind)
    {
        case IconKind::Paste:
        {
            // Airy Office 2021 thin-line warm clipboard + delicate white clip + clean white page
            c.RoundRect(4.4, 5.2, 15.8, 19.0, 0.7, O21ClipFill, true, O21ClipBorder, 1.0);
            c.RoundRect(8.9, 3.0, 11.3, 5.2, 0.9, COL_WHITE, true, O21LineGray, 0.9);
            c.RoundRect(7.1, 4.4, 13.1, 6.7, 0.5, COL_WHITE, true, O21LineGray, 0.9);
            c.RoundRect(10.6, 9.0, 19.6, 20.2, 0.4, COL_WHITE, true, O21LineGray, 0.95);
            c.Line(10.7, 9.1, 19.5, 9.1, O21FontInk, 1.0);
            break;
        }
        case IconKind::Cut:
        {
            c.Line(9.5, 14.8, 16.5, 5.2, O21Ink, 1.05);
            c.Line(14.5, 14.8, 7.5, 5.2, O21Ink, 1.05);
            c.Circle(7.8, 16.8, 2.3, O21Steel, true, COL_WHITE, 1.05);
            c.Circle(16.2, 16.8, 2.3, O21Steel, true, COL_WHITE, 1.05);
            break;
        }
        case IconKind::Copy:
        {
            c.RoundRect(5.2, 4.6, 14.0, 15.4, 1.0, COL_WHITE, true, O21Ink, 0.95);
            c.RoundRect(9.8, 8.4, 18.6, 19.2, 1.0, COL_WHITE, true, O21Ink, 0.95);
            c.Line(11.8, 11.6, 16.6, 11.6, O21Blue, 0.85);
            c.Line(11.8, 14.2, 16.6, 14.2, O21Blue, 0.85);
            c.Line(11.8, 16.8, 15.0, 16.8, O21Blue, 0.85);
            break;
        }
        case IconKind::Paintbrush:
        {
            c.RoundRect(10.0, 4.4, 14.0, 10.6, 0.8, O21Gold, true, O21Amber, 0.75);
            c.RoundRect(6.0, 10.6, 18.0, 13.8, 0.7, SurfaceSubtle, true, O21Ink, 0.85);
            c.Poly({ { 6.4, 13.8 }, { 17.6, 13.8 }, { 16.2, 19.4 }, { 7.8, 19.4 } },
                   true, O21Amber, 0.85, true, O21GoldLight);
            c.Line(10.0, 16.0, 10.0, 19.2, O21Amber, 0.65);
            c.Line(14.0, 16.0, 14.0, 19.2, O21Amber, 0.65);
            break;
        }
        case IconKind::ClearFormat:
        {
            c.Label(u"A"_ustr, 9.4, 10.4, 13.6, O21FontInk, WEIGHT_NORMAL);
            c.Poly({ { 11.6, 16.0 }, { 16.2, 11.4 }, { 19.2, 14.4 }, { 14.6, 19.0 } },
                   true, O21EraserLine, 0.85, true, O21EraserPale);
            c.Poly({ { 13.8, 13.8 }, { 16.2, 11.4 }, { 19.2, 14.4 }, { 16.8, 16.8 } },
                   true, O21EraserLine, 0.85, true, O21EraserPink);
            break;
        }
        case IconKind::Eraser:
        {
            c.Poly({ { 6.0, 15.2 }, { 13.6, 7.6 }, { 18.4, 12.4 }, { 10.8, 20.0 } },
                   true, O21Ink, 0.9, true, O21MagentaSoft);
            c.Poly({ { 9.6, 11.6 }, { 13.6, 7.6 }, { 18.4, 12.4 }, { 14.4, 16.4 } },
                   true, O21Ink, 0.85, true, O21Magenta);
            c.Line(12.8, 19.2, 18.8, 19.2, O21InkSoft, 0.85);
            break;
        }
        case IconKind::Bold:
            c.Label(u"B"_ustr, 12.0, 12.0, 14.0, O21FontInk, WEIGHT_BOLD, ITALIC_NONE, u"Calibri"_ustr);
            break;
        case IconKind::Italic:
        {
            // Delicate serifed italic I matching Word 2021
            c.Line(11.0, 6.4, 15.2, 6.4, O21FontInk, 0.95);
            c.Line(13.1, 6.6, 10.9, 17.4, O21FontInk, 1.0);
            c.Line(8.8, 17.6, 13.0, 17.6, O21FontInk, 0.95);
            break;
        }
        case IconKind::Underline:
        {
            c.Label(u"U"_ustr, 12.0, 10.3, 13.0, O21FontInk, WEIGHT_NORMAL);
            c.Line(7.4, 17.8, 16.6, 17.8, O21FontInk, 0.95);
            break;
        }
        case IconKind::Strikeout:
        {
            c.Label(u"ab"_ustr, 12.0, 11.4, 11.4, O21FontInk, WEIGHT_NORMAL);
            c.Line(5.2, 12.2, 18.8, 12.2, O21FontInk, 0.9);
            break;
        }
        case IconKind::SubScript:
        {
            c.Label(u"x"_ustr, 9.4, 11.2, 13.0, O21FontInk, WEIGHT_NORMAL);
            c.Label(u"2"_ustr, 16.4, 15.6, 8.4, O21SkyStroke, WEIGHT_NORMAL);
            break;
        }
        case IconKind::SuperScript:
        {
            c.Label(u"x"_ustr, 9.4, 12.8, 13.0, O21FontInk, WEIGHT_NORMAL);
            c.Label(u"2"_ustr, 16.4, 8.2, 8.4, O21SkyStroke, WEIGHT_NORMAL);
            break;
        }
        case IconKind::GrowFont:
        {
            c.Label(u"A"_ustr, 10.2, 12.6, 14.2, O21FontInk, WEIGHT_NORMAL);
            c.Poly({ { 15.2, 8.6 }, { 17.2, 6.4 }, { 19.2, 8.6 } }, false, O21SkyStroke, 1.0);
            break;
        }
        case IconKind::ShrinkFont:
        {
            c.Label(u"A"_ustr, 10.2, 13.2, 12.0, O21FontInk, WEIGHT_NORMAL);
            c.Poly({ { 15.2, 7.2 }, { 17.2, 9.4 }, { 19.2, 7.2 } }, false, O21SkyStroke, 1.0);
            break;
        }
        case IconKind::ChangeCase:
            c.Label(u"Aa"_ustr, 12.0, 12.2, 12.6, O21FontInk, WEIGHT_NORMAL);
            break;
        case IconKind::FontColor:
        {
            c.Label(u"A"_ustr, 12.0, 9.4, 12.6, O21FontInk, WEIGHT_NORMAL);
            c.RoundRect(5.2, 16.8, 18.8, 19.6, 0.3, Color(0xE0, 0x1E, 0x1E), true, Color(0xE0, 0x1E, 0x1E), 0.0);
            break;
        }
        case IconKind::HighlightColor:
        {
            // Word 2021 angled slender highlighter pen cleanly above live bottom 25% yellow color bar
            c.Poly({ { 10.6, 11.4 }, { 15.8, 6.2 }, { 18.2, 8.6 }, { 13.0, 13.8 } },
                   true, O21LineGray, 0.9, true, COL_WHITE);
            c.Line(14.6, 7.4, 17.0, 9.8, O21LineGray, 0.75);
            c.Poly({ { 8.4, 14.8 }, { 10.6, 11.4 }, { 13.0, 13.8 }, { 9.8, 15.2 } },
                   true, O21LineGray, 0.85, true, Color(0xD8, 0xD8, 0xD8));
            c.RoundRect(5.2, 16.8, 18.8, 19.6, 0.3, O21Yellow, true, Color(0xDC, 0xC4, 0x00), 0.3);
            break;
        }
        case IconKind::Spacing:
        {
            // Word 2021 Text Effects: crisp outlined blue A with soft sky-blue glow
            c.Poly({ { 7.2, 17.6 }, { 12.0, 6.2 }, { 16.8, 17.6 } }, false, O21SkyGlow, 2.7);
            c.Line(8.8, 13.8, 15.2, 13.8, O21SkyGlow, 2.4);
            c.Poly({ { 7.2, 17.6 }, { 12.0, 6.2 }, { 16.8, 17.6 } }, false, O21SkyStroke, 1.35);
            c.Line(8.8, 13.8, 15.2, 13.8, O21SkyStroke, 1.25);
            c.Poly({ { 7.6, 17.2 }, { 12.0, 6.8 }, { 16.4, 17.2 } }, false, COL_WHITE, 0.45);
            break;
        }
        case IconKind::Bullets:
        {
            for (int i = 0; i < 3; ++i)
            {
                double yy = 7.0 + i * 5.0;
                c.Circle(6.5, yy, 1.2, O21Blue, true, O21Blue, 0.0);
                c.Line(10.0, yy, 18.2, yy, O21Ink, 0.95);
            }
            break;
        }
        case IconKind::Numbering:
        case IconKind::Outline:
        {
            c.Label(u"1"_ustr, 6.6, 7.0, 6.4, O21Blue, WEIGHT_BOLD);
            c.Label(u"2"_ustr, 6.6, 12.0, 6.4, O21Blue, WEIGHT_BOLD);
            c.Label(u"3"_ustr, 6.6, 17.0, 6.4, O21Blue, WEIGHT_BOLD);
            c.Line(10.2, 7.0, 18.2, 7.0, O21Ink, 0.95);
            c.Line(10.2, 12.0, 18.2, 12.0, O21Ink, 0.95);
            c.Line(10.2, 17.0, 18.2, 17.0, O21Ink, 0.95);
            break;
        }
        case IconKind::AlignLeft:
        case IconKind::AlignCenter:
        case IconKind::AlignRight:
        case IconKind::AlignJustify:
        {
            for (int i = 0; i < 4; ++i)
            {
                double yy = 6.5 + i * 3.6;
                double len = (eKind == IconKind::AlignJustify) ? 13.0 : (i % 2 == 0 ? 13.0 : 8.5);
                double x1 = 5.5;
                if (eKind == IconKind::AlignCenter)
                    x1 = 12.0 - len * 0.5;
                else if (eKind == IconKind::AlignRight)
                    x1 = 18.5 - len;
                c.Line(x1, yy, x1 + len, yy, O21Ink, 0.95);
            }
            break;
        }
        case IconKind::VertTop:
        case IconKind::VertCenter:
        case IconKind::VertBottom:
        {
            const double off = (eKind == IconKind::VertTop) ? 0.0 : (eKind == IconKind::VertCenter ? 3.2 : 6.4);
            c.Line(5.8, 6.6 + off, 18.2, 6.6 + off, O21Ink, 0.95);
            c.Line(7.4, 9.2 + off, 16.6, 9.2 + off, O21Ink, 0.95);
            c.Line(6.4, 11.8 + off, 17.6, 11.8 + off, O21Ink, 0.95);
            break;
        }
        case IconKind::IndentInc:
        case IconKind::IndentDec:
        case IconKind::HangingIndent:
        {
            for (int i = 0; i < 3; ++i)
            {
                double yy = 7.0 + i * 4.8;
                c.Line(10.8, yy, 18.5, yy, O21Ink, 0.95);
            }
            c.Line(5.4, 6.5, 9.2, 6.5, O21InkSoft, 0.85);
            c.Line(5.4, 17.5, 9.2, 17.5, O21InkSoft, 0.85);
            if (eKind == IconKind::IndentDec)
                c.Poly({ { 9.0, 9.4 }, { 5.4, 12.0 }, { 9.0, 14.6 } }, true, O21Blue, 0.5, true, O21Blue);
            else
                c.Poly({ { 5.4, 9.4 }, { 9.0, 12.0 }, { 5.4, 14.6 } }, true, O21Blue, 0.5, true, O21Blue);
            break;
        }
        case IconKind::LineSpacing:
        {
            c.Line(11.0, 7.0, 18.5, 7.0, O21Ink, 0.95);
            c.Line(11.0, 12.0, 18.5, 12.0, O21Ink, 0.95);
            c.Line(11.0, 17.0, 18.5, 17.0, O21Ink, 0.95);
            c.Line(7.0, 7.0, 7.0, 17.0, O21Blue, 0.95);
            c.Poly({ { 5.2, 8.8 }, { 7.0, 6.5 }, { 8.8, 8.8 } }, false, O21Blue, 0.9);
            c.Poly({ { 5.2, 15.2 }, { 7.0, 17.5 }, { 8.8, 15.2 } }, false, O21Blue, 0.9);
            break;
        }
        case IconKind::Shading:
        {
            c.Poly({ { 7.2, 11.0 }, { 11.8, 6.4 }, { 16.4, 11.0 }, { 11.8, 15.6 } },
                   true, O21Ink, 0.85, true, COL_WHITE);
            c.Circle(17.5, 14.2, 1.3, O21Amber, true, O21Yellow, 0.5);
            c.RoundRect(5.2, 17.2, 18.8, 19.6, 0.4, O21Yellow, true, Color(0xD6, 0xBE, 0x00), 0.35);
            break;
        }
        case IconKind::Borders:
        {
            c.RoundRect(5.8, 5.8, 18.2, 18.2, 0.6, COL_WHITE, true, O21InkSoft, 0.7);
            c.Line(12.0, 5.8, 12.0, 18.2, Slate300, 0.7);
            c.Line(5.8, 12.0, 18.2, 12.0, Slate300, 0.7);
            c.Line(5.4, 18.2, 18.6, 18.2, O21Ink, 1.25);
            break;
        }
        case IconKind::Pilcrow:
            c.Label(u"\x00B6"_ustr, 12.0, 12.0, 14.0, O21Ink, WEIGHT_NORMAL);
            break;
        case IconKind::WrapText:
        {
            c.Label(u"ab"_ustr, 8.8, 8.4, 8.5, O21Ink, WEIGHT_NORMAL);
            c.Label(u"c"_ustr, 8.0, 14.8, 8.5, O21Ink, WEIGHT_NORMAL);
            c.Poly({ { 14.5, 11.2 }, { 18.2, 11.2 }, { 18.2, 16.6 }, { 13.4, 16.6 } }, false, O21Blue, 1.0);
            c.Poly({ { 15.4, 14.6 }, { 13.0, 16.6 }, { 15.4, 18.6 } }, false, O21Blue, 1.0);
            break;
        }
        case IconKind::MergeCells:
        {
            c.RoundRect(5.2, 6.2, 18.8, 17.8, 0.8, COL_WHITE, true, O21Ink, 0.9);
            c.Line(10.0, 6.4, 10.0, 17.6, Slate300, 0.65);
            c.Line(14.0, 6.4, 14.0, 17.6, Slate300, 0.65);
            c.RoundRect(7.4, 9.2, 16.6, 14.8, 0.4, O21BlueSoft, true, O21Blue, 0.85);
            c.Line(9.0, 12.0, 15.0, 12.0, O21Blue, 0.9);
            break;
        }
        case IconKind::Currency:
            c.Label(u"$"_ustr, 12.0, 12.0, 14.8, O21Ink, WEIGHT_SEMIBOLD);
            break;
        case IconKind::Percent:
            c.Label(u"%"_ustr, 12.0, 12.0, 13.5, O21Ink, WEIGHT_SEMIBOLD);
            break;
        case IconKind::CommaNumber:
            c.Label(u","_ustr, 12.0, 11.0, 16.0, O21Ink, WEIGHT_BOLD);
            break;
        case IconKind::IncDecimal:
        {
            c.Label(u".00"_ustr, 12.0, 9.2, 9.0, O21Ink, WEIGHT_SEMIBOLD);
            c.Line(6.8, 16.2, 17.2, 16.2, O21Blue, 1.05);
            c.Poly({ { 14.6, 14.0 }, { 17.4, 16.2 }, { 14.6, 18.4 } }, false, O21Blue, 1.05);
            break;
        }
        case IconKind::DecDecimal:
        {
            c.Label(u".0"_ustr, 12.0, 9.2, 9.5, O21Ink, WEIGHT_SEMIBOLD);
            c.Line(6.8, 16.2, 17.2, 16.2, O21Blue, 1.05);
            c.Poly({ { 9.4, 14.0 }, { 6.6, 16.2 }, { 9.4, 18.4 } }, false, O21Blue, 1.05);
            break;
        }
        case IconKind::ConditionalFormat:
        {
            // Excel 2021 crisp 3-row white grid with coral-red (top), sky-blue (mid), coral-red (bottom) bars
            c.RoundRect(4.4, 4.6, 19.6, 19.4, 0.3, COL_WHITE, true, O21LineGray, 0.0);
            c.Line(9.5, 4.6, 9.5, 19.4, O21GridLight, 0.75);
            c.Line(14.5, 4.6, 14.5, 19.4, O21GridLight, 0.75);
            c.Line(4.4, 9.5, 19.6, 9.5, O21GridLight, 0.75);
            c.Line(4.4, 14.5, 19.6, 14.5, O21GridLight, 0.75);
            c.RoundRect(4.6, 4.8, 14.6, 9.4, 0.0, O21CoralFill, true, O21CoralStroke, 0.75);
            c.RoundRect(4.6, 9.6, 11.8, 14.4, 0.0, O21SkyFill, true, O21SkyStroke, 0.75);
            c.RoundRect(4.6, 14.6, 16.0, 19.2, 0.0, O21CoralFill, true, O21CoralStroke, 0.75);
            c.RoundRect(4.4, 4.6, 19.6, 19.4, 0.3, COL_WHITE, false, O21LineGray, 0.95);
            break;
        }
        case IconKind::FormatTable:
        {
            // Excel 2021 crisp 3x3 grid with sky-blue lower-right 2x2 + diagonal blue paintbrush
            c.RoundRect(4.2, 4.6, 18.6, 17.8, 0.3, COL_WHITE, true, O21LineGray, 0.0);
            c.RoundRect(9.0, 9.0, 18.6, 17.8, 0.0, O21SkyFill, true, O21SkyStroke, 0.0);
            c.Line(9.0, 4.6, 9.0, 17.8, O21GridLight, 0.75);
            c.Line(13.8, 4.6, 13.8, 17.8, O21GridLight, 0.75);
            c.Line(4.2, 9.0, 18.6, 9.0, O21GridLight, 0.75);
            c.Line(4.2, 13.4, 18.6, 13.4, O21GridLight, 0.75);
            c.Line(9.0, 9.0, 18.6, 9.0, O21SkyStroke, 0.75);
            c.Line(9.0, 9.0, 9.0, 17.8, O21SkyStroke, 0.75);
            c.RoundRect(4.2, 4.6, 18.6, 17.8, 0.3, COL_WHITE, false, O21LineGray, 0.95);
            c.Line(19.0, 7.2, 13.2, 14.6, COL_WHITE, 2.9);
            c.Poly({ { 18.4, 6.6 }, { 19.8, 7.8 }, { 15.0, 13.6 }, { 13.6, 12.4 } },
                   true, O21LineGray, 0.85, true, COL_WHITE);
            c.Poly({ { 13.4, 12.6 }, { 15.0, 13.8 }, { 13.2, 16.8 }, { 10.6, 18.8 }, { 9.8, 17.8 }, { 11.4, 15.0 } },
                   true, O21SkyStroke, 0.85, true, O21SkyStroke);
            break;
        }
        case IconKind::InsertCells:
        {
            // Excel 2021 airy 3x3 white grid + sky-blue middle row + crisp left-pointing arrow on left edge
            c.RoundRect(5.8, 5.0, 19.8, 18.8, 0.3, COL_WHITE, true, O21LineGray, 0.0);
            c.Line(10.5, 5.0, 10.5, 18.8, O21GridLight, 0.75);
            c.Line(15.2, 5.0, 15.2, 18.8, O21GridLight, 0.75);
            c.Line(5.8, 9.6, 19.8, 9.6, O21GridLight, 0.75);
            c.Line(5.8, 14.2, 19.8, 14.2, O21GridLight, 0.75);
            c.RoundRect(5.8, 5.0, 19.8, 18.8, 0.3, COL_WHITE, false, O21LineGray, 0.95);
            c.RoundRect(5.8, 9.6, 19.8, 14.2, 0.0, O21SkyFill, true, O21SkyStroke, 0.95);
            c.Line(4.0, 11.9, 12.6, 11.9, COL_WHITE, 2.0);
            c.Poly({ { 7.6, 8.2 }, { 3.8, 11.9 }, { 7.6, 15.6 } }, false, COL_WHITE, 2.2);
            c.Line(4.2, 11.9, 12.6, 11.9, O21SkyStroke, 1.0);
            c.Poly({ { 7.6, 8.2 }, { 3.8, 11.9 }, { 7.6, 15.6 } }, false, O21SkyStroke, 1.05);
            break;
        }
        case IconKind::DeleteCells:
        {
            // Excel 2021 airy 3x3 white grid + sky-blue middle cells + crisp coral-red X on right of middle row
            c.RoundRect(4.4, 5.0, 18.4, 18.8, 0.3, COL_WHITE, true, O21LineGray, 0.0);
            c.Line(9.0, 5.0, 9.0, 18.8, O21GridLight, 0.75);
            c.Line(13.7, 5.0, 13.7, 18.8, O21GridLight, 0.75);
            c.Line(4.4, 9.6, 18.4, 9.6, O21GridLight, 0.75);
            c.Line(4.4, 14.2, 18.4, 14.2, O21GridLight, 0.75);
            c.RoundRect(4.4, 5.0, 18.4, 18.8, 0.3, COL_WHITE, false, O21LineGray, 0.95);
            c.RoundRect(6.8, 9.6, 14.2, 14.2, 0.0, O21SkyFill, true, O21SkyStroke, 0.9);
            c.Line(14.2, 8.8, 19.4, 15.0, COL_WHITE, 2.3);
            c.Line(19.4, 8.8, 14.2, 15.0, COL_WHITE, 2.3);
            c.Line(14.4, 9.0, 19.2, 14.8, O21CoralStroke, 1.15);
            c.Line(19.2, 9.0, 14.4, 14.8, O21CoralStroke, 1.15);
            break;
        }
        case IconKind::FormatCells:
        {
            // Excel 2021 crisp 3x3 white grid + sky-blue center cell + blue width bracket bar above top
            c.RoundRect(5.0, 6.8, 19.0, 19.2, 0.3, COL_WHITE, true, O21LineGray, 0.0);
            c.Line(9.6, 6.8, 9.6, 19.2, O21GridLight, 0.75);
            c.Line(14.4, 6.8, 14.4, 19.2, O21GridLight, 0.75);
            c.Line(5.0, 11.0, 19.0, 11.0, O21GridLight, 0.75);
            c.Line(5.0, 15.1, 19.0, 15.1, O21GridLight, 0.75);
            c.RoundRect(5.0, 6.8, 19.0, 19.2, 0.3, COL_WHITE, false, O21LineGray, 0.95);
            c.RoundRect(9.6, 11.0, 14.4, 15.1, 0.0, O21SkyFill, true, O21SkyStroke, 0.95);
            c.Line(8.6, 4.4, 15.4, 4.4, O21SkyStroke, 0.95);
            c.Line(8.6, 3.2, 8.6, 5.2, O21SkyStroke, 0.95);
            c.Line(15.4, 3.2, 15.4, 5.2, O21SkyStroke, 0.95);
            break;
        }
        case IconKind::Sum:
            c.Poly({ { 16.4, 6.2 }, { 7.6, 6.2 }, { 12.6, 12.0 }, { 7.6, 17.8 }, { 16.4, 17.8 } },
                   false, O21Ink, 1.2);
            break;
        case IconKind::FillDown:
        {
            c.RoundRect(6.0, 5.4, 18.0, 9.4, 0.7, COL_WHITE, true, O21Ink, 0.9);
            c.RoundRect(6.0, 10.6, 18.0, 18.4, 0.7, COL_WHITE, true, Slate300, 0.8);
            c.Line(12.0, 10.2, 12.0, 17.4, O21Blue, 1.15);
            c.Poly({ { 9.2, 14.6 }, { 12.0, 17.6 }, { 14.8, 14.6 } }, false, O21Blue, 1.15);
            break;
        }
        case IconKind::Sort:
        case IconKind::SortFilter:
        {
            if (eKind == IconKind::SortFilter || nBaseSize >= 32)
            {
                c.Label(u"A"_ustr, 7.2, 8.2, 8.8, O21SkyStroke, WEIGHT_NORMAL);
                c.Label(u"Z"_ustr, 7.2, 15.8, 8.8, O21LineGray, WEIGHT_NORMAL);
                c.Poly({ { 11.8, 6.4 }, { 20.0, 6.4 }, { 17.0, 11.4 }, { 17.0, 17.8 }, { 14.8, 17.8 }, { 14.8, 11.4 } },
                       true, O21LineGray, 0.95, true, COL_WHITE);
            }
            else
            {
                c.Label(u"A"_ustr, 8.2, 8.2, 8.2, O21Blue, WEIGHT_BOLD);
                c.Label(u"Z"_ustr, 8.2, 15.8, 8.2, O21Ink, WEIGHT_BOLD);
                c.Line(15.6, 6.2, 15.6, 17.8, O21Blue, 1.1);
                c.Poly({ { 13.0, 14.8 }, { 15.6, 18.0 }, { 18.2, 14.8 } }, false, O21Blue, 1.05);
            }
            break;
        }
        case IconKind::Filter:
        {
            c.Poly({ { 5.4, 6.4 }, { 18.6, 6.4 }, { 13.6, 12.2 }, { 13.6, 17.8 }, { 10.4, 19.0 }, { 10.4, 12.2 } },
                   true, O21Ink, 0.95, true, COL_WHITE);
            break;
        }
        case IconKind::Find:
        {
            c.Circle(13.4, 10.2, 5.2, O21LineGray, true, COL_WHITE, 0.95);
            c.Line(9.6, 14.0, 4.8, 18.8, O21LineGray, 1.15);
            break;
        }
        case IconKind::Replace:
        {
            c.Label(u"ab"_ustr, 8.4, 8.0, 8.2, O21Ink, WEIGHT_SEMIBOLD);
            c.Label(u"ac"_ustr, 15.4, 16.0, 8.2, O21Ink, WEIGHT_SEMIBOLD);
            c.Line(14.0, 7.4, 17.6, 11.0, O21Blue, 1.05);
            c.Poly({ { 15.2, 11.2 }, { 17.8, 11.2 }, { 17.8, 8.6 } }, false, O21Blue, 1.0);
            break;
        }
        case IconKind::SelectAll:
        case IconKind::SelectPointer:
        {
            c.Poly({ { 7.6, 5.2 }, { 17.4, 12.8 }, { 12.6, 13.6 }, { 14.8, 18.0 }, { 12.4, 19.0 }, { 10.2, 14.6 }, { 7.6, 17.2 } },
                   true, O21Ink, 0.95, true, COL_WHITE);
            break;
        }
        case IconKind::Save:
        case IconKind::SaveAs:
        {
            c.RoundRect(5.2, 5.2, 18.8, 18.8, 1.6, COL_WHITE, true, O21Ink, 0.95);
            c.RoundRect(8.2, 5.4, 15.2, 10.0, 0.5, O21BlueSoft, true, O21Blue, 0.75);
            c.RoundRect(7.6, 13.2, 16.4, 18.6, 0.6, COL_WHITE, true, O21InkSoft, 0.8);
            break;
        }
        case IconKind::Undo:
        {
            c.Poly({ { 17.5, 16.5 }, { 17.5, 12.0 }, { 15.2, 9.5 }, { 7.5, 9.5 } }, false, O21Blue, 1.2);
            c.Poly({ { 10.5, 6.2 }, { 6.5, 9.5 }, { 10.5, 12.8 } }, false, O21Blue, 1.2);
            break;
        }
        case IconKind::Redo:
        {
            c.Poly({ { 6.5, 16.5 }, { 6.5, 12.0 }, { 8.8, 9.5 }, { 16.5, 9.5 } }, false, O21Blue, 1.2);
            c.Poly({ { 13.5, 6.2 }, { 17.5, 9.5 }, { 13.5, 12.8 } }, false, O21Blue, 1.2);
            break;
        }
        default:
        {
            DrawIconArtwork(c, eKind, HarmonizeOffice2021Palette(ePal));
            break;
        }
    }
}

bool RenderOffice2021Icon(IconKind eKind, PaletteId ePalette, tools::Long nBaseSize,
                          sal_Int32 nScalePercentage, Bitmap& rBitmap)
{
    if (eKind == IconKind::None)
        return false;

    const sal_Int32 nPct = std::max<sal_Int32>(100, nScalePercentage);
    const tools::Long nSz = std::max<tools::Long>(16, std::lround(nBaseSize * nPct / 100.0));

    ScopedVclPtrInstance<VirtualDevice> pVDev(DeviceFormat::WITH_ALPHA);
    pVDev->SetOutputSizePixel(Size(nSz, nSz), true, true);
    pVDev->SetAntialiasing(AntialiasingFlags::Enable | AntialiasingFlags::PixelSnapHairline);

    const double fScale = (nBaseSize >= 24) ? 1.15 : 1.22;
    const double fStroke = (nBaseSize >= 32) ? 0.78 : ((nBaseSize >= 24) ? 0.95 : 1.25);
    Canvas aCanvas(*pVDev, nSz, false, fScale, fStroke);

    DrawOffice2021Artwork(aCanvas, eKind, ePalette, nBaseSize);
    rBitmap = pVDev->GetBitmap(Point(0, 0), Size(nSz, nSz));
    return !rBitmap.IsEmpty();
}

const CommandMapEntry* FindCommandEntry(const OUString& rName, tools::Long& rBaseSize)
{
    OUString sLower = rName.toAsciiLowerCase();
    if (!sLower.startsWith(u"cmd/"))
        return nullptr;

    std::u16string_view aRest = std::u16string_view(sLower).substr(4);
    if (o3tl::starts_with(aRest, u"rltb/"))
        aRest = aRest.substr(5);

    std::u16string_view aCmd;
    if (o3tl::starts_with(aRest, u"sc_"))
    {
        rBaseSize = 16;
        aCmd = aRest.substr(3);
    }
    else if (o3tl::starts_with(aRest, u"lc_"))
    {
        rBaseSize = 24;
        aCmd = aRest.substr(3);
    }
    else if (o3tl::starts_with(aRest, u"32/"))
    {
        rBaseSize = 32;
        aCmd = aRest.substr(3);
    }
    else
    {
        return nullptr;
    }

    size_t nDot = aCmd.rfind('.');
    if (nDot != std::u16string_view::npos)
        aCmd = aCmd.substr(0, nDot);
    if (aCmd.empty())
        return nullptr;

    for (const auto& rEntry : gCommandTable)
    {
        if (o3tl::equalsAscii(aCmd, rEntry.name))
            return &rEntry;
    }
    return nullptr;
}

} // namespace

bool TryRenderCommandIcon(const OUString& rName, sal_Int32 nScalePercentage, Bitmap& rBitmap)
{
    OUString sLower = rName.toAsciiLowerCase();

    if (sLower.startsWith(u"sc/res/sc2604") || sLower.startsWith(u"sc/res/sc2605"))
    {
        std::u16string_view aStem = std::u16string_view(sLower).substr(7);
        size_t nDot = aStem.rfind('.');
        if (nDot != std::u16string_view::npos)
            aStem = aStem.substr(0, nDot);

        if (aStem == u"sc26047")
            return RenderOffice2021Icon(IconKind::FunctionFx, PaletteId::Slate, 16, nScalePercentage, rBitmap);
        if (aStem == u"sc26048")
            return RenderOffice2021Icon(IconKind::Sum, PaletteId::Slate, 16, nScalePercentage, rBitmap);
        if (aStem == u"sc26049")
            return RenderOffice2021Icon(IconKind::FormulaEqual, PaletteId::Slate, 16, nScalePercentage, rBitmap);
        if (aStem == u"sc26050")
            return RenderStyledIcon(IconKind::FormulaCancel, PaletteId::Red, 16, nScalePercentage, false, rBitmap);
        if (aStem == u"sc26051")
            return RenderStyledIcon(IconKind::FormulaOk, PaletteId::Emerald, 16, nScalePercentage, false, rBitmap);
    }

    tools::Long nBaseSize = 0;
    const CommandMapEntry* pFound = FindCommandEntry(rName, nBaseSize);
    if (!pFound || pFound->kind == IconKind::None)
        return false;

    return RenderStyledIcon(pFound->kind, pFound->palette, nBaseSize, nScalePercentage, nBaseSize >= 24, rBitmap);
}

bool TryRenderOffice2021CommandIcon(const OUString& rName, sal_Int32 nScalePercentage, Bitmap& rBitmap)
{
    tools::Long nBaseSize = 0;
    const CommandMapEntry* pFound = FindCommandEntry(rName, nBaseSize);
    if (!pFound || pFound->kind == IconKind::None)
        return false;

    return RenderOffice2021Icon(pFound->kind, pFound->palette, nBaseSize, nScalePercentage, rBitmap);
}

} // namespace vcl::officestudio

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
