# FastBuffer
Very Fast unbounded MPMC C++ implementation
A very small and very fast implementation of the thread safe blocking queue. I have benchmarked it with respect to fastflow mpmc queues, boost::lockfree queue, concurrentqueue at https://github.com/cameron314/concurrentqueue. The benchmarks puts this implementation way ahead of all. I will upload the benchmark data, setup soon. 
The queue/buffer offers blocking and non blocking api. If you are willing to help with benchmarking/reviewing this queue , contact me. 


