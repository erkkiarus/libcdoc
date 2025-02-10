/*
 * libcdoc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include "XmlWriter.h"

#include <string>

class DDOCWriter: public XMLWriter
{
public:
	DDOCWriter(const std::string &path);
	DDOCWriter(std::vector<uint8_t>& vec);
	~DDOCWriter();

	void addFile(const std::string &name, const std::string &mime, const std::vector<unsigned char> &data);
	void close() override;

private:
	DDOCWriter(const DDOCWriter &) = delete;
	DDOCWriter &operator=(const DDOCWriter &) = delete;
	struct Private;
	Private *d;
};
