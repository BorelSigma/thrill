#pragma once
#include <stdint.h>
#include <iostream>

namespace GraphGen2
{

class BAHash
{


public:
	BAHash(){};
	//BAHash(uint64_t seed=1){};
	uint64_t operator()(const uint64_t &value) const
	{ 
		return (value*hash_multiplier_)*(1.0/0xffffffffffffffffLL) * value;
	}	
	//std::string getName() const{return name;};
private:
	constexpr static uint64_t hash_multiplier_=3141592653589793238LL;
	//const std::string name = "BAHash";
};

/* Auf Basis der deutschen Wikipedia */
class FNVHash
{


public:
	//FNVHash(uint64_t seed=1){};

	uint64_t operator()(const uint64_t &value) const
 	{
     		uint64_t        Hash       = 0xcbf29ce484222325;
 		uint8_t   	Buffer;
		int		shift = 0;
		
		for(uint8_t i=0;i<8;i++)
		{
			Buffer = value & 0xFF<<shift;
			shift += 8;

			Hash = (Hash ^ Buffer) * magic_prime_;
		}

		if(value > 0)
			Hash = Hash % value;

     		return Hash;
 	}

private:
	constexpr static uint64_t  magic_prime_ = 0x00000100000001b3;
	//const std::string name = "FNVHash";
	//std::string getName() const{return name;};
};

/* Die Basis ist aus der deutschen Wikipediaseite */
class CRC32Hash
{
private:
    	const uint64_t CRC32MASKREV_  = 0xEDB88320;
	//const std::string name = "CRC";

public:
	//CRC32Hash(uint64_t seed=1){};

	inline uint64_t crc32(const uint64_t &value) const
	{
		uint64_t crc32_rev   = ~0;
	
   	 	for (uint64_t i = 0; i < 32; i++)
    		{
        		if ((crc32_rev & 1) != ((value & 1) << i))
            			crc32_rev = (crc32_rev >> 1) ^ CRC32MASKREV_;
        		else
            			crc32_rev = (crc32_rev >> 1);
    		}

		return 	~crc32_rev;
	}	
	
	uint64_t operator()(const uint64_t &value) const
 	{
     		uint64_t lower = crc32(value & (((uint64_t)1<<32) -1));
		uint64_t higher = crc32(  value >> 32 );

		uint64_t Hash = (higher << 32) + lower;

		if(value > 0)
			Hash = Hash % value;

     		return Hash;
 	}	
};	

}
