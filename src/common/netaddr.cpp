
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "netaddr.h"

#pragma warning(disable:4996)

bool mac_address_t::parse(char const *ptr)
{
	int a, b, c, d, e, f;
	if (sscanf(ptr, "%x:%x:%x:%x:%x:%x", &a, &b, &c, &d, &e, &f) == 6) {
		//
	} else if (sscanf(ptr, "%x-%x-%x-%x-%x-%x", &a, &b, &c, &d, &e, &f) == 6) {
		//
	} else {
		return false;
	}
	if (a < 0 || a > 255) return false;
	if (b < 0 || b > 255) return false;
	if (c < 0 || c > 255) return false;
	if (d < 0 || d > 255) return false;
	if (e < 0 || e > 255) return false;
	if (f < 0 || f > 255) return false;
	values[0] = a;
	values[1] = b;
	values[2] = c;
	values[3] = d;
	values[4] = e;
	values[5] = f;
	return true;
}


std::string ipv4addr_t::tostring() const
{
	char tmp[100];
	unsigned char a = (unsigned char)(addr >> 24);
	unsigned char b = (unsigned char)(addr >> 16);
	unsigned char c = (unsigned char)(addr >> 8);
	unsigned char d = (unsigned char)(addr >> 0);
	sprintf(tmp, "%u.%u.%u.%u", a, b, c, d);
	return tmp;
}


bool ipv4addr_t::parse(char const *str)
{
	int a, b, c, d;
	if (sscanf(str, "%u.%u.%u.%u", &a, &b, &c, &d) != 4) {
		return false;
	}
	if (a < 0 || a > 255) return false;
	if (b < 0 || b > 255) return false;
	if (c < 0 || c > 255) return false;
	if (d < 0 || d > 255) return false;
	addr = (a << 24) | (b << 16) | (c << 8) | d;
	return true;
}


static std::string ipv6tostring(unsigned short const *element)
{
	char tmp[100], *p;
	int i, n;
	int zi = 8;
	int zn = 0;
	for (i = 0; i < 8; i++) {
		if (element[i] == 0) {
			for (n = 1; i + n < 8 && element[i + n] == 0; n++);
			if (zn < n) {
				zi = i;
				zn = n;
			}
		}
	}
	n = 0;
	i = 0;
	p = tmp;
	while (i < 8) {
		if (i == zi) {
			*p++ = ':';
			*p++ = ':';
			i += zn;
		} else {
			if (i > 0 && i != zi + zn) {
				*p++ = ':';
			}
			sprintf(p, "%x", element[i]);
			p += strlen(p);
			i++;
		}
	}
	*p = 0;
	return tmp;
}

std::string ipv6addr_t::tostring() const
{
	return ipv6tostring(element);
}

bool ipv6addr_t::parse(char const *ptr)
{
	char const *end = ptr + strlen(ptr);

	int i, j;
	unsigned short arr[8];
	for (i = 0; i < 8; i++) {
		arr[i] = 0;
	}
	if (ptr + 2 > end) {
		return false;
	}
	if (ptr[0] == ':' && ptr[1] == ':') {
		i = 8;
		ptr += 2;
		while (ptr < end) {
			char const *p;
			for (p = end; ptr < p && isxdigit(p[-1]); p--);
			if (i > 0 && p < end && p[-1] == ':') {
				unsigned long v = strtol(p, 0, 16);;
				if (v > 0xffff) {
					return false;
				}
				arr[--i] = (unsigned short)v;
				end = p - 1;
			} else {
				return false;
			}
		}
	} else if (end[-1] == ':' && end[-2] == ':') {
		i = 0;
		end -= 2;
		while (ptr < end) {
			char const *p;
			for (p = ptr; p < end && isxdigit(p[0]); p++);
			if (i < 8 && ptr < p && p[0] == ':') {
				unsigned long v = strtol(ptr, 0, 16);
				if (v > 0xffff) {
					return false;
				}
				arr[i++] = (unsigned short)v;
				ptr = p + 1;
			} else {
				return false;
			}
		}
	} else {
		char const *mid;
		for (mid = ptr; mid < end; mid++) {
			if (mid + 1 < end && mid[0] == ':' && mid[1] == ':') {
				break;
			}
		}
		i = 0;
		j = 8;
		while (ptr < mid) {
			char const *p;
			for (p = ptr; p < mid && isxdigit(p[0]); p++);
			if (ptr < p && i < j) {
				unsigned long v = strtol(ptr, 0, 16);
				if (v > 0xffff) {
					return false;
				}
				arr[i++] = (unsigned short)v;
				ptr = p;
				if (ptr < mid) {
					if (ptr[0] == ':') {
						ptr++;
					} else {
						return false;
					}
				}
			} else {
				return false;
			}
		}
		if (mid + 2 < end) {
			mid += 2;
			while (mid < end) {
				char const *p;
				for (p = end; ptr < p && isxdigit(p[-1]); p--);
				if (p < end && i < j) {
					unsigned long v = strtol(p, 0, 16);
					if (v > 0xffff) {
						return false;
					}
					arr[--j] = (unsigned short)v;
					end = p;
					if (mid < end) {
						if (end[-1] == ':') {
							end--;
						} else {
							return false;
						}
					}
				} else {
					return false;
				}
			}
		}
	}
	memcpy(element, arr, sizeof(arr));
	return true;
}


std::string mac_address_t::tostring() const
{
	char tmp[100];
	sprintf(tmp, "%02X:%02X:%02X:%02X:%02X:%02X"
		, values[0]
		, values[1]
		, values[2]
		, values[3]
		, values[4]
		, values[5]
		);
		return tmp;
}


static int ipv6compare(unsigned short const *left, unsigned short const *right)
{
	for (int i = 0; i < 8; i++) {
		unsigned short x = left[i];
		unsigned short y = right[i];
		if (x < y) return -1;
		if (x > y) return 1;
	}
	return 0;
}

int ipv6addr_t::compare(ipv6addr_t const &r) const
{
	return ipv6compare(element, r.element);
}

int ip_address_t::compare(ip_address_t const &r) const
{
	if (version < r.version) {
		return -1;
	}
	if (version > r.version) {
		return 1;
	}
	if (version == 4) {
		if (element.v4 < r.element.v4) {
			return -1;
		}
		if (element.v4 > r.element.v4) {
			return 1;
		}
		return 0;
	}
	if (version == 6) {
		return ipv6compare(element.v6, r.element.v6);
	}
	assert(0);
	return 0;
}

std::string ip_address_t::tostring() const
{
	if (version == 4) {
		ipv4addr_t a(element.v4);
		return a.tostring();
	}
	if (version == 6) {
		return ipv6tostring(element.v6);
	}
	return std::string();
}

bool ip_address_t::parse(char const *ptr)
{
	ipv4addr_t v4;
	if (v4.parse(ptr)) {
		version = 4;
		element.v4 = v4;
		return true;
	} 
	ipv6addr_t v6;
	if (v6.parse(ptr)) {
		version = 6;
		memcpy(element.v6, v6.element, sizeof(element.v6));
		return true;
	}
	return false;
}


