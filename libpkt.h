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
* @brief ���ݰ�
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
	* @brief ��ʼ��
	* @param[in] byteOrder �ֽ���
	* @param[in] headerData ͷ������
	* @param[in] headerByte ͷ���ֽ�
	* @param[in] lengthByte �����ֽ�
	* @param[in] commandByte �����ֽ�
	* @param[in] crcKey crc��Կ
	* @return void
	*/
    Packet(int byteOrder, uint32_t headerData, int headerByte, int lengthByte = 4, int commandByte = 2, uint32_t crcKey = 0);

	/*
	* @brief ����
	*/
	~Packet();

	/*
	* @brief ���л�
	* @retval true �ɹ�
	* @retval false ʧ��
	*/
	bool serialize();

	/*
	* @brief �����л�
	* @retval true �ɹ�
	* @retval false ʧ��
	*/
	bool unserialize();

	/*
	* @brief ��������
	* @param[in] command ����
	* @return void
	*/
	void set_command(uint16_t command);

	/*
	* @brief ��������
	* @param[in] data ����
	* @param[in] length ����
	* @return void
	*/
	void set_data(const void* data, uint32_t length);

	/*
	* @brief ��ȡ����
	* @param[in] bytes ���ٸ��ֽ�
	* @return ���ݵ��׵�ַ
	*/
	void* fetch_payload(uint32_t bytes = -1);

	/*
	* @brief ��ȡ����
	* @return ģ������
	* @note ���ݴ�С����ģ�����Ͷ���
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
	* @brief ��ȡ����
	* @tparam[out] json json����
	* @param[out] error ������Ϣ
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
					*error = "�ֽڴ�С����";
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
	* @brief ��ȡʣ����ٸ��ֽ�
	* @return ʣ���ֽ���
	*/
	uint32_t remain_bytes() const { return getCtx().fetchRemain; }

	/*
	* @brief ׷������
	* @param[in] data ����
	* @param[in] length ����
	* @return Packet&
	*/
	Packet& append_payload(const void* data, uint32_t length);

	/*
	* @brief ׷������
	* @param[in] data ����
	* @return Packet&
	* @note ���ݴ�Сȡ������������
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
	* @brief �Ƴ�����
	* @param[in] index ����(��0��ʼ)
	* @param[in] length �Ƴ��ĳ���
	* @return void
	*/
	void remove_payload(int index, uint32_t length);

	/*
	* @brief ��������
	* @param[in] index ����(��0��ʼ)
	* @parma[in] data ��������ݵ�ַ
	* @param[in] length ����ĳ���
	* @return void
	*/
	void insert_payload(int index, const void* data, uint32_t length);

	/*
	* @brief ��������
	* @param[in] index ����(��0��ʼ)
	* @param[in] data ��������ݵ�ַ
	* @return ģ������
	* @note ����Ĵ�Сȡ����ģ����������
	*/
	template<class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
        void insert_payload(int index, const T* data) { 
            insert_data(index, data, sizeof(T)); }

	/*
	* @brief �ͷ�����
	* @return void
	*/
	void release_data();

	/*
	* @brief ��ȡ����
	* @return ����
	*/
	inline uint8_t* data() const { return getCtx().data; }

	/*
	* @brief ��ȡ��С
	* @return ��С
	*/
	inline uint32_t data_length() const { return getCtx().dataLength; }

	/*
	* @brief ��ȡ����
	* @return ����
	*/
	inline uint16_t command() const { return getCtx().command; }

	/*
	* @brief ��ȡ�����غ�
	* @return �����غ�
	*/
    inline uint8_t* payload() const { 
        return getCtx().payload;
    }

	/*
	* @brief ��ȡ�����غɳ���
	* @return �����غɳ���
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
