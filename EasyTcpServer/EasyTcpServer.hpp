#ifndef _EasyTcpServer_Hpp_
#define _EasyTcpServer_Hpp_

#include <iostream>

#ifdef _WIN32
#define FD_SETSIZE  1024
#include <WinSock2.h>
#include <Windows.h>
#pragma comment(lib,"ws2_32.lib")
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#else
#include <unistd.h>
#include <arpa/inet.h>
#include <string>

#define SOCKET int
#define INVALID_SOCKET (SOCKET)(~0)
#define SOCKET_ERROR (-1)
#endif // _WIN32

#include <vector>
#include <thread>
#include <mutex>
#include <functional>
#include <atomic>

#include "MessageHeader.hpp"
#include "CELLTimestamp.hpp"
#include "CELLTask.hpp"

#ifndef RECV_BUFF_SIZE
#define RECV_BUFF_SIZE 10240*5
#define SEND_BUFF_SIZE RECV_BUFF_SIZE
#endif //RECV_BUFF_SIZE


#define _CELLSERVER_THREAD_COUNT 4

//�ͻ�����������
class ClientSocket
{
public:
	ClientSocket(SOCKET sockfd = INVALID_SOCKET)
	{
		_sockfd = sockfd;
		memset(_szMsgBuf, 0, sizeof(_szMsgBuf));
		_lastPos = 0;
	}

	SOCKET sockfd()
	{
		return _sockfd;
	}

	char* msgBuf()
	{
		return _szMsgBuf;
	}

	int getLastPos()
	{
		return _lastPos;
	}

	void setLastPos(int pos)
	{
		_lastPos = pos;
	}

	int SendData(DataHeader* header)
	{
		int ret = SOCKET_ERROR;
		//Ҫ���͵����ݳ���
		int nSendLen = header->dataLength;
		//Ҫ���͵�����
		const char* pSendData = (const char*)header;

		while (true)
		{
			if (_lastSendPos + nSendLen >= SEND_BUFF_SIZE)
			{
				//����ɿ��������ݳ���
				int nCopyLen = SEND_BUFF_SIZE - _lastSendPos;
				//��������
				memcpy(_szSendBuf + _lastSendPos, pSendData, nCopyLen);
				//����ʣ������λ��
				pSendData += nCopyLen;
				//����ʣ�����ݳ���
				nSendLen -= nSendLen;
				//��������
				ret = send(_sockfd, _szSendBuf, SEND_BUFF_SIZE, 0);
				//����β��λ������
				_lastSendPos = 0;
				//���ʹ���
				if (SOCKET_ERROR == ret)
				{
					return ret;
				}
			}
			else {
				//��Ҫ���͵����� ���������ͻ�����β��
				memcpy(_szSendBuf + _lastSendPos, pSendData, nSendLen);
				//��������β��λ��
				_lastSendPos += nSendLen;
				break;
			}
		}
		return ret;
	}
private:
	SOCKET _sockfd; //fd_set �ļ�����������

	//�ڶ������� ��Ϣ������
	char _szMsgBuf[RECV_BUFF_SIZE];
	//��Ϣ������β��λ��
	int _lastPos;

	//�ڶ������� ���ͻ�����
	char _szSendBuf[SEND_BUFF_SIZE];
	//���ͻ�����������β��λ��
	int _lastSendPos;
};

//�����¼��ӿ�
class INetEvent
{
public:
	virtual void OnLeave(ClientSocket* pClient) = 0;

private:

};

//������Ϣ��������
class CellSendMsg2ClientTask :public CellTask
{
private:
	ClientSocket* _pClient;
	DataHeader* _pHeader;
public:
	CellSendMsg2ClientTask(ClientSocket* pClient,DataHeader* header)
	{
		_pClient = pClient;
		_pHeader = header;
	}

	void doTask()
	{
		_pClient->SendData(_pHeader);
		delete _pClient;
	}
};

//������Ϣ���մ��������
class CellServer
{
public:
	CellServer(SOCKET sock = INVALID_SOCKET)
	{
		_sock = sock;
		_pThread = nullptr;
		_recvCount = 0;
		_pINetEvent = nullptr;
	}
	~CellServer()
	{
		Close();
		_sock = INVALID_SOCKET;
	}

	void setEventObj(INetEvent* event)
	{
		_pINetEvent = event;
	}

	//�Ƿ�����
	bool isRun()
	{
		return _sock != INVALID_SOCKET;
	}

	//����������Ϣ
	void OnRun()
	{
		while (isRun())
		{
			if (!_clientsBuff.empty())
			{
				std::lock_guard<std::mutex> lock(_mutex);
				for (auto pClient : _clientsBuff)
				{
					_clients.push_back(pClient);
				}
				_clientsBuff.clear();
			}

			//���û����Ҫ����Ŀͻ���,������
			if (_clients.empty())
			{
				std::chrono::milliseconds t(1);
				std::this_thread::sleep_for(t);
				continue;
			}
		
			//������ socket  
			fd_set fdRead;

			FD_ZERO(&fdRead);

			int maxSock = _clients[0]->sockfd();
			for (int i = (int)_clients.size() - 1; i >= 0; i--)
			{
				FD_SET(_clients[i]->sockfd(), &fdRead);
				if (maxSock < _clients[i]->sockfd())
				{
					maxSock = _clients[i]->sockfd();
				}
			}

			timeval t = { 1,0 };
			int ret = select(maxSock + 1, &fdRead, nullptr, nullptr, &t);
			if (ret < 0)
			{
				std::cout << "select �������..." << std::endl;
				Close();
				return;
			}

			for (int i = (int)_clients.size() - 1; i >= 0; i--)
			{
				if (FD_ISSET(_clients[i]->sockfd(), &fdRead))
				{
					if (-1 == RecvData(_clients[i]))
					{
						auto iter = _clients.begin() + i;
						if (iter != _clients.end())
						{
							delete _clients[i];
							_clients.erase(iter);
						}
					}
				}
			}
			return;
		}
		return;
	}

	void Close()
	{
		if (_sock != INVALID_SOCKET)
		{
#ifdef _WIN32
			for (int n = (int)_clients.size() - 1; n >= 0; n--)
			{
				closesocket(_clients[n]->sockfd());
				delete _clients[n];
			}
			// 8 �ر��׽���closesocket
			closesocket(_sock);
			//------------
			//���Windows socket����
			WSACleanup();
#else
			//�ر�socket
			for (int n = (int)_clients.size() - 1; n >= 0; n--)
			{
				close(_clients[n]->sockfd());
				delete _clients[n];
			}
			close(_sock);
#endif
			_clients.clear();
		}
	}

	//������
	char _szRecv[RECV_BUFF_SIZE] = {};
	int RecvData(ClientSocket* pClient)
	{
		//���տͻ�������   
		int nlen = (int)recv(pClient->sockfd(), _szRecv, RECV_BUFF_SIZE, 0);
		if (nlen <= 0)
		{
			std::cout << "�ͻ���< " << pClient->sockfd() << " >�˳�... " << std::endl;
			return -1;
		}
		//�����յ����ݿ�������Ϣ������
		memcpy(pClient->msgBuf() + pClient->getLastPos(), _szRecv, nlen);
		//��Ϣ������������β��λ�ú���
		pClient->setLastPos(pClient->getLastPos() + nlen);
		//�ж���Ϣ�����������ݳ��ȴ�����ϢͷDataHeader����
		//��ʱ�Ϳ���֪����ǰ��Ϣ��ĳ���
		while (pClient->getLastPos() >= sizeof(DataHeader))
		{
			DataHeader* header = (DataHeader*)pClient->msgBuf();
			//�ж���Ϣ�����������ݳ��ȴ�����Ϣ����
			if (pClient->getLastPos() >= header->dataLength)
			{
				//��Ϣ������ʣ��δ�������ݵĳ���
				int nSize = pClient->getLastPos() - header->dataLength;
				OnNetMsg(pClient->sockfd(), header);
				//����Ϣ������ʣ��δ��������ǰ��
				memcpy(pClient->msgBuf(), pClient->msgBuf() + header->dataLength, nSize);
				//��Ϣ������������β��λ��ǰ��
				pClient->setLastPos(nSize);
			}
			else
			{
				//��Ϣ������ʣ�಻��һ��������Ϣ
				break;
			}
		}
		return 0;
	}

	virtual void OnNetMsg(SOCKET _cSock, DataHeader* header)
	{
		_recvCount++;
		switch (header->cmd)
		{
		case CMD_LOGIN:
		{
			Login* login = (Login*)header;
			//std::cout <<"���յ��ͻ���< "<<_cSock<<" >"<<" CMD_LOGIN,���ݳ��� :"<<login->dataLength<<", �û���: "<<login->userName<<" ����: "<<login->PassWord<<std::endl;
			//LogResult ret;
			//SendData(_cSock, &ret);
			break;
		}
		case CMD_LOGOUT:
		{
			LogOut* logout = (LogOut*)header;
			//std::cout <<"���յ��ͻ���< "<<_cSock<<" >"<<" CMD_LOGOUT,���ݳ��� :"<<logout->dataLength<<", �û���: "<<logout->userName<<std::endl;
			//LogOutRes ret;
			//SendData(_cSock, &ret);
		}
		break;
		default:
		{
			std::cout << "���յ�δ������Ϣ: <" << _cSock << ">���ݳ��� :" << header->dataLength << std::endl;
			//DataHeader ret;
			//SendData(_cSock, &ret);
		}
		break;
		}
	}

	void addClient(ClientSocket* pClient)
	{
		//_mutex.lock();
		std::lock_guard<std::mutex> lock(_mutex);
		_clientsBuff.push_back(pClient);
		//_mutex.unlock();
	}

	void Start()
	{		
		_pThread = new std::thread(std::mem_fun(&CellServer::OnRun), this);
	}

	size_t getClientCount()
	{
		return _clients.size() + _clientsBuff.size();
	}
private:
	SOCKET _sock;
	//��ʽ�ͻ�����
	std::vector<ClientSocket*> _clients;
	//����ͻ�����
	std::vector<ClientSocket*> _clientsBuff;
	std::mutex _mutex;
	std::thread* _pThread;
	INetEvent* _pINetEvent;
public:
	std::atomic_int _recvCount;
};

class EasyTcpServer : public INetEvent
{
private:
	SOCKET _sock;
	std::vector<ClientSocket*> _clients;
	std::vector<CellServer*> _CellServers;
	CELLTimestamp _tTime;
public:
	EasyTcpServer()
	{
		_sock = INVALID_SOCKET;
	}
	virtual ~EasyTcpServer()
	{
		Close();
	}

	//��ʼ��socket
	SOCKET InitSocket()
	{
#ifdef _WIN32
		//��������
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif
		if (INVALID_SOCKET != _sock)
		{
			std::cout << "< " << _sock << " >�رվ�����..." << std::endl;
			Close();
		}

		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock)
		{
			std::cout << "����socketʧ��..." << std::endl;
		}
		else
		{
			std::cout << "����socket�ɹ�..." << std::endl;
		}
		return _sock;
	}

	//�󶨶˿ں�
	int Bind(const char* ip, unsigned short port)
	{
		if (INVALID_SOCKET == _sock)
		{
			InitSocket();
		}
		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET; //�����������
		_sin.sin_port = htons(port); //�󶨶˿�
#ifdef _WIN32
		if (ip)
		{
			_sin.sin_addr.S_un.S_addr = inet_addr(ip);//inet_addr("127.0.0.1");
		}
		else
		{
			_sin.sin_addr.S_un.S_addr = INADDR_ANY;
		}
#else 
		if (ip)
		{
			_sin.sin_addr.s_addr = inet_addr(ip); //����IP
		}
		else
		{
			_sin.sin_addr.s_addr = INADDR_ANY;
		}
#endif
		int ret = bind(_sock, (sockaddr*)&_sin, sizeof(_sin));
		if (ret == SOCKET_ERROR)
		{
			std::cout << "�󶨶˿� <" << port << "> ʧ��..." << std::endl;
		}
		else
		{
			std::cout << "�󶨶˿� <" << port << "> �ɹ�..." << std::endl;
		}
		return ret;
	}

	//�ر�socket
	void Close()
	{
		if (_sock != INVALID_SOCKET)
		{
#ifdef _WIN32
			for (int n = (int)_clients.size() - 1; n >= 0; n--)
			{
				closesocket(_clients[n]->sockfd());
				delete _clients[n];
			}
			// 8 �ر��׽���closesocket
			closesocket(_sock);
			//------------
			//���Windows socket����
			WSACleanup();
#else
			//�ر�socket
			for (int n = (int)_clients.size() - 1; n >= 0; n--)
			{
				close(_clients[n]->sockfd());
				delete _clients[n];
			}
			close(_sock);
#endif
			_clients.clear();
		}
	}

	//�����˿ں�
	int Listen(int n)
	{
		int ret = listen(_sock, n);
		if (ret == SOCKET_ERROR)
		{
			std::cout << "�������� <" << _sock << "> ʧ��..." << std::endl;
		}
		else
		{
			std::cout << "�������� <" << _sock << "> �ɹ�..." << std::endl;
		}
		return ret;
	}

	//���ܿͻ�������
	SOCKET Accept()
	{
		sockaddr_in clientAddr = {};
		int nAddrLen = sizeof(sockaddr_in);
		SOCKET cSock = INVALID_SOCKET;


#ifdef _WIN32
		cSock = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen);
#else
		cSock = accept(_sock, (sockaddr*)&clientAddr, (socklen_t*)&nAddrLen);
#endif

		if (cSock == INVALID_SOCKET)
		{
			std::cout << "���յ�< " << cSock << " >��Ч������..." << std::endl;
		}
		else
		{
			//NewUserJoin userJoin;
			//SendDataToAll(&userJoin);
			_clients.push_back(new ClientSocket(cSock));
			//std::cout << "�¿ͻ���< " << cSock << " >���� : IP = " << inet_ntoa(clientAddr.sin_addr) << std::endl;
		}
		return cSock;
	}

	void addClientToCellServer(ClientSocket* pClient)
	{
		_clients.push_back(pClient);
		//���ҿͻ��������ٵ�CellServer��Ϣ�������
		auto pMinServer = _CellServers[0];
		for (auto pCellServer : _CellServers)
		{
			if (pMinServer->getClientCount() > pCellServer->getClientCount())
			{
				pMinServer = pCellServer;
			}
		}
		pMinServer->addClient(pClient);
	}

		
	void Start()
	{
		for (int n = 0; n < _CELLSERVER_THREAD_COUNT; n++)
		{
			auto ser = new CellServer(_sock);
			_CellServers.push_back(ser);
			ser->setEventObj(this);
			ser->Start();
		}
	}

	//����������Ϣ
	bool OnRun()
	{		
		if (isRun())
		{
			time4msg();
			//������ socket  
			fd_set fdRead;
			//fd_set fdWrite;
			//fd_set fdExp;

			FD_ZERO(&fdRead);
			//FD_ZERO(&fdWrite);
			//FD_ZERO(&fdExp);

			FD_SET(_sock, &fdRead);
			//FD_SET(_sock, &fdWrite);
			//FD_SET(_sock, &fdExp);

			timeval t = { 0,10 };
			int ret = select(_sock + 1, &fdRead,nullptr, nullptr, &t);
			if (ret < 0)
			{
				std::cout << "select �������..." << std::endl;
				Close();
				return false;
			}

			//�ж������� �Ƿ��ڼ�����
			if (FD_ISSET(_sock, &fdRead))
			{
				FD_CLR(_sock, &fdRead);
				//accept �ȴ����տͻ�������
				Accept();
				return true;
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

	//��Ӧ������Ϣ
	void time4msg()
	{
		auto t1 = _tTime.getElapsedSecond();
		if (t1 >= 1.0)
		{
			int recvCount = 0;
			for (auto ser : _CellServers)
			{
				recvCount += ser->_recvCount;
				//ser->_recvCount = 0;
			}
			std::cout << "Time = " << t1 << " ,���յ���Ϣ: <" << _sock <<"> ,������: <"<< _clients.size()<< "> ,��Ϣ���� :" << recvCount/t1<< std::endl;
			_tTime.update();
		}
	}

	//����ָ��Socket����
	int SendData(SOCKET _cSock, DataHeader* header)
	{
		if (isRun() && header)
		{
			return send(_cSock, (const char*)header, header->dataLength, 0);
		}
		return SOCKET_ERROR;
	}

	void SendDataToAll(DataHeader* header)
	{
		if (isRun() && header)
		{
			for (int i = (int)_clients.size() - 1; i >= 0; i--)
			{
				SendData(_clients[i]->sockfd(), header);
			}
		}
	}

	virtual void OnLeave(ClientSocket* pClient)
	{	
		for (int i = (int)_clients.size() - 1; i >= 0; i--)
		{
			if (_clients[i] == pClient)
			{
				auto iter = _clients.begin() + i;
				if (iter != _clients.end())
				{
					_clients.erase(iter);
				}
			}
		}
	}
};
#endif