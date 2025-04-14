#pragma once
#include <Windows.h>

enum : WORD
{
	{%- for proto in parser.m_Protocols %}
    {{ proto.packet_name.upper() }} = {{ proto.packet_id }},
	{%- endfor %}
};