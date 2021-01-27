
#include "cpustat.h"

#ifdef WIN32

#pragma comment(lib, "pdh.lib")

bool ProcessorLoad_::open()
{
	wchar_t const *path = L"\\Processor(_Total)\\% Processor Time";

	HQUERY hQuery;
	HCOUNTER hCounter;

	PDH_STATUS r;
	r = PdhOpenQuery(NULL, 0, &hQuery);
	if (r != ERROR_SUCCESS) {
		return false;
	}
	r = PdhAddCounter(hQuery, path, 0, &hCounter);
	if (r != ERROR_SUCCESS) {
		return false;
	}
	r = PdhCollectQueryData(hQuery);
	if (r != ERROR_SUCCESS) {
		return false;
	}
	r = PdhCollectQueryData(hQuery);
	if (r != ERROR_SUCCESS) {
		return false;
	}
	_hQuery = hQuery;
	_hCounter = hCounter;
	return true;
}

bool ProcessorLoad_::get(double *result)
{
	PDH_STATUS r;
	PDH_FMT_COUNTERVALUE fmtVal;
	r = PdhCollectQueryData(_hQuery);
	if (r != ERROR_SUCCESS) {
		return false;
	}
	r = PdhGetFormattedCounterValue(_hCounter, PDH_FMT_DOUBLE, NULL, &fmtVal);
	if (r != ERROR_SUCCESS) {
		return false;
	}
	*result = fmtVal.doubleValue;
	return true;
}

void ProcessorLoad_::close()
{
	PdhCloseQuery(_hQuery);
	_hQuery = 0;
	_hCounter = 0;
}


#elif defined(Linux)

#include <stdlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <ctype.h>

static char *skipspace(char *p)
{
	if (p) {
		while (isspace(*p)) {
			p++;
		}
	}
	return p;
}


void ProcessorLoad_::update()
{
	unsigned long long cpu_us;
	unsigned long long cpu_ni;
	unsigned long long cpu_sy;
	unsigned long long cpu_id;
	bool success = false;
	FILE *fp;
	fp = fopen("/proc/stat", "r");
	if (fp) {
		char tmp[1000];
		while (!feof(fp)) {
			fgets(tmp, sizeof(tmp), fp);
			if (memcmp(tmp, "cpu ", 4) == 0) {
				char *p = tmp + 4;
				p = skipspace(p);
				cpu_us = (unsigned long long)strtoll(p, &p, 10);
				p = skipspace(p);
				cpu_ni = (unsigned long long)strtoll(p, &p, 10);
				p = skipspace(p);
				cpu_sy = (unsigned long long)strtoll(p, &p, 10);
				p = skipspace(p);
				cpu_id = (unsigned long long)strtoll(p, &p, 10);
				success = true;
				break;
			}
		}
		fclose(fp);
	}

	if (success) {
		busy = cpu_us + cpu_ni + cpu_sy;
		idle = cpu_id;
	} else {
		busy = 0;
		idle = 0;
	}
}

bool ProcessorLoad_::open()
{
	update();
}

bool ProcessorLoad_::get(double *result)
{
	long long old_busy = busy;
	long long old_idle = idle;
	update();
	long long d = (busy + idle) - (old_busy + old_idle);
	if (d < 1) {
		return false;
	}
	*result = (busy - old_busy) * 100.0 / d;
	return true;
}

void ProcessorLoad_::close()
{
}

#elif defined(Darwin)

#include <mach/mach.h>
#include <mach/mach_host.h>

void ProcessorLoad_::update()
{
	unsigned long long cpu_us = 0;
	unsigned long long cpu_sy = 0;
	unsigned long long cpu_ni = 0;
	unsigned long long cpu_id = 0;
	bool success = false;
	{
		processor_cpu_load_info_t cpuinfo;
		natural_t cpucount;
		mach_msg_type_number_t msgcount;
		kern_return_t kr;
		kr = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &cpucount, (processor_info_array_t *)&cpuinfo, &msgcount);
		if (kr == KERN_SUCCESS) {
			for (int i = 0; i < cpucount; i++) {
				cpu_us += cpuinfo[i].cpu_ticks[CPU_STATE_USER];
				cpu_sy += cpuinfo[i].cpu_ticks[CPU_STATE_SYSTEM];
				cpu_ni += cpuinfo[i].cpu_ticks[CPU_STATE_NICE];
				cpu_id += cpuinfo[i].cpu_ticks[CPU_STATE_IDLE];
			}
			success = true;
		}
	}
	if (success) {
		busy = cpu_us + cpu_ni + cpu_sy;
		idle = cpu_id;
	} else {
		busy = 0;
		idle = 0;
	}
}

bool ProcessorLoad_::open()
{
	update();
}

bool ProcessorLoad_::get(double *result)
{
	unsigned long long old_busy = busy;
	unsigned long long old_idle = idle;
	update();
	long long d = (busy + idle) - (old_busy + old_idle);
	if (d < 1) {
		return false;
	}
	*result = (busy - old_busy) * 100.0 / d;
	return true;
}

void ProcessorLoad_::close()
{
}

#endif


