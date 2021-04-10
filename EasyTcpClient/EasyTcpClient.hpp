#ifndef _EASYTCPCLIENT_HPP_
#define _EASYTCPCLIENT_HPP_

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#define _WINSOCK_DEPRECATED_NO_WARNINGS
	#define _CRT_SECURE_NO_WARNINGS
	#include <WinSock2.h>
	#include <Windows.h>
#pragma comment(lib,"ws2_32.lib")
#else
	#include <unistd.h>
	#include <arpa/inet.h>
	#include <string>

	#define SOCKET int
	#define INVALID_SOCKET (SOCKET)(~0)
	#define INVALID_ERROR (-1)
#endif // _WIN32

#include <iostream>
#include "MessageHeader.hpp"

class EasyTcpClient
{
	SOCKET _sock;
private:

public:
	EasyTcpClient()
	{
		_sock = INVALID_SOCKET;
	}
	virtual ~EasyTcpClient()
	{
		closesocket(_sock);
	}

	//初始化socket
	void InitSocket()
	{
#ifdef _WIN32
		//建立连接
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif
		if (INVALID_SOCKET != _sock)
		{
			std::cout << "< "<<_sock <<" >关闭旧连接..." << std::endl;
			Close();
		}
		//绑定端口
		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock)
		{
			std::cout << "建立socket失败..." << std::endl;
		}
		else
		{
			std::cout << "建立socket成功..." << std::endl;
		}
	}

	//连接服务器
	int Connect(const char* ip,unsigned short port)
	{
		if (INVALID_SOCKET == _sock)
		{
			InitSocket();
		}
		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET;
		_sin.sin_port = htons(port);

#ifdef _WIN32
		_sin.sin_addr.S_un.S_addr = inet_addr(ip);
#else
		_sin.sin_addr.s_addr = inet_addr(ip);
#endif // _WIN32

		int ret = connect(_sock, (sockaddr*)&_sin, sizeof(sockaddr_in));
		if (-1 == ret)
		{
			std::cout << "连接失败..." << std::endl;
		}
		else
		{
			std::cout << "连接成功..." << std::endl;
		}
		return ret;
	}

	//关闭socket
	void Close()
	{
		if (_sock != INVALID_SOCKET)
		{
#ifdef _WIN32
			closesocket(_sock);
			WSACleanup();
#else
			close(_sock);
#endif // _WIN32
			_sock = INVALID_SOCKET;
		}

	}

	//处理网络消息
	bool OnRun()
	{
		if (isRun())
		{
			fd_set fdRead;
			FD_ZERO(&fdRead);
			FD_SET(_sock, &fdRead);
			timeval t = { 0,0 };
			int ret = (int)select(_sock + 1, &fdRead, NULL, NULL, &t);
			if (ret < 0)
			{
				std::cout << "< " << _sock << " >select 任务结束1..." << std::endl;
				Close();
				return false;
			}

			if (FD_ISSET(_sock, &fdRead))
			{
				FD_CLR(_sock, &fdRead);

				if (-1 == RecvData(_sock))
				{
					std::cout << "< " << _sock << " >select 任务结束2..." << std::endl;
					Close();
					return false;
				}
			}
			return true;
		}
		return false;
	}

	//是否工作中
	bool isRun()
	{
		return _sock != INVALID_SOCKET;
	}

	//缓冲区最小单元
#define RECV_BUFF_SIZE 1024
	//接收缓冲区
	char _szRecv[RECV_BUFF_SIZE] = {};
	//第二缓冲区 消息缓冲区
	char _szMsgBuf[RECV_BUFF_SIZE*10] = {};
	//消息缓冲区尾部位置
	int _lastPos = 0;
	//接收数据 处理粘包 拆分包
	int RecvData(SOCKET cSock)
	{
		//接收数据   
		int nlen = (int)recv(cSock, _szRecv, RECV_BUFF_SIZE, 0);
		if (nlen <= 0)
		{
			std::cout <<"<"<< cSock << ">与服务器断开连接... " <<  std::endl;
			return -1;
		}

		//将接收的数据拷贝到消息缓冲区
		memcpy(_szMsgBuf+_lastPos, _szRecv, nlen);
		//消息缓冲区的数据尾部位置后移
		_lastPos += nlen;
		//判断消息缓冲区的数据长度大于消息头DataHeader长度
		//这时就可以知道当前消息体的长度
		while (_lastPos >= sizeof(DataHeader))
		{
			DataHeader* header = (DataHeader*)_szMsgBuf;
			//判断消息缓冲区的数据长度大于消息长度
			if (_lastPos >= header->dataLength)
			{
				//消息缓冲区剩余未处理数据的长度
				int nSize = _lastPos - header->dataLength;
				OnNetMsg(header);
				//将消息缓冲区剩余未处理数据前移
				memcpy(_szMsgBuf, _szMsgBuf + header->dataLength, nSize);
				//消息缓冲区的数据尾部位置前移
				_lastPos = nSize;
			}
			else
			{
				//消息缓冲区剩余不够一条完整消息
				break;
			}
		}
		return 0;
	}

	//响应网络消息
	void OnNetMsg(DataHeader* header)
	{
		switch (header->cmd)
		{
		case CMD_LOGRESULT:
		{
			LogResult* logresult = (LogResult*)header;
			std::cout << "服务器数据: CMD_LOGRESULT,数据长度 :" << header->dataLength << std::endl;
		}
		break;
		case CMD_LOGOUTRES:
		{
			LogOutRes* lotoutres = (LogOutRes*)header;
			std::cout << "服务器数据: CMD_LOGOUTRES,数据长度 :" << header->dataLength << std::endl;
		}
		break;
		case CMD_NEW_USER_JOIN:
		{
			NewUserJoin* newuserjoin = (NewUserJoin*)header;
			std::cout << "服务器数据: CMD_NEW_USER_JOIN,数据长度 :" << header->dataLength << std::endl;
		}
		break;
		case CMD_ERROR:
		{
			std::cout << "接收到服务器数据: CMD_ERROR,数据长度 :" << header->dataLength << std::endl;
		}
		break;
		default:
		{
			std::cout << "接收到未定义消息: <"<<_sock<<">数据长度 :" << header->dataLength << std::endl;
		}
		}
	}

	//发送数据
	int SendData(DataHeader* header, int nLen)
	{
		if (isRun() && header)
		{
			return send(_sock, (const char*)header, nLen, 0);
		}
		return SOCKET_ERROR;
	}
private:

};
#endif // !_EASYTCPCLIENT_HPP_
