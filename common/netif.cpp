
#include "netif.h"


#ifdef WIN32

#include <winsock2.h>
#include <iphlpapi.h>

#pragma comment(lib, "wsock32.lib")
#pragma comment(lib, "iphlpapi.lib")

#pragma warning(disable:4996)

void NetworkInterfaces::get_nic_info(std::vector<Adapter> *result)
{
	std::vector<unsigned char> buffer;

	PIP_ADAPTER_INFO dapterinfo;
	PIP_ADAPTER_INFO adapter = NULL;
	DWORD rc = 0;
	size_t i;

	ULONG outlen = sizeof(IP_ADAPTER_INFO);
	buffer.resize(outlen);

	dapterinfo = (IP_ADAPTER_INFO *)&buffer[0];
	if (!dapterinfo) {
		return;
	}

	if (GetAdaptersInfo(dapterinfo, &outlen) == ERROR_BUFFER_OVERFLOW) {
		buffer.resize(outlen);
		dapterinfo = (IP_ADAPTER_INFO *)&buffer[0];
	}

	rc = GetAdaptersInfo(dapterinfo, &outlen);
	if (rc == NO_ERROR) {
		adapter = dapterinfo;
		while (adapter) {
			std::string name(adapter->AdapterName);
			unsigned long long address = 0;
			for (i = 0; i < adapter->AddressLength; i++) {
				address = (address << 8) | adapter->Address[i];
			}
			Adapter nic(name, address);
			result->push_back(nic);
			adapter = adapter->Next;
		}
	}
}

void NetworkInterfaces::open()
{
	get_nic_info(&table);
}

void NetworkInterfaces::close()
{
}

bool NetworkInterfaces::query(char const *name, unsigned long long *address)
{
	if (!name) {
		return false;
	}
	char const *left = strchr(name, '{');
	char const *right = strchr(name, '}');
	if (!left || !right || left > right) {
		return false;
	}
	right++;
	std::string id(left, right);

	for (size_t i = 0; i < table.size(); i++) {
		size_t n = right - left;
		if (n == table[i].name.size()) {
			if (memicmp(left, table[i].name.c_str(), n) == 0) {
				*address = table[i].address;
				return true;
			}
		}

	}
	return false;
}

#else // WIN32

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>

void NetworkInterfaces::open()
{
}

void NetworkInterfaces::close()
{
}

bool NetworkInterfaces::query(char const *name, unsigned long long *address)
{
	int fd;
	struct ifreq ifr;

	fd = socket(AF_INET, SOCK_DGRAM, 0);

	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, name, IFNAMSIZ - 1);

	if (ioctl(fd, SIOCGIFHWADDR, &ifr) == -1) {
		::close(fd);
		return false;
	}

	::close(fd);

	unsigned long long a = 0;
	for (int i = 0; i < 6; i++) {
		a = (a << 8) | (unsigned char)ifr.ifr_hwaddr.sa_data[i];
	}
	*address = a;

	return true;
}


#endif // WIN32
