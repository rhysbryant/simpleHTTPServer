#include "CBuffer.h"
#include <string.h>
#include <math.h>
using SimpleHTTP::CBuffer;
CBuffer::CBuffer(char* buf,int bufferSize){
	buffer = buf;
	bufferEnd = buffer + bufferSize - 1;
	head = buffer;
	tail = head;
	tailMarker = 0;
	memset(buffer,0,bufferSize);
}

bool CBuffer::get(char* buf,uint32_t len){
	if( backLogSize() < len){
		return true;
	}

	if( tail >= bufferEnd ){
		tail=buffer;
	}

	if( head > tail ){
		memcpy(buf,tail,len);
		tail += len;
	}else{
		uint32_t toEnd=(bufferEnd - tail);
		uint32_t size=len>toEnd?toEnd:len;

		memcpy(buf,tail,size);
		if( size < len ){
			uint32_t offset = size;
			size=len-size;
			memcpy(buf + offset,buffer,size);
			tail = buffer+size;
		}else{
			tail+=len;
		}


	}

	return false;
}

bool CBuffer::put(char* srcBuf,uint32_t len){
	if( freeSpace() < len){
		return true;
	}
	if(head >= bufferEnd){
		head=buffer;
	}

	if( head >= tail){
		uint32_t remaing=(bufferEnd - head);
		if( remaing > len){
			memcpy(head,srcBuf,len);

			head+= len;
		}else{
			memcpy(head,srcBuf,remaing);
			uint32_t wrapSize=len-remaing;
			memcpy(buffer,srcBuf+ remaing,wrapSize);
			head=buffer + wrapSize;
		}
	}else{
		memcpy(head,srcBuf,len);
		head+= len;
	}

	return false;
}

uint32_t CBuffer::freeSpace(){
	if (head >= tail){
		return (bufferEnd - head) + (tail - buffer);
	}else{
		return (tail - head);
	}
}

uint32_t CBuffer::backLogSize(){

	if( head >= tail){
		return head - tail;
	}else{
		return (bufferEnd - tail) + (head - buffer);
	}
}

bool CBuffer::discard(uint16_t size){
	if( backLogSize() < size){
		return true;
	}
	uint32_t remaing=bufferEnd-tail;

	if( tail + size > bufferEnd){
		tail = buffer+(size-remaing);
	}
	else{
		tail += size;
	}
	return false;
}

char CBuffer::peek(){
	if( tail > bufferEnd){
		return buffer[0];
	}
	else{
		return tail[0];
	}
}


void CBuffer::operator>>(uint8_t & val)
{
	get((char*)&val, 1);
	return;
	if( tail >= bufferEnd){
		char b= buffer[0];
		tail=buffer+1;
		val=b;
	}
	else{
		val=tail[0];
		tail++;
	}

}


void CBuffer::markTail(){
	tailMarker=tail;
}

void CBuffer::resetTail(){
	tail=tailMarker;
}

void CBuffer::reset(){
	tail=buffer;
	head=buffer;
}

