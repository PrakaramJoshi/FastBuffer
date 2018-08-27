// FASTBuffer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "BlockingQueueMinimal.h"
#include <atomic>
#include <chrono>
//#include< ctime>
#include <fstream>
#include <iostream>
#include<sstream>
#include <stdio.h>
#include <time.h>
typedef int* QueueData;
typedef BlockingQueueFast<int> ThreadsafeQueue;

#define DataGen new int
#define DataDel(x) delete x;
std::atomic<bool> start;
std::vector<QueueData> data_cache;
inline std::tm localtime(std::time_t const & time) {
	std::tm tm_snapshot;
#if (defined(__MINGW32__) || defined(__MINGW64__))
	memcpy(&tm_snapshot, ::localtime(&time), sizeof(std::tm));
#elif (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
	localtime_s(&tm_snapshot, &time);
#else
	localtime_r(&time, &tm_snapshot); // POSIX  
#endif
	return tm_snapshot;
}
std::string static time_stamp_file_name() {

	std::ostringstream s;

	std::time_t t = std::time(NULL);
	std::tm timeinfo = localtime(t);

	s << timeinfo.tm_mon + 1
		<< "."
		<< timeinfo.tm_mday
		<< "."
		<< timeinfo.tm_year + 1900
		<< "_"
		<< timeinfo.tm_hour
		<< "."
		<< timeinfo.tm_min
		<< "."
		<< timeinfo.tm_sec;
	return s.str();
}


void producer(std::vector<QueueData> *_data, ThreadsafeQueue *_q) {
	std::size_t i = 0;
	auto size = _data->size();
	auto token =  _q->get_producer_token();
	while (!start) {
		std::this_thread::sleep_for(std::chrono::microseconds(1));
	}
	while (i < size) {
		_q->Insert((*_data)[i],token);
		i++;
	}
}

void consumer(ThreadsafeQueue *_q) {
	QueueData i;
	auto token = _q->get_consumer_token();
	while (_q->Remove(&i,token)) {
		i;
	}
}

void init_data(std::vector<std::vector<QueueData>> &_data, std::size_t _producers, std::size_t _data_per_producer) {
	//std::cout << " initializing data...";
	std::size_t index = 0;
	std::size_t data_cache_index = 0;
	_data.resize(_producers);
	while (_producers > 0) {
		_data[index].resize(_data_per_producer);
		for (auto i = 0; i < _data_per_producer; i++) {
			if (data_cache_index >= data_cache.size()) {
				data_cache.reserve(data_cache.size() + (_data_per_producer)*(_producers)+(_data_per_producer - i));
				for (auto j = 0; j < (_data_per_producer)*(_producers)+(_data_per_producer - i); j++) {
					data_cache.push_back(DataGen);
				}
			}
			_data[index][i] =data_cache[data_cache_index];
			data_cache_index++;
		}
		index++;
		_producers--;
	}
}

void init_data_cache(std::size_t _max_data) {
	data_cache.reserve(_max_data);
	for (auto i = 0; i < _max_data; i++) {
		data_cache.push_back( DataGen);
	}
}

void clear_data_cache() {
	for (auto c : data_cache) {
		DataDel(c)
	}
	data_cache.clear();
}

std::string benchmark(std::size_t _threads_producer, std::size_t _threads_consumer, std::size_t _data_per_producer) {
	std::vector<std::vector<QueueData> > data;
	start = false;
	init_data(data, _threads_producer, _data_per_producer);
	ThreadsafeQueue q(_threads_producer, _threads_consumer);
	//ThreadsafeQueue q;
	std::vector<std::thread*> producers;
	std::vector<std::thread*> consumers;
	producers.resize(_threads_producer);
	consumers.resize(_threads_consumer);
	for (auto i = 0; i < _threads_consumer; i++) {
		consumers[i] = new std::thread(consumer, &q);
	}
	for (auto i = 0; i < _threads_producer; i++) {
		producers[i] = new std::thread(producer, &data[i], &q);
	}
	auto start_time = std::chrono::high_resolution_clock::now();
	start = true;
	for (auto i = 0; i < _threads_producer; i++) {
		producers[i]->join();
	}
	q.ShutDown();
	for (auto i = 0; i < _threads_consumer; i++) {
		consumers[i]->join();
	}
	auto duration = (std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start_time).count());
	auto items_per_microsecond = static_cast<double>(_data_per_producer)*static_cast<double>(_threads_producer) / static_cast<double>(duration);
	std::stringstream str;
	str << _threads_producer << "," << _threads_consumer << "," << _data_per_producer*_threads_producer << "," << duration << "," << items_per_microsecond << std::endl;
	for (auto i = 0; i < _threads_consumer; i++) {
		delete consumers[i];
	}
	for (auto i = 0; i < _threads_producer; i++) {
		delete producers[i];
	}
	return str.str();

}

void benchmark_usual() {
	std::string file_name = "usual_tests";
	std::ofstream ofs(file_name + ".csv");
	ofs << "Producers,Consumers,Data,Time(us),Speed(Data/us)" << std::endl;
	init_data_cache(std::size_t(std::thread::hardware_concurrency() * 1000000));
	for (unsigned int  producer_count = 1; producer_count < std::thread::hardware_concurrency(); producer_count += 1) {
		for (unsigned int i = 1; i < std::thread::hardware_concurrency(); i += 1) {
			std::cout << "Benchmarking Producers : " << producer_count << " Consumers : " << i << std::endl;
			ofs << benchmark(producer_count, i, 1000000);
		}
	}
	clear_data_cache();
}

int main()
{
	benchmark_usual();
	std::string file_name = "extreme_tests";
	std::ofstream ofs(file_name+ ".csv");
	ofs << "Producers,Consumers,Data,Time(us),Speed(Data/us)" << std::endl;
	init_data_cache(std::size_t(100 * 100000));
	for (auto producer_count = 1; producer_count < 100; producer_count += 10) {
		for (auto i = 1; i < 100; i += 10) {
			std::cout << "Benchmarking Producers : " << producer_count << " Consumers : " << i << std::endl;
			ofs << benchmark(producer_count, i, 100000);
		}
	}
	clear_data_cache();
    return 0;
}

