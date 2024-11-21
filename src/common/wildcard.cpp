
#include <vector>
#include "uchar.h"

struct state_t {
	size_t p;
	size_t s;
	state_t()
		: p(0)
		, s(0)
	{
	}
};

template <typename T> bool t_wildcard(T const *pattern, T const *string)
{
	if (!pattern) {
		return false;
	}
	if (!string) {
		return false;
	}
	std::vector<state_t> states;
	states.push_back(state_t());
	while (1) {
		size_t i = states.size();
		while (i > 0) {
			i--;
			T cp = pattern[states[i].p];
			T cs = string[states[i].s];
			if (cp == 0 || cs == 0) {
				if (cp == 0 && cs == 0) {
					return true;
				} else if (cp == '*') {
					states[i].p++;
				} else {
					states.erase(states.begin() + i);
				}
			} else if (cp == '?') {
				states[i].p++;
				states[i].s++;
			} else if (cp == '*') {
				states.push_back(state_t());
				states.back().p = states[i].p + 1;
				states.back().s = states[i].s + 1;
				states.push_back(state_t());
				states.back().p = states[i].p + 1;
				states.back().s = states[i].s;
				states[i].s++;
			} else {
				if (cp == cs) {
					states[i].p++;
					states[i].s++;
				} else {
					states.erase(states.begin() + i);
				}
			}
		}
		if (states.empty()) {
			return false;
		}
	}
}

bool wildcard(uchar_t const *pattern, uchar_t const *string)
{
	return t_wildcard(pattern, string);
}

bool wildcard(char const *pattern, char const *string)
{
	return t_wildcard(pattern, string);
}



