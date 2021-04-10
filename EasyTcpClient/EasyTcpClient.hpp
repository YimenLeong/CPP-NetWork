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

	//��ʼ��socket
	void InitSocket()
	{
#ifdef _WIN32
		//��������
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif
		if (INVALID_SOCKET != _sock)
		{
			std::cout << "< "<<_sock <<" >�رվ�����..." << std::endl;
			Close();
		}
		//�󶨶˿�
		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock)
		{
			std::cout << "����socketʧ��..." << std::endl;
		}
		else
		{
			std::cout << "����socket�ɹ�..." << std::endl;
		}
	}

	//���ӷ�����
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
			std::cout << "����ʧ��..." << std::endl;
		}
		else
		{
			std::cout << "���ӳɹ�..." << std::endl;
		}
		return ret;
	}

	//�ر�socket
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

	//����������Ϣ
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
				std::cout << "< " << _sock << " >select �������1..." << std::endl;
				Close();
				return false;
			}

			if (FD_ISSET(_sock, &fdRead))
			{
				FD_CLR(_sock, &fdRead);

				if (-1 == RecvData(_sock))
				{
					std::cout << "< " << _sock << " >select �������2..." << std::endl;
					Close();
					return false;
				}
			}
			return true;
		}
		return false;
	}

	//�Ƿ�����
	bool isRun()
	{
		return _sock != INVALID_SOCKET;
	}

	//��������С��Ԫ
#define RECV_BUFF_SIZE 1024
	//���ջ�����
	char _szRecv[RECV_BUFF_SIZE] = {};
	//�ڶ������� ��Ϣ������
	char _szMsgBuf[RECV_BUFF_SIZE*10] = {};
	//��Ϣ������β��λ��
	int _lastPos = 0;
	//�������� ����ճ�� ��ְ�
	int RecvData(SOCKET cSock)
	{
		//��������   
		int nlen = (int)recv(cSock, _szRecv, RECV_BUFF_SIZE, 0);
		if (nlen <= 0)
		{
			std::cout <<"<"<< cSock << ">��������Ͽ�����... " <<  std::endl;
			return -1;
		}

		//�����յ����ݿ�������Ϣ������
		memcpy(_szMsgBuf+_lastPos, _szRecv, nlen);
		//��Ϣ������������β��λ�ú���
		_lastPos += nlen;
		//�ж���Ϣ�����������ݳ��ȴ�����ϢͷDataHeader����
		//��ʱ�Ϳ���֪����ǰ��Ϣ��ĳ���
		while (_lastPos >= sizeof(DataHeader))
		{
			DataHeader* header = (DataHeader*)_szMsgBuf;
			//�ж���Ϣ�����������ݳ��ȴ�����Ϣ����
			if (_lastPos >= header->dataLength)
			{
				//��Ϣ������ʣ��δ�������ݵĳ���
				int nSize = _lastPos - header->dataLength;
				OnNetMsg(header);
				//����Ϣ������ʣ��δ��������ǰ��
				memcpy(_szMsgBuf, _szMsgBuf + header->dataLength, nSize);
				//��Ϣ������������β��λ��ǰ��
				_lastPos = nSize;
			}
			else
			{
				//��Ϣ������ʣ�಻��һ��������Ϣ
				break;
			}
		}
		return 0;
	}

	//��Ӧ������Ϣ
	void OnNetMsg(DataHeader* header)
	{
		switch (header->cmd)
		{
		case CMD_LOGRESULT:
		{
			LogResult* logresult = (LogResult*)header;
			std::cout << "����������: CMD_LOGRESULT,���ݳ��� :" << header->dataLength << std::endl;
		}
		break;
		case CMD_LOGOUTRES:
		{
			LogOutRes* lotoutres = (LogOutRes*)header;
			std::cout << "����������: CMD_LOGOUTRES,���ݳ��� :" << header->dataLength << std::endl;
		}
		break;
		case CMD_NEW_USER_JOIN:
		{
			NewUserJoin* newuserjoin = (NewUserJoin*)header;
			std::cout << "����������: CMD_NEW_USER_JOIN,���ݳ��� :" << header->dataLength << std::endl;
		}
		break;
		case CMD_ERROR:
		{
			std::cout << "���յ�����������: CMD_ERROR,���ݳ��� :" << header->dataLength << std::endl;
		}
		break;
		default:
		{
			std::cout << "���յ�δ������Ϣ: <"<<_sock<<">���ݳ��� :" << header->dataLength << std::endl;
		}
		}
	}

	//��������
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
