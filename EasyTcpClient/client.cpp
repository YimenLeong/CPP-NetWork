#include "EasyTcpClient.hpp"
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

const int cCount = 1000;
const int tCount = 4;

EasyTcpClient* client[cCount];

void sendThread(int id)
{
	int c = cCount / tCount;
	int begin = (id - 1) * c;
	int end = id * c;

	for (int n = begin; n < end; n++)
	{
		client[n] = new EasyTcpClient();
	}
	for (int n = begin; n < end; n++)
	{
		client[n]->Connect("127.0.0.1", 5005);
		printf("thread<%d>,Connect=%d\n", id, n);
	}

	std::chrono::milliseconds t(5000);
	std::this_thread::sleep_for(t);

	Login login[10];
	for (int n = 0; n < 10; n++)
	{
		strcpy(login[n].userName, "lym");
		strcpy(login[n].PassWord, "lymmm");
	}
	const int nLen = sizeof(login);
	while (g_bRun)
	{
		for (int n = begin; n < end; n++)
		{
			client[n]->SendData(login, nLen);
			//client[n]->OnRun();
		}
	}

	for (int n = begin; n < end; n++)
	{
		client[n]->Close();
	}
}

int main()
{	
	std::thread t1(cmdThread);
	t1.detach();

	

	for (int n = 0; n < tCount; n++)
	{
		std::thread t1(sendThread,n+1);
		t1.detach();
	}

	std::cout << "退出任务..." << std::endl;
	getchar();
	return 0;
}
