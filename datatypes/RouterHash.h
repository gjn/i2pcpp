#ifndef ROUTERHASH_H
#define ROUTERHASH_H

#include <array>
#include <string>
#include <functional>
#include <iostream>

#include "ByteArray.h"

namespace i2pcpp {

	class RouterHash : public std::array<unsigned char, 32> {
		public:
			RouterHash();
			RouterHash(ByteArray const &b);
			RouterHash(std::string const &s);

			operator ByteArray() const;
			operator std::string() const;
	};

	std::ostream& operator<<(std::ostream &s, RouterHash const &rh);
}
#ifdef USE_CLANG
template <>
struct std::hash<i2pcpp::RouterHash>
#else
namespace std { 
template <>
struct hash<i2pcpp::RouterHash> 
#endif
{
	public:
		size_t operator()(const i2pcpp::RouterHash &rh) const
		{
			std::hash<std::string> f;
			return f(std::string(rh.cbegin(), rh.cend()));
		}
};
#ifndef USE_CLANG
}
#endif
#endif
