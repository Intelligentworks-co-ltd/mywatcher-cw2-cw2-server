
#ifndef __stringwriter_h
#define __stringwriter_h

#include <vector>
#include <string>

class stringwriter : public soramimi::jcode::writer {
private:
	std::vector<char> buffer;
public:
	stringwriter()
	{
		buffer.reserve(1024);
	}
	virtual void put(unsigned char c)
	{
		buffer.push_back(c);
	}
	std::string str() const
	{
		if (buffer.empty()) {
			return std::string();
		}
		return std::string(&buffer[0], buffer.size());
	}
	void clear()
	{
		buffer.clear();
		buffer.reserve(1024);
	}
};

#endif
