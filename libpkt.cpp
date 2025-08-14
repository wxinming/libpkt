#include "libpkt.h"
#include <mutex>

#define CRC_BYTE_SIZE 4

static void nwbcpy(void* dst, const void* src, size_t size, int order)
{
    if (!dst || !src || !size || dst == src) {
		return;
	}

    uint16_t value = 0x1234;
    auto byte = *reinterpret_cast<uint8_t*>(&value);
	if (order == BIG_ENDIAN_MODE) {
        if (byte != 0x12) {
            for (size_t i = 0; i < size; ++i) {
                reinterpret_cast<uint8_t*>(dst)[i] =
                    reinterpret_cast<const uint8_t*>(src)[size - i - 1];
            }
        }
        else {
            memcpy(dst, src, size);
        }
	}
	else {
        if (byte != 0x12) {
            memcpy(dst, src, size);
        }
        else {
            for (size_t i = 0; i < size; ++i) {
                reinterpret_cast<uint8_t*>(dst)[i] =
                    reinterpret_cast<const uint8_t*>(src)[size - i - 1];
            }
        }
	}
}

static const uint32_t g_crc32Table[256] =
{
	0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,
	0x9E6495A3,0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,0x09B64C2B,0x7EB17CBD,
	0xE7B82D07,0x90BF1D91,0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,
	0x6DDDE4EB,0xF4D4B551,0x83D385C7,0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,
	0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,0x3B6E20C8,0x4C69105E,0xD56041E4,
	0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,0x35B5A8FA,0x42B2986C,
	0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,0x26D930AC,
	0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,
	0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,0x2F6F7C87,0x58684C11,0xC1611DAB,
	0xB6662D3D,0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,
	0x9FBFE4A5,0xE8B8D433,0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,
	0x086D3D2D,0x91646C97,0xE6635C01,0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,
	0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,0x65B0D9C6,0x12B7E950,0x8BBEB8EA,
	0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,0x4DB26158,0x3AB551CE,
	0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,0x4369E96A,
	0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,
	0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,
	0xCE61E49F,0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,
	0xB7BD5C3B,0xC0BA6CAD,0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,0xEAD54739,
	0x9DD277AF,0x04DB2615,0x73DC1683,0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,
	0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,0xF00F9344,0x8708A3D2,0x1E01F268,
	0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,0xFED41B76,0x89D32BE0,
	0x10DA7A5A,0x67DD4ACC,0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,0xD6D6A3E8,
	0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
	0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,0x316E8EEF,
	0x4669BE79,0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,0xCC0C7795,0xBB0B4703,
	0x220216B9,0x5505262F,0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB36A04,0xC2D7FFA7,
	0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,
	0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,0x95BF4A82,0xE2B87A14,0x7BB12BAE,
	0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,0x86D3D2D4,0xF1D4E242,
	0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,0x88085AE6,
	0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
	0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,0xA7672661,0xD06016F7,0x4969474D,
	0x3E6E77DB,0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,
	0x47B2CF7F,0x30B5FFE9,0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,
	0xCDD70693,0x54DE5729,0x23D967BF,0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,
	0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D
};

static uint32_t getCrc(const uint8_t* data, uint32_t size, uint32_t key)
{
	uint32_t crc32val = key ^ 0xFFFFFFFFU;
	for (uint32_t i = 0; i < size; i++) {
		crc32val = g_crc32Table[(crc32val ^ data[i]) & 0xff] ^ (crc32val >> 8);
	}
	return crc32val ^ 0xFFFFFFFFU;
}

static bool crcIsOk(const uint8_t* data, uint32_t size, uint32_t key, int order)
{
	if ((int)size - 4 < 0) {
		return false;
	}

	uint32_t crc0 = getCrc(data, size - CRC_BYTE_SIZE, key);
	uint32_t crc1 = 0;
	nwbcpy(&crc1, data + size - CRC_BYTE_SIZE, CRC_BYTE_SIZE, order);
	return crc0 == crc1;
}

Packet::Packet(int byteOrder, uint32_t headerData, int headerByte, int lengthByte, int commandByte, uint32_t crcKey)
    :byte_order_(byteOrder), header_data_(headerData), header_byte_(headerByte), length_byte_(lengthByte),
    command_byte_(commandByte), crc_key_(crcKey)
{

}

Packet::~Packet()
{

}

bool Packet::serialize()
{
    auto& ctx = getCtx();
	uint32_t dataLength = command_byte_ + ctx.copyLength + CRC_BYTE_SIZE;
	uint32_t bufferLength = header_byte_ + length_byte_ + dataLength;
	uint8_t* bufferData = new uint8_t[bufferLength];
	uint32_t size = 0;

	nwbcpy(&bufferData[size], &header_data_, header_byte_, byte_order_);
	size += header_byte_;

	nwbcpy(&bufferData[size], &dataLength, length_byte_, byte_order_);
	size += length_byte_;

	nwbcpy(&bufferData[size], &ctx.command, command_byte_, byte_order_);
	size += command_byte_;

	if (ctx.copyLength != 0) {
		memcpy(&bufferData[size], ctx.copyData, ctx.copyLength);
		size += ctx.copyLength;
	}

	uint32_t crc = getCrc(bufferData, size, crc_key_);
	nwbcpy(&bufferData[size], &crc, CRC_BYTE_SIZE, byte_order_);
	size += CRC_BYTE_SIZE;

	if (ctx.data) {
		delete[] ctx.data;
	}

	ctx.data = bufferData;
	ctx.dataLength = bufferLength;
    ctx.payload = bufferData + header_byte_ + length_byte_ + command_byte_;
    ctx.payloadLength = bufferLength - header_byte_ - length_byte_ - command_byte_ - CRC_BYTE_SIZE;

    if (ctx.copyData) {
		delete[] ctx.copyData;
		ctx.copyData = nullptr;
	}
	ctx.copyLength = 0;
	ctx.tempLength = 0;
    ctx.fetchOffset = 0;
	return true;
}

bool Packet::unserialize()
{
    auto& ctx = getCtx();
	if (!ctx.copyData && !ctx.data) {
		return false;
	}

	if (!ctx.copyData && ctx.data) {
		ctx.copyData = new uint8_t[ctx.dataLength];
		memcpy(ctx.copyData, ctx.data, ctx.dataLength);
		ctx.copyLength = ctx.dataLength;
	}

	if (!crcIsOk(ctx.copyData, ctx.copyLength, crc_key_, byte_order_)) {
		return false;
	}

	uint32_t size = 0;
	size += header_byte_;
	uint32_t bufferLength = 0;

	nwbcpy(&bufferLength, &ctx.copyData[size], length_byte_, byte_order_);
	size += length_byte_;
	ctx.command = 0;

	nwbcpy(&ctx.command, &ctx.copyData[size], command_byte_, byte_order_);

	size += command_byte_;
	bufferLength -= command_byte_;
	bufferLength -= CRC_BYTE_SIZE;

	uint8_t* bufferData = nullptr;
	if (bufferLength != 0) {
		bufferData = new uint8_t[bufferLength];
		memcpy(bufferData, &ctx.copyData[size], bufferLength);
	}

	if (ctx.data) {
		delete[] ctx.data;
	}
	ctx.data = bufferData;
	ctx.dataLength = bufferLength;
	ctx.fetchRemain = bufferLength;
    ctx.payload = bufferData + header_byte_ + length_byte_ + command_byte_;
    ctx.payloadLength = bufferLength - header_byte_ - length_byte_ - command_byte_ - CRC_BYTE_SIZE;

	if (ctx.copyData) {
		delete[] ctx.copyData;
		ctx.copyData = nullptr;
	}
	ctx.copyLength = 0;
	ctx.tempLength = 0;
    ctx.fetchOffset = 0;
	return true;
}

void Packet::set_command(uint16_t command)
{
    auto& ctx = getCtx();
	ctx.command = command;
}

void Packet::set_data(const void* data, uint32_t length)
{
    auto& ctx = getCtx();
	if (ctx.copyData) {
		delete[] ctx.copyData;
		ctx.copyData = nullptr;
	}

	ctx.copyData = new uint8_t[length];
	memcpy(ctx.copyData, data, length);
	ctx.copyLength = length;
}

void* Packet::fetch_payload(uint32_t bytes)
{
    auto& ctx = getCtx();
	if (bytes == static_cast<uint32_t>(-1)) {
		bytes = ctx.dataLength - ctx.fetchOffset;
	}
	ctx.fetchBytes = bytes;

	if (bytes == 0 ||
		bytes > ctx.dataLength ||
		ctx.fetchOffset + bytes > ctx.dataLength) {
		return nullptr;
	}

	if (!ctx.fetchData) {
	RENEW:
		ctx.fetchLength += bytes + 1024 * 1024;
		ctx.fetchData = new uint8_t[ctx.fetchLength];
		memset(ctx.fetchData, 0, ctx.fetchLength);
	}
	else if (ctx.fetchLength < bytes) {
		delete[] ctx.fetchData;
		goto RENEW;
	}

	void* data = ctx.data + ctx.fetchOffset;
	memset(ctx.fetchData, 0, ctx.fetchLength);
	memcpy(ctx.fetchData, data, bytes);
	ctx.fetchOffset += bytes;
	ctx.fetchRemain = ctx.dataLength - ctx.fetchOffset;
	return ctx.fetchData;
}

Packet& Packet::append_payload(const void* data, uint32_t length)
{
	if (!data || !length) {
		return *this;
	}

    auto& ctx = getCtx();
	if (!ctx.copyData) {
		ctx.tempLength += length + 1024 * 1024;
		ctx.copyData = new uint8_t[ctx.tempLength];
		memset(ctx.copyData, 0, ctx.tempLength);
	}

	if (ctx.copyLength + length > ctx.tempLength) {
		ctx.tempLength += ctx.copyLength + length;
		uint8_t* tempData = new uint8_t[ctx.copyLength];
		memset(tempData, 0, ctx.copyLength);
		memcpy(tempData, ctx.copyData, ctx.copyLength);
		delete[] ctx.copyData;
		ctx.copyData = new uint8_t[ctx.tempLength];
		memcpy(ctx.copyData, tempData, ctx.copyLength);
		delete[] tempData;
	}
	memcpy(ctx.copyData + ctx.copyLength, data, length);
	ctx.copyLength += length;

	return *this;
}

void Packet::remove_payload(int index, uint32_t length)
{
    auto& ctx = getCtx();
	if (index < 0 || !length || !ctx.copyData) {
		return;
	}

	//[0 1 2 3 4 5 6 7 8 9]
	//index = 8,length = 4
	if (index + length > ctx.copyLength) {
		length = ctx.copyLength - index;
	}

	//[0 1 2 3 4 5 6 7 8 9]
	//index = 2 length = 3
	//[0 1 5 6 7 8 9]
	ctx.copyLength -= length;
	for (int j = index; j < ctx.copyLength; ++j) {
		ctx.copyData[j] = ctx.copyData[j + length];
	}
}

void Packet::insert_payload(int index, const void* data, uint32_t length)
{
	if (index < 0 || !data || !length) {
		return;
	}

    auto& ctx = getCtx();
	if (!ctx.copyData) {
		ctx.tempLength += length + 1024 * 1024;
		ctx.copyData = new uint8_t[ctx.tempLength];
		memset(ctx.copyData, 0, ctx.tempLength);
	}

	if (ctx.copyLength + length > ctx.tempLength) {
		ctx.tempLength += ctx.copyLength + length;
		uint8_t* tempData = new uint8_t[ctx.copyLength];
		memset(tempData, 0, ctx.copyLength);
		memcpy(tempData, ctx.copyData, ctx.copyLength);

		delete[] ctx.copyData;
		ctx.copyData = new uint8_t[ctx.tempLength];
		memset(ctx.copyData, 0, ctx.tempLength);
		memcpy(ctx.copyData, tempData, ctx.copyLength);
		delete[] tempData;
	}
	//[0 1       5 6 7 8 9]
	//index = 2 data = {2 3 4} length = 3
	//[0 1 2 3 4 5 6 7 8 9]
	//[f f f f 1 2 3 4 5 6 7 8 9]
	//for (int j = index; j < ctx.copyLength; ++j)
	//{
	//	ctx.copyData[j + length] = ctx.copyData[j];
	//}
	for (int j = ctx.copyLength + length - 1; j >= index; --j) {
		ctx.copyData[j] = ctx.copyData[j - length];
	}
	memcpy(ctx.copyData + index, data, length);
	ctx.copyLength += length;
}

void Packet::release_data()
{
    auto& ctx = getCtx();
	if (ctx.copyData) {
		delete[] ctx.copyData;
		ctx.copyData = nullptr;
	}
	ctx.copyLength = 0;
	ctx.tempLength = 0;
    ctx.fetchOffset = 0;
}

Packet::Ctx& Packet::getCtx() const
{
    std::lock_guard lock(mutex_);
    return ctx_[std::this_thread::get_id()];
}

Packet::Ctx::Ctx(){
    command = 0;
    data = nullptr;
    dataLength = 0;
    copyData = nullptr;
    copyLength = 0;
    tempLength = 0;
    fetchData = nullptr;
    fetchLength = 0;
    fetchOffset = 0;
    fetchBytes = 0;
    fetchRemain = 0;
    payload = nullptr;
    payloadLength = 0;
}

Packet::Ctx::~Ctx()
{
    if (copyData) {
        delete[] copyData;
    }

    if (data) {
        delete[] data;
    }

    if (fetchData) {
        delete[] fetchData;
    }
}
