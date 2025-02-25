#pragma once
#include <stdint.h>
namespace SimpleHTTP{
	class CBuffer
	{
	public:
		CBuffer(char* buf,int bufferSize);
		/**
		 * gets data from the buffer returning true on error (not enough space)
		 */
		bool get(char *buffer, uint32_t len);
		/**
		 * puts data on the buffer returning true on error (not enough space)
		 */
		bool put(char *buffer, uint32_t len);

		/**
		 * returns the freespace athe head side of the buffer
		 */
		uint32_t freeSpace();
		/**
		 * returns the number of bytes ready for read
		 */
		uint32_t backLogSize();

		/**
		read an int8_t from the buffer;
		**/
		void operator>>(uint8_t& val);
		/**
		read an int16_t from the buffer;
		**/
		void operator>>(uint16_t& val);
		/**
		read an unsigned int32_t from the buffer;
		**/
		void operator>>(uint32_t& val);
		/**
		read a signed int32_t from the buffer;
		**/
		void operator>>(int32_t& val);
		/**
		 * read an int16_t from the buffer as LittleEndian
		 **/
		void readInt16LittleEndian(uint16_t& val);
		/**
		 * returns the next byte for read without moving the pointer
		 */
		char peek();
		/*
		* moves the pointer without copying any data
		*/
		bool discard(uint16_t size);

		void markTail();

		void resetTail();

		void reset();
	private:
		static const int bufferSize = 2*2*96*96;//8ms of 24bit audio at 96k
		int packerCount;
		char* buffer;
		char *bufferEnd;
		char *head;
		char *tail;
		char *tailMarker;
	};
};

