// Pegasus Frontend
// Copyright (C) 2017  Mátyás Mustoha
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.


#pragma once

class QString;
enum class AssetType : unsigned char;


namespace pegasus_assets {

AssetType str_to_type(const QString&);

/// Pegasus X: a folder or file name as names are compared: lower case,
/// letters and digits only ("3D Boxes", "3d_boxes" and "3dboxes" are one)
QString plain_name(const QString&);
/// Whether a plain name is `wanted` or its plural (-s, -es), or the other way round
bool same_plain_name(const QString& plain, const QString& wanted);
/// Whether a picture is a 3D box render, by its folder or file name
/// (3dboxes/, box3d/, "3D Box"/, 3dbox.png, ...)
bool is_3d_box(const QString& path);

} // namespace pegasus_assets
