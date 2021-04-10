#include "EasyTcpServer.hpp"
#include <thread>

bool g_bRun = true;
void cmdThread()
{
	while (true)
	{
		char cmdBuf[256] = {};
		std::cin >> cmdBuf;

		if (0 == strcmp(cmdBuf, "exit"))
		{
			g_bRun = false;
			std::cout << "退出cmdThread线程..." << std::endl;
			break;
		}
		else
		{
			std::cout << "不支持的命令..." << std::endl;
		}
	}
}

class MyServer : public EasyTcpServer
{
public:

	//只会被一个线程触发 安全
	virtual void OnNetJoin(ClientSocket* pClient)
	{
		EasyTcpServer::OnNetJoin(pClient);
	}
	//cellServer 4 多个线程触发 不安全
	//如果只开启1个cellServer就是安全的
	virtual void OnNetLeave(ClientSocket* pClient)
	{
		EasyTcpServer::OnNetLeave(pClient);
	}
	//cellServer 4 多个线程触发 不安全
	//如果只开启1个cellServer就是安全的
	virtual void OnNetMsg(CellServer* pCellServer, ClientSocket* pClient, DataHeader* header)
	{
		EasyTcpServer::OnNetMsg(pCellServer, pClient, header);
		switch (header->cmd)
		{
		case CMD_LOGIN:
		{
			//send recv 
			Login* login = (Login*)header;
			//printf("收到客户端<Socket=%d>请求：CMD_LOGIN,数据长度：%d,userName=%s PassWord=%s\n", cSock, login->dataLength, login->userName, login->PassWord);
			//忽略判断用户密码是否正确的过程
			//LoginResult ret;
			//pClient->SendData(&ret);
			LoginResult* ret = new LoginResult();
			pCellServer->addSendTask(pClient, ret);
		}//接收 消息---处理 发送   生产者 数据缓冲区  消费者 
		break;
		case CMD_LOGOUT:
		{
			Logout* logout = (Logout*)header;
			//printf("收到客户端<Socket=%d>请求：CMD_LOGOUT,数据长度：%d,userName=%s \n", cSock, logout->dataLength, logout->userName);
			//忽略判断用户密码是否正确的过程
			//LogoutResult ret;
			//SendData(cSock, &ret);
		}
		break;
		default:
		{
			printf("<socket=%d>收到未定义消息,数据长度：%d\n", pClient->sockfd(), header->dataLength);
			//DataHeader ret;
			//SendData(cSock, &ret);
		}
		break;
		}
	}
private:

};


int main()
{
    EasyTcpServer server;
    //server.InitSocket();
    server.Bind(nullptr, 5005);
    server.Listen(1000);
	server.Start();

	std::thread t1(cmdThread);
	t1.detach();

    while (g_bRun)
    {
        server.OnRun();
    }

    server.Close();
    getchar();
    return 0;
}