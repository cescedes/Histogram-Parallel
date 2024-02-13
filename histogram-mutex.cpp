/**
 * @author Sükriye Aybüke Akgül
 * @id 12211694
 */

#include <thread>
#include <vector>
#include <iostream>
#include <random>
#include <chrono>
#include <numeric>
#include <mutex>
#include <cstdlib> 

using namespace std;

// Parsing arguments
void parse_args(int argc, char **argv, int& num_threads, int &buckets, int &sample_size, int& print_level) {
    std::string usage("Usage: --num-threads <integer> --N <integer> --samlpe-size <integer> --print-level <integer>");

    for (int i = 1; i < argc; ++i) {
        if ( std::string(argv[i]).compare("--num-threads") == 0 ) {
            num_threads=std::stoi(argv[++i]);
        } else if ( std::string(argv[i]).compare("--N") == 0 ) {
            buckets=stoi(argv[++i]);
        } else if ( std::string(argv[i]).compare("--sample-size") == 0 ) {
            sample_size=stoi(argv[++i]);
        } else if ( std::string(argv[i]).compare("--print-level") == 0 ) {
            print_level=stoi(argv[++i]);
		} else if (  std::string(argv[i]).compare("--help") == 0 ) {
            std::cout << usage << std::endl;
            exit(-1);
        }
    }
};

struct generator_cfg {
	generator_cfg(int max) : max(max) {}
	int max;
};

struct generator {
	// let's generate random number like this for consistent tests
	generator(const generator_cfg& cfg) : dist(0, cfg.max) {	}
	int operator()() {
		return dist(engine);
	}
private:
	minstd_rand engine; 
	uniform_int_distribution<int> dist;
};

struct histogram {
	histogram(int count) : data(count), mutex() { }

	void add(int i) {
		std::lock_guard<std::mutex> lock(mutex);
		++data[i];
	}

	int get(int i)	{
		std::lock_guard<std::mutex> lock(mutex);
		return data[i];
	}

	void print(std::ostream& str) {
		std::lock_guard<std::mutex> lock(mutex);
		for (size_t i = 0; i < data.size(); ++i) str << i << ":" << data[i] << endl;
		str << "total:" << accumulate(data.begin(), data.end(), 0) << endl;
	}

private:
	vector<int> data;
	std::mutex mutex;
};

struct worker
{
	worker(int sample_count, histogram& h, const generator_cfg& cfg) 
		: sample_count(sample_count), h(h), cfg(cfg) 
	{ }

	long do_work() {
		long count = 0.0;

		generator gen(cfg);
		while (sample_count--) {
			int next = gen();
			h.add(next);
			count++;
		}

		return count;
	}
private:
	int sample_count;
	histogram& h;
	const generator_cfg& cfg;
};

int main(int argc, char **argv)
{
	int N = 10;
	int sample_count = 30000000;

	int num_threads = 1;//std::thread::hardware_concurrency();
	int print_level = 2; // 0 exec time only, 1 histogram, 2 histogram + config
	parse_args(argc, argv, num_threads, N, sample_count, print_level);
	
	generator_cfg cfg(N);
	histogram h(N+1);

	long processed_count = 0;
	
	auto t1 = chrono::high_resolution_clock::now();

	vector<thread> threads;

	/* static work distribution */
	for (int t = 0; t < num_threads; ++t)
	{
		int from = (sample_count*t / num_threads);
		int to = (sample_count*(t+1) / num_threads);
		threads.emplace_back([&]() { processed_count += worker(to - from, h, cfg).do_work(); }); // <-- execute this in a thread
	}

	for (auto &thread : threads){
		thread.join();
	}

	auto t2 = chrono::high_resolution_clock::now();
	if ( print_level >= 2 ) cout << "N: " << N << ", sample size: " << sample_count << ", threads: " << num_threads << ", items processed: " << processed_count << endl;
	if ( print_level >= 1 ) h.print(cout);
	cout << chrono::duration<double>(t2 - t1).count() << endl;
	return 0;
}
