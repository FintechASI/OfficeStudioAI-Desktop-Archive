/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <rtl/ustring.hxx>
#include <sal/types.h>
#include <vcl/bitmap.hxx>

namespace vcl::officestudio
{

/** Renders a colorful OfficeStudio vector command icon for paths such as
 *  cmd/sc_<cmd>.png, cmd/lc_<cmd>.png, cmd/32/<cmd>.png (or .svg).
 */
bool TryRenderCommandIcon(const OUString& rName, sal_Int32 nScalePercentage, Bitmap& rBitmap);

/** Renders a refined Microsoft Office 2021-inspired colorful monoline vector
 *  command icon (transparent background, no bright square tiles) for Docs &
 *  Spreadsheet Ribbons.
 */
bool TryRenderOffice2021CommandIcon(const OUString& rName, sal_Int32 nScalePercentage, Bitmap& rBitmap);

} // namespace vcl::officestudio

