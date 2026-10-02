/* DirectPlay Lite
 * Copyright (C) 2018 Daniel Collins <solemnwarning@solemnwarning.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifdef USE_ICONV
#include <iconv.h>
#endif
#include <stdexcept>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <utility>
#include <windows.h>

#include "packet.hpp"

const uint32_t FIELD_TYPE_NULL    = 0;
const uint32_t FIELD_TYPE_DWORD   = 1;
const uint32_t FIELD_TYPE_DATA    = 2;
const uint32_t FIELD_TYPE_WSTRING = 3;
const uint32_t FIELD_TYPE_GUID    = 4;

PacketSerialiser::PacketSerialiser(uint32_t type)
{
	/* Avoid reallocations during packet construction unless we get given a lot of data. */
	sbuf.reserve(4096);
	
	TLVChunk header;
	header.type = type;
	header.value_length = 0;
	
	sbuf.insert(sbuf.begin(), (unsigned char*)(&header), (unsigned char*)(&header + 1));

#ifdef USE_ICONV
	convd = iconv_open("UTF-16LE", "wchar_t");
	if (convd == (iconv_t)(-1))
		throw std::runtime_error("Failed to get iconv descriptor");
#endif
}

PacketSerialiser::~PacketSerialiser()
{
#ifdef USE_ICONV
	iconv_close(convd);
#endif
}

std::pair<const void*, size_t> PacketSerialiser::raw_packet() const
{
	return std::make_pair<const void*, size_t>(sbuf.data(), sbuf.size());
}

void PacketSerialiser::append_null()
{
	TLVChunk header;
	header.type = FIELD_TYPE_NULL;
	header.value_length = 0;
	
	sbuf.insert(sbuf.end(), (unsigned char*)(&header), (unsigned char*)(&header + 1));
	
	((TLVChunk*)(sbuf.data()))->value_length += sizeof(header);
}

void PacketSerialiser::append_dword(DWORD value)
{
	TLVChunk header;
	header.type = FIELD_TYPE_DWORD;
	header.value_length = sizeof(DWORD);
	
	sbuf.insert(sbuf.end(), (unsigned char*)(&header), (unsigned char*)(&header + 1));
	sbuf.insert(sbuf.end(), (unsigned char*)(&value),  (unsigned char*)(&value + 1));
	
	((TLVChunk*)(sbuf.data()))->value_length += sizeof(header) + sizeof(value);
}

void PacketSerialiser::append_data(const void *data, size_t size)
{
	TLVChunk header;
	header.type = FIELD_TYPE_DATA;
	header.value_length = size;
	
	sbuf.insert(sbuf.end(), (unsigned char*)(&header), (unsigned char*)(&header + 1));
	sbuf.insert(sbuf.end(), (unsigned char*)(data),  (unsigned char*)(data) + size);
	
	((TLVChunk*)(sbuf.data()))->value_length += sizeof(header) + size;
}

void PacketSerialiser::append_wstring(const std::wstring &string)
{
#ifdef USE_ICONV
	/* We are converting to UTF-16, so every code point can take either 2 or 4 bytes. */
	std::vector<unsigned char> buffer(string.length() * 4);
	
	size_t string_bytes = 0;

	if (buffer.size() > 0)
	{
		char *inbuf = (char*)(string.data());
		size_t inbytesleft = string.length() * sizeof(wchar_t);
		char *outbuf = (char*)(buffer.data());
		size_t outbytesleft = buffer.size();
		
		if (iconv(convd, &inbuf, &inbytesleft, &outbuf, &outbytesleft) != (size_t)(-1))
		{
			string_bytes = buffer.size() - outbytesleft;
		}

		/* Flush for next conversion. */
		iconv(convd, NULL, NULL, &outbuf, &outbytesleft);
	}
#else
	size_t string_bytes = string.length() * sizeof(wchar_t);
#endif
	
	TLVChunk header;
	header.type = FIELD_TYPE_WSTRING;
	header.value_length = string_bytes;
	
	sbuf.insert(sbuf.end(), (unsigned char*)(&header),       (unsigned char*)(&header + 1));
	if (string_bytes > 0)
	{
#ifdef USE_ICONV
		sbuf.insert(sbuf.end(), (unsigned char*)(buffer.data()), (unsigned char*)(buffer.data()) + string_bytes);
#else
		sbuf.insert(sbuf.end(), (unsigned char*)(string.data()), (unsigned char*)(string.data()) + string_bytes);
#endif
	}
	
	((TLVChunk*)(sbuf.data()))->value_length += sizeof(header) + string_bytes;
}

void PacketSerialiser::append_guid(const GUID &guid)
{
	TLVChunk header;
	header.type = FIELD_TYPE_GUID;
	header.value_length = sizeof(GUID);
	
	sbuf.insert(sbuf.end(), (unsigned char*)(&header), (unsigned char*)(&header + 1));
	sbuf.insert(sbuf.end(), (unsigned char*)(&guid),   (unsigned char*)(&guid) + sizeof(GUID));
	
	((TLVChunk*)(sbuf.data()))->value_length += sizeof(header) + sizeof(GUID);
}

PacketDeserialiser::PacketDeserialiser(const void *serialised_packet, size_t packet_size)
{
	header = (const TLVChunk*)(serialised_packet);
	
	if(packet_size < sizeof(TLVChunk) || packet_size < sizeof(TLVChunk) + header->value_length)
	{
		throw Error::Incomplete();
	}
	
	const unsigned char *at = header->value;
	size_t value_remain = header->value_length;
	
	while(value_remain > 0)
	{
		const TLVChunk *field = (TLVChunk*)(at);
		
		if(value_remain < sizeof(TLVChunk) || value_remain < sizeof(TLVChunk) + field->value_length)
		{
			throw Error::Malformed();
		}
		
		fields.push_back(field);
		
		at           += sizeof(TLVChunk) + field->value_length;
		value_remain -= sizeof(TLVChunk) + field->value_length;
	}

#ifdef USE_ICONV
	convd = iconv_open("wchar_t", "UTF-16LE");
	if (convd == (iconv_t)(-1))
		throw std::runtime_error("Failed to get iconv descriptor");
#endif
}

PacketDeserialiser::~PacketDeserialiser()
{
#ifdef USE_ICONV
	iconv_close(convd);
#endif
}

uint32_t PacketDeserialiser::packet_type() const
{
	return header->type;
}

size_t PacketDeserialiser::num_fields() const
{
	return fields.size();
}

bool PacketDeserialiser::is_null(size_t index) const
{
	if(fields.size() <= index)
	{
		throw Error::MissingField();
	}
	
	return (fields[index]->type == FIELD_TYPE_NULL);
}

DWORD PacketDeserialiser::get_dword(size_t index) const
{
	if(fields.size() <= index)
	{
		throw Error::MissingField();
	}
	
	if(fields[index]->type != FIELD_TYPE_DWORD)
	{
		throw Error::TypeMismatch();
	}
	
	if(fields[index]->value_length != sizeof(DWORD))
	{
		throw Error::Malformed();
	}
	
	return *(DWORD*)(fields[index]->value);
}

std::pair<const void*,size_t> PacketDeserialiser::get_data(size_t index) const
{
	if(fields.size() <= index)
	{
		throw Error::MissingField();
	}
	
	if(fields[index]->type != FIELD_TYPE_DATA)
	{
		throw Error::TypeMismatch();
	}
	
	return std::make_pair((const void*)(fields[index]->value), (size_t)(fields[index]->value_length));
}

std::wstring PacketDeserialiser::get_wstring(size_t index) const
{
	if(fields.size() <= index)
	{
		throw Error::MissingField();
	}
	
	if(fields[index]->type != FIELD_TYPE_WSTRING)
	{
		throw Error::TypeMismatch();
	}
	
#ifdef USE_ICONV
	if((fields[index]->value_length % sizeof(char16_t)) != 0)
	{
		throw Error::Malformed();
	}
	
	/* We are converting to the native wchar_t. This is dependent on the OS, but is generally UTF-32 or UTF-16. */
	static_assert(sizeof(wchar_t) <= 4, "wchar_t must be at most 4 bytes");
	std::vector<unsigned char> buffer((size_t)(fields[index]->value_length) / sizeof(char16_t) * 4);
	
	if (buffer.size() == 0)
		return L"";
	
	char *inbuf = (char*)(fields[index]->value);
	size_t inbytesleft = (size_t)(fields[index]->value_length);
	char *outbuf = (char*)(buffer.data());
	size_t outbytesleft = buffer.size();
	
	size_t converted = iconv(convd, &inbuf, &inbytesleft, &outbuf, &outbytesleft);
	size_t string_bytes = buffer.size() - outbytesleft;

	/* Flush for next conversion. */
	iconv(convd, NULL, NULL, &outbuf, &outbytesleft);

	if (converted == (size_t)(-1))
	{
		throw Error::Malformed();
	}
	
	return std::wstring((const wchar_t*)(buffer.data()), string_bytes / sizeof(wchar_t));
#else
	if((fields[index]->value_length % sizeof(wchar_t)) != 0)
	{
		throw Error::Malformed();
	}
	
	return std::wstring((const wchar_t*)(fields[index]->value), (fields[index]->value_length / sizeof(wchar_t)));
#endif
}

GUID PacketDeserialiser::get_guid(size_t index) const
{
	if(fields.size() <= index)
	{
		throw Error::MissingField();
	}
	
	if(fields[index]->type != FIELD_TYPE_GUID)
	{
		throw Error::TypeMismatch();
	}
	
	if(fields[index]->value_length != sizeof(GUID))
	{
		throw Error::Malformed();
	}
	
	return *(GUID*)(fields[index]->value);
}
