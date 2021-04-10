#ifndef _CELL_TASK_H_

#include <thread>
#include <mutex>
#include <list>

#include <functional>

//任务类型 - 基类
class CellTask
{
public:
	CellTask()
	{

	}

	virtual ~CellTask()
	{

	}

	//执行任务
	virtual void doTask()
	{

	}

private:

};

//执行任务的服务类型
class CellTaskServer
{
public:
	CellTaskServer()
	{

	}

	~CellTaskServer()
	{

	}

	//添加任务
	void addTask(CellTask* task)
	{
		std::lock_guard<std::mutex> lock();
		_taskBuff.push_back(task);
	}
	
	//启动工作线程
	void Start()
	{
		//线程
		std::thread t(std::mem_fn(&CellTaskServer::OnRun), this);
		t.detach();
	}

	//工作函数
	void OnRun()
	{
		while (true)
		{
			//从缓冲区取出数据
			if (_taskBuff.empty())
			{
				std::lock_guard<std::mutex> lock();
				for (auto pTask : _taskBuff)
				{
					_task.push_back(pTask);
				}
				_taskBuff.clear();
			}

			//如果没有任务
			if (_task.empty())
			{
				//延迟 没有任务休息1毫秒
				std::chrono::milliseconds t(1);
				std::this_thread::sleep_for(t);
				continue;
			}

			//处理任务
			for (auto pTask : _task)
			{
				pTask->doTask();
				delete pTask;
			}
			//清空任务
			_task.clear();
		}
	}

private:
	//任务数据
	std::list<CellTask*> _task;
	//任务数据缓冲区
	std::list<CellTask*> _taskBuff;
	//改变数据缓冲区时需要加锁
	std::mutex _mutex;
};

#endif // !_CELL_TASK_H_
