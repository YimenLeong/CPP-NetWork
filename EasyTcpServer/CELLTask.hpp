#ifndef _CELL_TASK_H_

#include <thread>
#include <mutex>
#include <list>

#include <functional>

//�������� - ����
class CellTask
{
public:
	CellTask()
	{

	}

	virtual ~CellTask()
	{

	}

	//ִ������
	virtual void doTask()
	{

	}

private:

};

//ִ������ķ�������
class CellTaskServer
{
public:
	CellTaskServer()
	{

	}

	~CellTaskServer()
	{

	}

	//�������
	void addTask(CellTask* task)
	{
		std::lock_guard<std::mutex> lock();
		_taskBuff.push_back(task);
	}
	
	//���������߳�
	void Start()
	{
		//�߳�
		std::thread t(std::mem_fn(&CellTaskServer::OnRun), this);
		t.detach();
	}

	//��������
	void OnRun()
	{
		while (true)
		{
			//�ӻ�����ȡ������
			if (_taskBuff.empty())
			{
				std::lock_guard<std::mutex> lock();
				for (auto pTask : _taskBuff)
				{
					_task.push_back(pTask);
				}
				_taskBuff.clear();
			}

			//���û������
			if (_task.empty())
			{
				//�ӳ� û��������Ϣ1����
				std::chrono::milliseconds t(1);
				std::this_thread::sleep_for(t);
				continue;
			}

			//��������
			for (auto pTask : _task)
			{
				pTask->doTask();
				delete pTask;
			}
			//�������
			_task.clear();
		}
	}

private:
	//��������
	std::list<CellTask*> _task;
	//�������ݻ�����
	std::list<CellTask*> _taskBuff;
	//�ı����ݻ�����ʱ��Ҫ����
	std::mutex _mutex;
};

#endif // !_CELL_TASK_H_
