
#ifndef __NETADDR_H
#define __NETADDR_H

#include <string.h>
#include <vector>
#include <string>
#include "mem.h"

class mac_address_t {
private:
	unsigned char values[6];
	int compare(mac_address_t const &r)
	{
		return memcmp(values, r.values, 6);
	}
	void assign(unsigned long long a)
	{
		int i = 6;
		while (i > 0) {
			i--;
			values[i] = (unsigned char)a;
			a >>= 8;
		}
	}
public:
	mac_address_t()
	{
		memset(values, 0, 6);
	}
	mac_address_t(mac_address_t const &r)
	{
		memcpy(values, r.values, 6);
	}
	mac_address_t(unsigned long long a)
	{
		assign(a);
	}
	mac_address_t(unsigned char const *p)
	{
		memcpy(values, p, 6);
	}
	void operator = (mac_address_t const &r)
	{
		memcpy(values, r.values, 6);
	}
	void operator = (unsigned char const *p)
	{
		memcpy(values, p, 6);
	}
	void operator = (unsigned long long a)
	{
		assign(a);
	}
	bool operator == (mac_address_t const &r)
	{
		return compare(r) == 0;
	}
	bool operator != (mac_address_t const &r)
	{
		return compare(r) != 0;
	}
	bool operator < (mac_address_t const &r)
	{
		return compare(r) < 0;
	}
	bool operator > (mac_address_t const &r)
	{
		return compare(r) > 0;
	}
	bool operator <= (mac_address_t const &r)
	{
		return compare(r) <= 0;
	}
	bool operator >= (mac_address_t const &r)
	{
		return compare(r) >= 0;
	}
	std::string tostring() const;
	bool parse(char const *ptr);
	bool empty() const
	{
		for (int i = 0; i < 6; i++) {
			if (values[i] != 0) {
				return false;
			}
		}
		return true;
	}
};

//typedef unsigned long ipv4addr_t;
class ipv4addr_t {
	friend class ip_address_t;
private:
	unsigned long addr;
public:
	ipv4addr_t()
	{
		addr = 0;
	}
	ipv4addr_t(unsigned long a)
	{
		addr = a;
	}
	void operator = (unsigned long a)
	{
		addr = a;
	}
	operator unsigned long () const
	{
		return addr;
	}
	bool operator < (ipv4addr_t const &r) const
	{
		return addr < r.addr;
	}
	bool operator > (ipv4addr_t const &r) const
	{
		return addr > r.addr;
	}
	bool operator <= (ipv4addr_t const &r) const
	{
		return addr <= r.addr;
	}
	bool operator >= (ipv4addr_t const &r) const
	{
		return addr >= r.addr;
	}
	bool operator == (ipv4addr_t const &r) const
	{
		return addr == r.addr;
	}
	bool operator != (ipv4addr_t const &r) const
	{
		return addr != r.addr;
	}
	unsigned long operator >> (int n) const
	{
		return addr >> n;
	}
	unsigned long operator << (int n) const
	{
		return addr << n;
	}
	std::string tostring() const;
	bool parse(char const *ptr);
};

class ipv6addr_t {
	friend class ip_address_t;
private:
	unsigned short element[8];
	void clear()
	{
		for (int i = 0; i < 8; i++) {
			element[i] = 0;
		}
	}
	int compare(ipv6addr_t const &r) const;
public:
	ipv6addr_t()
	{
		clear();
	}
	ipv6addr_t(unsigned short const *p)
	{
		for (int i = 0; i < 8; i++) {
			element[i] = p[i];
		}
	}
	void operator = (ipv6addr_t const &a)
	{
		memcpy(element, a.element, sizeof(element));
	}
	bool operator < (ipv6addr_t const &r) const
	{
		return compare(r) < 0;
	}
	bool operator > (ipv6addr_t const &r) const
	{
		return compare(r) > 0;
	}
	bool operator <= (ipv6addr_t const &r) const
	{
		return compare(r) <= 0;
	}
	bool operator >= (ipv6addr_t const &r) const
	{
		return compare(r) >= 0;
	}
	bool operator == (ipv6addr_t const &r) const
	{
		return compare(r) == 0;
	}
	bool operator != (ipv6addr_t const &r) const
	{
		return compare(r) != 0;
	}

	std::string tostring() const;
	bool parse(char const *ptr);
};

class ip_address_t {
private:
public: // for debug
	union {
		unsigned long v4;
		unsigned short v6[8];
	} element;
public:
	unsigned char version;
private:
	int compare(ip_address_t const &r) const;
public:
	void assign(ipv6addr_t const &a)
	{
		version = 6;
		for (int i = 0; i < 8; i++) {
			element.v6[i] = a.element[i];
		}
	}
	void assign(unsigned long a)
	{
		version = 4;
		element.v4 = a;
	}
	void assign(unsigned short const *p)
	{
		version = 6;
		for (int i = 0; i < 8; i++) {
			element.v6[i] = BE2(p + i);
		}
	}
	ip_address_t()
	{
		version = 0;
	}
	ip_address_t(unsigned long a)
	{
		assign(a);
	}
	ip_address_t(ipv4addr_t const &a)
	{
		assign((unsigned long)a);
	}
	ip_address_t(unsigned short const *p)
	{
		assign(p);
	}
	ip_address_t(ip_address_t const &r)
	{
		assign(r);
	}
	void assign(ip_address_t const &r)
	{
		version = r.version;
		memcpy(&element, &r.element, sizeof(element));
	}
	void operator = (ip_address_t const &r)
	{
		assign(r);
	}
	bool operator < (ip_address_t const &r) const
	{
		return compare(r) < 0;
	}
	bool operator > (ip_address_t const &r) const
	{
		return compare(r) > 0;
	}
	bool operator <= (ip_address_t const &r) const
	{
		return compare(r) <= 0;
	}
	bool operator >= (ip_address_t const &r) const
	{
		return compare(r) >= 0;
	}
	bool operator == (ip_address_t const &r) const
	{
		return compare(r) == 0;
	}
	bool operator != (ip_address_t const &r) const
	{
		return compare(r) != 0;
	}

	std::string tostring() const;
	bool parse(char const *ptr);

	bool empty() const
	{
		if (version == 4 || version == 6) {
			return false;
		}
		return true;
	}
};



#endif



