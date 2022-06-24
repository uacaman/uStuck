#pragma once

#include "windows.h"
#include "Timer.h"
#include <API/ARK/Ark.h>

namespace uStuck::Tools
{
	class Timer
	{
	public:
		Timer()
		{
			ArkApi::GetCommands().AddOnTimerCallback("uStuck.HelperTimer.Update", std::bind(&Timer::Update, this));
		}

		~Timer()
		{
			ArkApi::GetCommands().RemoveOnTimerCallback("uStuck.HelperTimer.Update");
		}

		template <class callable, class... arguments>
		void DelayExec(callable&& f, int after, bool async, arguments&&... args)
		{
			if (!async)
			{
				std::lock_guard<std::mutex> lg(tasks_mtx_);
				tasks_.push_back(std::make_shared<Task>(std::bind(f, std::forward<arguments>(args)...), after));
			}
			else
			{
				auto func = std::bind(f, std::forward<arguments>(args)...);
				std::thread([&func, after]()
					{
						std::this_thread::sleep_for(std::chrono::seconds(after));
						func();
					}
				).detach();
			}
		}

		void Update()
		{
			const time_t now = std::time(nullptr);

			std::lock_guard<std::mutex> lg(tasks_mtx_);
			for (const auto& func : tasks_)
			{
				if (func
					&& func->execTime <= now
					&& func->callback)
				{
					func->callback();

					tasks_.erase(std::find(tasks_.begin(), tasks_.end(), func));
				}
			}
		}

		static Timer& Get()
		{
			static Timer instance;
			return instance;
		}

	private:
		struct Task
		{
			Task(std::function<void()> cbk, int after)
				: callback(std::move(cbk))
			{
				execTime = std::time(nullptr) + after;
			}

			std::function<void()> callback;
			time_t execTime;
		};

		std::mutex tasks_mtx_;
		std::vector<std::shared_ptr<Task>> tasks_;
	};

	inline int GetRandomNumber(int min, int max)
	{
		const int n = max - min + 1;
		const int remainder = RAND_MAX % n;
		int x;

		do
		{
			x = rand();
		} while (x >= RAND_MAX - remainder);

		return min + x % n;
	}
}
