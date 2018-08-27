#ifndef BUFFER_H
#define BUFFER_H
#include <mutex>
#include <queue>
#include <atomic>
#include <set>
#include <thread>
#include <numeric>

template<class QueueData>class BlockingQueue
{
private:
	std::queue<QueueData*> m_queue;						// Use STL queue to store data
	std::atomic<bool> m_lock;
	QueueData* m_latest;
	bool m_done;

	BlockingQueue(const BlockingQueue &);

	BlockingQueue* operator=(const BlockingQueue &);

	BlockingQueue(BlockingQueue &&);

	BlockingQueue* operator=(BlockingQueue &&);

	void lock() {
		bool val = false;
		while (!m_lock.compare_exchange_strong(val, true,std::memory_order::memory_order_seq_cst)) {
			val = false;
			std::this_thread::sleep_for(std::chrono::nanoseconds(10));
		}
	}

	void unlock() {
		m_lock.store(false);
	}

	bool try_lock() {
		bool val = false;
		if (m_lock.compare_exchange_strong(val, true, std::memory_order::memory_order_seq_cst)) {
			return true;
		}
		return false;
	}

	bool try_lock_for(int _10_nanoseconds_times) {
		int count = 0;
		while (!try_lock()) {
			if (count >= _10_nanoseconds_times) {
				return false;
			}
			count++;
			std::this_thread::sleep_for(std::chrono::nanoseconds(10));
		}
		return true;
	}

public:

	BlockingQueue()
	{
		m_done = false;
		m_lock = false;
		m_latest = nullptr;
	}

	~BlockingQueue() {
		ShutDown();
		CleanUp();
	};

	void Insert(QueueData *_data)
	{
		lock();
		if (m_done) {
			unlock();
			return;
		}
		m_queue.push(_data);
		m_latest = _data;
		unlock();
		return;
	}

	bool Insert_try(QueueData *_data)
	{
		if (!try_lock_for(10)) {
			return false;
		}
		if (m_done) {
			unlock();
			return true;
		}
		m_queue.push(_data);
		unlock();
		return true;
	}
	int Remove_try(QueueData **_data)
	{
		if (!try_lock_for(500)) {
			return 2;
		}

		if (m_queue.size() == 0)
		{
			if (m_done) {
				unlock();
				return 0;
			}
			unlock();
			return 2;
		}
		if (m_latest) {
			*_data = m_latest;
			m_latest = nullptr;
		}
		else {
			*_data = &(*m_queue.front());
		}
		m_queue.pop();
		unlock();
		return 1;
	};

	int Remove(QueueData **_data) {
		while (true) {
			auto c = Remove_try(_data);
			if (c == 0 || c == 1) {
				return c;
			}
			std::this_thread::sleep_for(std::chrono::nanoseconds(10));
		}
	}

	int CanInsert()
	{
		lock();
		if (m_done) {
			unlock();
			return 0;
		}
		unlock();
		return 1;
	};


	void ShutDown()
	{
		lock();
		m_done = true;
		unlock();
	}

	void Restart() {
		CleanUp();
		lock();
		m_done = false;
		unlock();
	}

	bool IsShutDown() {
		lock();
		bool val= m_done;
		unlock();
		return val;
	}

	void CleanUp()
	{
		lock();
		while (!m_queue.empty())
		{
			m_queue.pop();
		}
		unlock();
	}
};

template<class QueueData>
class BlockingQueueFast {
	std::vector<BlockingQueue<QueueData>* > m_qs;
	std::vector<std::atomic<bool>*> m_consumer_qs;
	std::size_t m_max_val;
	std::atomic<int> m_producer_token;
	std::atomic<int> m_consumer_token;
	int m_token_jmp_consumer;
	int m_token_jmp_producer;
public:


	BlockingQueueFast(std::size_t _producers,
		std::size_t _consumers)
	{
		std::size_t count = (std::max)((_producers + _consumers) * 2, static_cast<std::size_t>(std::thread::hardware_concurrency()));
		m_qs.reserve(count);
		std::size_t s = 0;
		for (; s < count; s++) {
			m_qs.push_back(new BlockingQueue<QueueData>());
			m_consumer_qs.push_back(new std::atomic<bool>(true));
		}
		m_max_val = m_qs.size();
		m_consumer_token = 0;
		m_producer_token = 0;
		m_token_jmp_consumer = static_cast<double>(count) / static_cast<double>(_consumers);
		m_token_jmp_producer = static_cast<double>(count) / static_cast<double>(_producers);
	}

	~BlockingQueueFast() {
		ShutDown();
		CleanUp();
		std::size_t s = 0;
		for (; s<m_qs.size(); s++) {
			delete m_qs[s];
			delete m_consumer_qs[s];
		}
		m_qs.clear();
	};

	int get_producer_token() {
		return m_producer_token.fetch_add(m_token_jmp_producer);
	}

	int get_consumer_token() {
		return m_consumer_token.fetch_add(m_token_jmp_consumer);
	}

	void Insert(QueueData *_data, int &_token )
	{
		for (auto v = 0; v < m_max_val; v++) {
			_token = _token % m_max_val;
			if (m_qs[_token]->Insert_try(_data)) {
				return;
			}
			_token++;
		}
		_token = _token % m_max_val;
		m_qs[_token]->Insert(_data);
	}

	  // Get data from the queue. Wait for data if not available
	int Remove(QueueData **_data, int &_token )
	{
		auto complete_qs = 0;
		while (true) {
			_token = _token%m_max_val;

			if (m_consumer_qs[_token]->load()) {
				int c = m_qs[_token]->Remove_try(_data);
				if (c == 1) {
					return c;
				}
				if (c == 0) {
					m_consumer_qs[_token]->store(false);
				}
				if (c == 2) {
					std::this_thread::sleep_for(std::chrono::nanoseconds(10));
				}
			}
			else {
				complete_qs++;
				if (complete_qs >= m_max_val) {
					return 0;
				}
			}
			_token++;
		}
	};


	void ShutDown()
	{
		std::size_t s = 0;
		for(;s<m_qs.size();s++){
			m_qs[s]->ShutDown();
		}

	}

	void CleanUp()
	{
		std::size_t s = 0;
		for (; s<m_qs.size(); s++) {
			m_qs[s]->CleanUp();
		}
	}
};
#endif
