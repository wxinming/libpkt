#ifndef __PACKET__
#define __PACKET__

#include <cstdint>
#include <stdint.h>
#include <map>
#include <string>
#include <thread>
#include <mutex>
#include <iostream>
#include "json.hpp"

#define BIG_ENDIAN_MODE 0
#define LITTLE_ENDIAN_MODE 1

/*
* @brief 数据包
* @note
* [h_0 h_1 len_0 len_1 len_2 len_3 (cmd_0 cmd_1) (data_n) crc_0 crc_1 crc_2 crc_3]
* header maximum 4 bytes uint32_t
* length maximum 4 bytes uint32_t
* command maximum 2 bytes uint16_t, can be zero byte, it's not used!
* data n bytes, can be null
* crc fixed size 4 bytes uint32_t
*/

class Packet
{
public:
	/*
	* @brief 初始化
	* @param[in] byteOrder 字节序
	* @param[in] headerData 头部数据
	* @param[in] headerByte 头部字节
	* @param[in] lengthByte 长度字节
	* @param[in] commandByte 命令字节
	* @param[in] crcKey crc密钥
	* @return void
	*/
    Packet(int byteOrder, uint32_t headerData, int headerByte, int lengthByte = 4, int commandByte = 2, uint32_t crcKey = 0);

	/*
	* @brief 析构
	*/
	~Packet();

	/*
	* @brief 序列化
	* @retval true 成功
	* @retval false 失败
	*/
	bool serialize();

	/*
	* @brief 反序列化
	* @retval true 成功
	* @retval false 失败
	*/
	bool unserialize();

	/*
	* @brief 设置命令
	* @param[in] command 命令
	* @return void
	*/
	void set_command(uint16_t command);

	/*
	* @brief 设置数据
	* @param[in] data 数据
	* @param[in] length 长度
	* @return void
	*/
	void set_data(const void* data, uint32_t length);

	/*
	* @brief 提取数据
	* @param[in] bytes 多少个字节
	* @return 数据的首地址
	*/
	void* fetch_payload(uint32_t bytes = -1);

	/*
	* @brief 提取数据
	* @return 模板数据
	* @note 数据大小根据模板类型定义
	*/
	template<class T,
		std::enable_if_t <
		std::is_integral_v<T> ||
		std::is_same_v<T, std::string> ||
		std::is_same_v<T, nlohmann::json>,
		int> = 0>
	T fetch_payload() {
		if constexpr (std::is_integral_v<T>) {
			void* data = fetch_payload(sizeof(T));
			return *reinterpret_cast<T*>(data);
		}
		else if constexpr (std::is_same_v<T, std::string>) {
			auto length = fetch_payload<uint32_t>();
			return std::string(static_cast<const char*>(fetch_payload(length)), length);
		}
		else {
			nlohmann::json json;
            std::string err;
            if (!fetch_payload(json, &err)) {
                std::cerr << err << std::endl;
            }
			return json;
		}
	}

	/*
	* @brief 提取数据
	* @tparam[out] json json数据
	* @param[out] error 错误信息
	* @return bool
	*/
	template<class T,
		std::enable_if_t<
		std::is_same_v<T, nlohmann::json>, int> = 0>
	bool fetch_payload(T & json, std::string* error = nullptr)
	{
		bool result = false;
		do {
			if (remain_bytes() < sizeof(uint32_t)) {
				if (error) {
					*error = "字节大小错误";
				}
				break;
			}

			auto dump = fetch_payload<std::string>();
			try {
				json = nlohmann::json::parse(dump);
			}
			catch (const std::exception& e) {
				json = nlohmann::json();
				if (error) {
					*error = e.what();
				}
				break;
			}
			result = true;
		} while (false);
		return result;
	}

	/*
	* @brief 获取剩余多少个字节
	* @return 剩余字节数
	*/
	uint32_t remain_bytes() const { return getCtx().fetchRemain; }

	/*
	* @brief 追加数据
	* @param[in] data 数据
	* @param[in] length 长度
	* @return Packet&
	*/
	Packet& append_payload(const void* data, uint32_t length);

	/*
	* @brief 追加数据
	* @param[in] data 数据
	* @return Packet&
	* @note 数据大小取决于数据类型
	*/
	template<class T, 
        std::enable_if_t<std::is_integral_v<T> ||
		std::is_same_v<T, std::string> ||
		std::is_same_v<T, nlohmann::json>, int> = 0>
	Packet & append_payload(const T & data) {
		if constexpr (std::is_integral_v<T>) {
			return append_payload(&data, sizeof(T));
		}
        else if constexpr (std::is_same_v<T, std::string>) {
            return append_payload(static_cast<uint32_t>(data.length())).append_payload(data.c_str(), data.length());
		}
		else if constexpr (std::is_same_v<T, nlohmann::json>) {
			auto dump = data.dump();
			return append_payload(dump);
		}
	}

	/*
	* @brief 移除数据
	* @param[in] index 索引(从0开始)
	* @param[in] length 移除的长度
	* @return void
	*/
	void remove_payload(int index, uint32_t length);

	/*
	* @brief 插入数据
	* @param[in] index 索引(从0开始)
	* @parma[in] data 插入的数据地址
	* @param[in] length 插入的长度
	* @return void
	*/
	void insert_payload(int index, const void* data, uint32_t length);

	/*
	* @brief 插入数据
	* @param[in] index 索引(从0开始)
	* @param[in] data 插入的数据地址
	* @return 模板数据
	* @note 插入的大小取决于模板数据类型
	*/
	template<class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
        void insert_payload(int index, const T* data) { 
            insert_data(index, data, sizeof(T)); }

	/*
	* @brief 释放数据
	* @return void
	*/
	void release_data();

	/*
	* @brief 获取数据
	* @return 数据
	*/
	inline uint8_t* data() const { return getCtx().data; }

	/*
	* @brief 获取大小
	* @return 大小
	*/
	inline uint32_t data_length() const { return getCtx().dataLength; }

	/*
	* @brief 获取命令
	* @return 命令
	*/
	inline uint16_t command() const { return getCtx().command; }

	/*
	* @brief 获取数据载荷
	* @return 数据载荷
	*/
    inline uint8_t* payload() const { 
        return getCtx().payload;
    }

	/*
	* @brief 获取数据载荷长度
	* @return 数据载荷长度
	*/
    inline uint32_t payload_length() const {
        return getCtx().payloadLength;
    }

protected:
    struct Ctx {
        Ctx();
        ~Ctx();
        uint16_t command;
        uint8_t* data;
        uint32_t dataLength;
	    uint8_t* copyData;
	    uint32_t copyLength;
	    uint32_t tempLength;
	    uint8_t* fetchData;
	    uint32_t fetchLength;
	    uint32_t fetchOffset;
	    uint32_t fetchBytes;
	    uint32_t fetchRemain;
        uint8_t* payload;
        uint32_t payloadLength;
    };
    Ctx& getCtx() const;

private:
	const int byte_order_;
	const int header_byte_;
	const int length_byte_;
	const int command_byte_;
	const uint32_t header_data_;
	const uint32_t crc_key_;
    mutable std::map<std::thread::id, Ctx> ctx_;
    mutable std::mutex mutex_;
};

#endif
