
#ifndef __NETIF_H
#define __NETIF_H

#include <vector>
#include <string>

class NetworkInterfaces {

#ifdef WIN32
private:
	struct Adapter {
		std::string name;
		unsigned long long address;
		Adapter()
		{
			address = 0;
		}
		Adapter(std::string n, unsigned long long a)
			: name(n)
			, address(a)
		{
		}
	};
	std::vector<Adapter> table;
	static void get_nic_info(std::vector<NetworkInterfaces::Adapter> *result);
#endif

public:

	~NetworkInterfaces()
	{
		close();
	}

	void open();
	void close();
	bool query(char const *name, unsigned long long *address);
};

#endif
