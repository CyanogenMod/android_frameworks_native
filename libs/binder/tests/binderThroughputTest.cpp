#include <binder/Binder.h>
#include <binder/IBinder.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#include <iostream>
#include <vector>
#include <tuple>

#include <unistd.h>
#include <sys/wait.h>

using namespace std;
using namespace android;

enum BinderWorkerServiceCode {
    BINDER_NOP = IBinder::FIRST_CALL_TRANSACTION,
};

#define ASSERT_TRUE(cond) \
do { \
    if (!(cond)) {\
       cerr << __func__ << ":" << __LINE__ << " condition:" << #cond << " failed\n" << endl; \
       exit(EXIT_FAILURE); \
    } \
} while (0)

class BinderWorkerService : public BBinder
{
public:
    BinderWorkerService() {}
    ~BinderWorkerService() {}
    virtual status_t onTransact(uint32_t code,
                                const Parcel& data, Parcel* reply,
                                uint32_t flags = 0) {
        (void)flags;
        (void)data;
        (void)reply;
        switch (code) {
        case BINDER_NOP:
            return NO_ERROR;
        default:
            return UNKNOWN_TRANSACTION;
        };
    }
};

class Pipe {
    int m_readFd;
    int m_writeFd;
    Pipe(int readFd, int writeFd) : m_readFd{readFd}, m_writeFd{writeFd} {}
    Pipe(const Pipe &) = delete;
    Pipe& operator=(const Pipe &) = delete;
    Pipe& operator=(const Pipe &&) = delete;
public:
    Pipe(Pipe&& rval) noexcept {
        m_readFd = rval.m_readFd;
        m_writeFd = rval.m_writeFd;
        rval.m_readFd = 0;
        rval.m_writeFd = 0;
    }
    ~Pipe() {
        if (m_readFd)
            close(m_readFd);
        if (m_writeFd)
            close(m_writeFd);
    }
    void signal() {
        bool val = true;
        int error = write(m_writeFd, &val, sizeof(val));
        ASSERT_TRUE(error >= 0);
    };
    void wait() {
        bool val = false;
        int error = read(m_readFd, &val, sizeof(val));
        ASSERT_TRUE(error >= 0);
    }
    template <typename T> void send(const T& v) {
        int error = write(m_writeFd, &v, sizeof(T));
        ASSERT_TRUE(error >= 0);
    }
    template <typename T> void recv(T& v) {
        int error = read(m_readFd, &v, sizeof(T));
        ASSERT_TRUE(error >= 0);
    }
    static tuple<Pipe, Pipe> createPipePair() {
        int a[2];
        int b[2];

        int error1 = pipe(a);
        int error2 = pipe(b);
        ASSERT_TRUE(error1 >= 0);
        ASSERT_TRUE(error2 >= 0);

        return make_tuple(Pipe(a[0], b[1]), Pipe(b[0], a[1]));
    }
};

static const uint32_t num_buckets = 128;
static const uint64_t max_time_bucket = 50ull * 1000000;
static const uint64_t time_per_bucket = max_time_bucket / num_buckets;
static constexpr float time_per_bucket_ms = time_per_bucket / 1.0E6;

struct ProcResults {
    uint64_t m_best = max_time_bucket;
    uint64_t m_worst = 0;
    uint32_t m_buckets[num_buckets] = {0};
    uint64_t m_transactions = 0;
    uint64_t m_total_time = 0;

    void add_time(uint64_t time) {
        m_buckets[min(time, max_time_bucket-1) / time_per_bucket] += 1;
        m_best = min(time, m_best);
        m_worst = max(time, m_worst);
        m_transactions += 1;
        m_total_time += time;
    }
    static ProcResults combine(const ProcResults& a, const ProcResults& b) {
        ProcResults ret;
        for (int i = 0; i < num_buckets; i++) {
            ret.m_buckets[i] = a.m_buckets[i] + b.m_buckets[i];
        }
        ret.m_worst = max(a.m_worst, b.m_worst);
        ret.m_best = min(a.m_best, b.m_best);
        ret.m_transactions = a.m_transactions + b.m_transactions;
        ret.m_total_time = a.m_total_time + b.m_total_time;
        return ret;
    }
    void dump() {
        double best = (double)m_best / 1.0E6;
        double worst = (double)m_worst / 1.0E6;
        double average = (double)m_total_time / m_transactions / 1.0E6;
        cout << "average:" << average << "ms worst:" << worst << "ms best:" << best << "ms" << endl;

        uint64_t cur_total = 0;
        for (int i = 0; i < num_buckets; i++) {
            float cur_time = time_per_bucket_ms * i + 0.5f * time_per_bucket_ms;
            if ((cur_total < 0.5f * m_transactions) && (cur_total + m_buckets[i] >= 0.5f * m_transactions)) {
                cout << "50%: " << cur_time << " ";
            }
            if ((cur_total < 0.9f * m_transactions) && (cur_total + m_buckets[i] >= 0.9f * m_transactions)) {
                cout << "90%: " << cur_time << " ";
            }
            if ((cur_total < 0.95f * m_transactions) && (cur_total + m_buckets[i] >= 0.95f * m_transactions)) {
                cout << "95%: " << cur_time << " ";
            }
            if ((cur_total < 0.99f * m_transactions) && (cur_total + m_buckets[i] >= 0.99f * m_transactions)) {
                cout << "99%: " << cur_time << " ";
            }
            cur_total += m_buckets[i];
        }
        cout << endl;

    }
};

String16 generateServiceName(int num)
{
    char num_str[32];
    snprintf(num_str, sizeof(num_str), "%d", num);
    String16 serviceName = String16("binderWorker") + String16(num_str);
    return serviceName;
}

void worker_fx(
    int num,
    int worker_count,
    int iterations,
    Pipe p)
{
    // Create BinderWorkerService and for go.
    ProcessState::self()->startThreadPool();
    sp<IServiceManager> serviceMgr = defaultServiceManager();
    sp<BinderWorkerService> service = new BinderWorkerService;
    serviceMgr->addService(generateServiceName(num), service);

    srand(num);
    p.signal();
    p.wait();

    // Get references to other binder services.
    cout << "Created BinderWorker" << num << endl;
    (void)worker_count;
    vector<sp<IBinder> > workers;
    for (int i = 0; i < worker_count; i++) {
        if (num == i)
            continue;
        workers.push_back(serviceMgr->getService(generateServiceName(i)));
    }

    // Run the benchmark.
    ProcResults results;
    chrono::time_point<chrono::high_resolution_clock> start, end;
    for (int i = 0; i < iterations; i++) {
        int target = rand() % workers.size();
        Parcel data, reply;
        start = chrono::high_resolution_clock::now();
        status_t ret = workers[target]->transact(BINDER_NOP, data, &reply);
        end = chrono::high_resolution_clock::now();

        uint64_t cur_time = uint64_t(chrono::duration_cast<chrono::nanoseconds>(end - start).count());
        results.add_time(cur_time);

        if (ret != NO_ERROR) {
           cout << "thread " << num << " failed " << ret << "i : " << i << endl;
           exit(EXIT_FAILURE);
        }
    }
    // Signal completion to master and wait.
    p.signal();
    p.wait();

    // Send results to master and wait for go to exit.
    p.send(results);
    p.wait();

    exit(EXIT_SUCCESS);
}

Pipe make_worker(int num, int iterations, int worker_count)
{
    auto pipe_pair = Pipe::createPipePair();
    pid_t pid = fork();
    if (pid) {
        /* parent */
        return move(get<0>(pipe_pair));
    } else {
        /* child */
        worker_fx(num, worker_count, iterations, move(get<1>(pipe_pair)));
        /* never get here */
        return move(get<0>(pipe_pair));
    }

}

void wait_all(vector<Pipe>& v)
{
    for (int i = 0; i < v.size(); i++) {
        v[i].wait();
    }
}

void signal_all(vector<Pipe>& v)
{
    for (int i = 0; i < v.size(); i++) {
        v[i].signal();
    }
}

int main(int argc, char *argv[])
{
    int workers = 2;
    int iterations = 10000;
    (void)argc;
    (void)argv;
    vector<Pipe> pipes;

    // Parse arguments.
    for (int i = 1; i < argc; i++) {
        if (string(argv[i]) == "-w") {
            workers = atoi(argv[i+1]);
            i++;
            continue;
        }
        if (string(argv[i]) == "-i") {
            iterations = atoi(argv[i+1]);
            i++;
            continue;
        }
    }

    // Create all the workers and wait for them to spawn.
    for (int i = 0; i < workers; i++) {
        pipes.push_back(make_worker(i, iterations, workers));
    }
    wait_all(pipes);


    // Run the workers and wait for completion.
    chrono::time_point<chrono::high_resolution_clock> start, end;
    cout << "waiting for workers to complete" << endl;
    start = chrono::high_resolution_clock::now();
    signal_all(pipes);
    wait_all(pipes);
    end = chrono::high_resolution_clock::now();

    // Calculate overall throughput.
    double iterations_per_sec = double(iterations * workers) / (chrono::duration_cast<chrono::nanoseconds>(end - start).count() / 1.0E9);
    cout << "iterations per sec: " << iterations_per_sec << endl;

    // Collect all results from the workers.
    cout << "collecting results" << endl;
    signal_all(pipes);
    ProcResults tot_results;
    for (int i = 0; i < workers; i++) {
        ProcResults tmp_results;
        pipes[i].recv(tmp_results);
        tot_results = ProcResults::combine(tot_results, tmp_results);
    }
    tot_results.dump();

    // Kill all the workers.
    cout << "killing workers" << endl;
    signal_all(pipes);
    for (int i = 0; i < workers; i++) {
        int status;
        wait(&status);
        if (status != 0) {
            cout << "nonzero child status" << status << endl;
        }
    }
    return 0;
}
