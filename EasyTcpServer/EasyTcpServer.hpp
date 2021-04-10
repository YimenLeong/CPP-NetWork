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

//客户端数据类型
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
		//要发送的数据长度
		int nSendLen = header->dataLength;
		//要发送的数据
		const char* pSendData = (const char*)header;

		while (true)
		{
			if (_lastSendPos + nSendLen >= SEND_BUFF_SIZE)
			{
				//计算可拷贝的数据长度
				int nCopyLen = SEND_BUFF_SIZE - _lastSendPos;
				//拷贝数据
				memcpy(_szSendBuf + _lastSendPos, pSendData, nCopyLen);
				//计算剩余数据位置
				pSendData += nCopyLen;
				//计算剩余数据长度
				nSendLen -= nSendLen;
				//发送数据
				ret = send(_sockfd, _szSendBuf, SEND_BUFF_SIZE, 0);
				//数据尾部位置清零
				_lastSendPos = 0;
				//发送错误
				if (SOCKET_ERROR == ret)
				{
					return ret;
				}
			}
			else {
				//将要发送的数据 拷贝到发送缓冲区尾部
				memcpy(_szSendBuf + _lastSendPos, pSendData, nSendLen);
				//计算数据尾部位置
				_lastSendPos += nSendLen;
				break;
			}
		}
		return ret;
	}
private:
	SOCKET _sockfd; //fd_set 文件描述符集合

	//第二缓冲区 消息缓冲区
	char _szMsgBuf[RECV_BUFF_SIZE];
	//消息缓冲区尾部位置
	int _lastPos;

	//第二缓冲区 发送缓冲区
	char _szSendBuf[SEND_BUFF_SIZE];
	//发送缓冲区的数据尾部位置
	int _lastSendPos;
};

//网络事件接口
class INetEvent
{
public:
	virtual void OnLeave(ClientSocket* pClient) = 0;

private:

};

//网络消息发送任务
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

//网络消息接收处理服务类
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

	//是否工作中
	bool isRun()
	{
		return _sock != INVALID_SOCKET;
	}

	//处理网络消息
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

			//如果没有需要处理的客户端,就跳过
			if (_clients.empty())
			{
				std::chrono::milliseconds t(1);
				std::this_thread::sleep_for(t);
				continue;
			}
		
			//伯克利 socket  
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
				std::cout << "select 任务结束..." << std::endl;
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
			// 8 关闭套节字closesocket
			closesocket(_sock);
			//------------
			//清除Windows socket环境
			WSACleanup();
#else
			//关闭socket
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

	//缓冲区
	char _szRecv[RECV_BUFF_SIZE] = {};
	int RecvData(ClientSocket* pClient)
	{
		//接收客户端数据   
		int nlen = (int)recv(pClient->sockfd(), _szRecv, RECV_BUFF_SIZE, 0);
		if (nlen <= 0)
		{
			std::cout << "客户端< " << pClient->sockfd() << " >退出... " << std::endl;
			return -1;
		}
		//将接收的数据拷贝到消息缓冲区
		memcpy(pClient->msgBuf() + pClient->getLastPos(), _szRecv, nlen);
		//消息缓冲区的数据尾部位置后移
		pClient->setLastPos(pClient->getLastPos() + nlen);
		//判断消息缓冲区的数据长度大于消息头DataHeader长度
		//这时就可以知道当前消息体的长度
		while (pClient->getLastPos() >= sizeof(DataHeader))
		{
			DataHeader* header = (DataHeader*)pClient->msgBuf();
			//判断消息缓冲区的数据长度大于消息长度
			if (pClient->getLastPos() >= header->dataLength)
			{
				//消息缓冲区剩余未处理数据的长度
				int nSize = pClient->getLastPos() - header->dataLength;
				OnNetMsg(pClient->sockfd(), header);
				//将消息缓冲区剩余未处理数据前移
				memcpy(pClient->msgBuf(), pClient->msgBuf() + header->dataLength, nSize);
				//消息缓冲区的数据尾部位置前移
				pClient->setLastPos(nSize);
			}
			else
			{
				//消息缓冲区剩余不够一条完整消息
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
			//std::cout <<"接收到客户端< "<<_cSock<<" >"<<" CMD_LOGIN,数据长度 :"<<login->dataLength<<", 用户名: "<<login->userName<<" 密码: "<<login->PassWord<<std::endl;
			//LogResult ret;
			//SendData(_cSock, &ret);
			break;
		}
		case CMD_LOGOUT:
		{
			LogOut* logout = (LogOut*)header;
			//std::cout <<"接收到客户端< "<<_cSock<<" >"<<" CMD_LOGOUT,数据长度 :"<<logout->dataLength<<", 用户名: "<<logout->userName<<std::endl;
			//LogOutRes ret;
			//SendData(_cSock, &ret);
		}
		break;
		default:
		{
			std::cout << "接收到未定义消息: <" << _cSock << ">数据长度 :" << header->dataLength << std::endl;
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
	//正式客户队列
	std::vector<ClientSocket*> _clients;
	//缓冲客户队列
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

	//初始化socket
	SOCKET InitSocket()
	{
#ifdef _WIN32
		//建立连接
		WORD ver = MAKEWORD(2, 2);
		WSADATA dat;
		WSAStartup(ver, &dat);
#endif
		if (INVALID_SOCKET != _sock)
		{
			std::cout << "< " << _sock << " >关闭旧连接..." << std::endl;
			Close();
		}

		_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (INVALID_SOCKET == _sock)
		{
			std::cout << "建立socket失败..." << std::endl;
		}
		else
		{
			std::cout << "建立socket成功..." << std::endl;
		}
		return _sock;
	}

	//绑定端口号
	int Bind(const char* ip, unsigned short port)
	{
		if (INVALID_SOCKET == _sock)
		{
			InitSocket();
		}
		sockaddr_in _sin = {};
		_sin.sin_family = AF_INET; //基于网络服务
		_sin.sin_port = htons(port); //绑定端口
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
			_sin.sin_addr.s_addr = inet_addr(ip); //本机IP
		}
		else
		{
			_sin.sin_addr.s_addr = INADDR_ANY;
		}
#endif
		int ret = bind(_sock, (sockaddr*)&_sin, sizeof(_sin));
		if (ret == SOCKET_ERROR)
		{
			std::cout << "绑定端口 <" << port << "> 失败..." << std::endl;
		}
		else
		{
			std::cout << "绑定端口 <" << port << "> 成功..." << std::endl;
		}
		return ret;
	}

	//关闭socket
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
			// 8 关闭套节字closesocket
			closesocket(_sock);
			//------------
			//清除Windows socket环境
			WSACleanup();
#else
			//关闭socket
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

	//监听端口号
	int Listen(int n)
	{
		int ret = listen(_sock, n);
		if (ret == SOCKET_ERROR)
		{
			std::cout << "监听服务 <" << _sock << "> 失败..." << std::endl;
		}
		else
		{
			std::cout << "监听服务 <" << _sock << "> 成功..." << std::endl;
		}
		return ret;
	}

	//接受客户端连接
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
			std::cout << "接收到< " << cSock << " >无效的连接..." << std::endl;
		}
		else
		{
			//NewUserJoin userJoin;
			//SendDataToAll(&userJoin);
			_clients.push_back(new ClientSocket(cSock));
			//std::cout << "新客户端< " << cSock << " >加入 : IP = " << inet_ntoa(clientAddr.sin_addr) << std::endl;
		}
		return cSock;
	}

	void addClientToCellServer(ClientSocket* pClient)
	{
		_clients.push_back(pClient);
		//查找客户数量最少的CellServer消息处理对象
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

	//处理网络消息
	bool OnRun()
	{		
		if (isRun())
		{
			time4msg();
			//伯克利 socket  
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
				std::cout << "select 任务结束..." << std::endl;
				Close();
				return false;
			}

			//判断描述符 是否在集合中
			if (FD_ISSET(_sock, &fdRead))
			{
				FD_CLR(_sock, &fdRead);
				//accept 等待接收客户端连接
				Accept();
				return true;
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

	//响应网络消息
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
			std::cout << "Time = " << t1 << " ,接收到消息: <" << _sock <<"> ,连接数: <"<< _clients.size()<< "> ,消息次数 :" << recvCount/t1<< std::endl;
			_tTime.update();
		}
	}

	//发送指定Socket数据
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