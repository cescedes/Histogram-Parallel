#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <numeric>
#include <future>
#include <algorithm>
#include <atomic>

using namespace std;

// Parsing arguments
void parse_args(int argc, char **argv, int& num_threads, int &buckets, int &sample_size, int& print_level) {
    std::string usage("Usage: --num-threads <integer> --N <integer> --sample-size <integer> --print-level <integer>");

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
		for (size_t i = 0; i < data.size(); ++i) {
			str << i << ":" << data[i] << endl;
		}
		str << "total:" << accumulate(data.begin(), data.end(), 0) << endl;
	}

private:
	vector<int> data;
	std::mutex mutex;
};

struct worker
{
    worker(int sample_count, vector<int>& local_histogram, const generator_cfg& cfg)
        : sample_count(sample_count), local_histogram(local_histogram), cfg(cfg) {}

    void do_work()
    {
        generator gen(cfg);
        while (sample_count--)
        {
            int next = gen();
            ++local_histogram[next];
        }
    }

private:
    int sample_count;
    vector<int>& local_histogram;
    const generator_cfg& cfg;
};

int main(int argc, char **argv)
{
    int N = 10;
    int sample_count = 30000000;

    int num_threads = 1;
    int print_level = 2; // 0 exec time only, 1 histogram, 2 histogram + config
    parse_args(argc, argv, num_threads, N, sample_count, print_level);

    generator_cfg cfg(N);

    // Create a vector of thread-local histograms
    vector<vector<int>> local_histograms(num_threads, vector<int>(N + 1, 0));

    auto t1 = chrono::high_resolution_clock::now();

    vector<future<void>> futures;

    // dynamic work distribution based on the number of threads
    for (int t = 0; t < num_threads; ++t)
    {
        int from = (sample_count * t / num_threads);
        int to = (sample_count * (t + 1) / num_threads);

        // asynchronously execute the worker
        futures.push_back(async(launch::async, [&local_histograms, &cfg](int start, int end, int thread_id) {
            worker(end - start, local_histograms[thread_id], cfg).do_work();
        }, from, to, t));
    }

    // wait for all tasks to complete
    for (auto& future : futures)
    {
        future.wait();
    }

    // Merge local histograms into the global histogram
    vector<int> global_histogram(N + 1, 0);
    for (const auto& local_histogram : local_histograms)
    {
        transform(local_histogram.begin(), local_histogram.end(), global_histogram.begin(), global_histogram.begin(), plus<int>());
    }

    auto t2 = chrono::high_resolution_clock::now();
    if (print_level >= 2)
        cout << "N: " << N << ", sample size: " << sample_count << ", threads: " << num_threads << endl;
    if (print_level >= 1)
    {
        for (size_t i = 0; i < global_histogram.size(); ++i)
            cout << i << ":" << global_histogram[i] << endl;
        cout << "total:" << accumulate(global_histogram.begin(), global_histogram.end(), 0) << endl;
    }
    cout << chrono::duration<double>(t2 - t1).count() << endl;
}