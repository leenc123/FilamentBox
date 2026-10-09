/**
<one line to give the program's name and a brief idea of what it does.>
Copyright (C) <2024>  <JianFeng>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef _IMAGE_H_
#define _IMAGE_H_

#ifdef __cplusplus
extern "C"
{
#endif

extern const unsigned char *logo_allArray[];
extern const unsigned char boot_spool_48[];  // 开机线盘图标 48x48（备用）
extern const unsigned char boot_nfc_48[];    // 开机 NFC 卡片图标 48x48

#ifdef __cplusplus
}
#endif

#endif
