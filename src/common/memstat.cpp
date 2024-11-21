
#include "memstat.h"

#if defined(WIN32)

#include <windows.h>

double memory_used_ratio()
{
	MEMORYSTATUS ms;
	ms.dwLength = sizeof(ms);
	GlobalMemoryStatus(&ms);
	return ms.dwMemoryLoad;
}

#elif defined(Linux)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

double memory_used_ratio()
{
	int memtotal = -1;
	int memfree = -1;
	FILE *fp;
	fp = fopen("/proc/meminfo", "r");
	if (fp) {
		char tmp[1000];
		while (!feof(fp) && (memtotal < 0 || memfree < 0)) {
			fgets(tmp, sizeof(tmp), fp);
			if (strncmp(tmp, "MemTotal:", 9) == 0) {
				memtotal = atoi(tmp + 9);
			} else if (strncmp(tmp, "MemFree:", 8) == 0) {
				memfree = atoi(tmp + 8);
			}
		}
		fclose(fp);
	}

	if (memtotal > 0) {
		double percent = (memtotal - memfree) * 100.0 / memtotal;
		return percent;
	}
	return 0;
}

#elif defined(Darwin)

#include <mach/mach_init.h>
#include <mach/host_info.h>
#include <mach/mach_host.h>

double memory_used_ratio()
{
	vm_statistics_data_t page_info;
	vm_size_t pagesize;
	mach_msg_type_number_t count;
	kern_return_t kret;

	pagesize = 0;
	kret = host_page_size(mach_host_self(), &pagesize);

	count = HOST_VM_INFO_COUNT;
	kret = host_statistics(mach_host_self(), HOST_VM_INFO, (client_data_t)&page_info, &count);

	if (kret == KERN_SUCCESS){
		unsigned int pw, pa, pi, pf;

		pw = page_info.wire_count;
		pa = page_info.active_count;
		pi = page_info.inactive_count;
		pf = page_info.free_count;

		unsigned int totalpages = pw + pa + pi + pf;

		if (totalpages > 0) {
			return (pw + pa) * 100.0 / totalpages;
		}
	}
	return 0;
}

#endif
