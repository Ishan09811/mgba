
#pragma once

#include <cstdint>
#include <string>

struct NoIntroMetadata {
	std::string name;
	std::string romName;
	uint64_t size = 0;
	uint32_t crc32 = 0;
	bool verified = false;
};

bool noIntroInit(const char* databasePath, const char* datPath);
void noIntroShutdown();

bool noIntroLookupCRC32(uint32_t crc32, NoIntroMetadata& out);
[[maybe_unused]] bool noIntroLookupMD5(const uint8_t md5[16], NoIntroMetadata& out);
[[maybe_unused]] bool noIntroLookupSHA1(const uint8_t sha1[20], NoIntroMetadata& out);