
#ifndef __widestringwriter_h
#define __widestringwriter_h

#include <vector>
#include <string>

class widestringwriter : public soramimi::jcode::wcswriter {
private:
	std::vector<uchar_t> buffer;
public:
	widestringwriter()
	{
		buffer.reserve(1024);
	}
	virtual void put(uchar_t c)
	{
		buffer.push_back(c);
	}
	ustring str() const
	{
		if (buffer.empty()) {
			return ustring();
		}
		return ustring(&buffer[0], buffer.size());
	}
	void clear()
	{
		buffer.clear();
		buffer.reserve(1024);
	}
};

#endif
