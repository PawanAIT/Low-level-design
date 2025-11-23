#include <chrono>
#include <functional>
#include <iostream>
#include <thread>
using namespace std;
using Clock = chrono::steady_clock;
using Time = Clock::time_point;
using Duration = Clock::duration;
struct Job {
  int id;
  function<void()> task;
  Time when;
  Clock::duration after;
  bool is_recurring;

 public:
  Job(int id,
      function<void()> task,
      Time when,
      Clock::duration after = Duration::zero())
      : id(id),
        task(task),
        when(when),
        after(after),
        is_recurring(after != Duration::zero())
  {
  }
  bool operator<(const Job& other) const { return when > other.when; }
};
class IJobScheduler {
  virtual int schedule(function<void()> task, Time t) = 0;
  virtual int recurringSchedule(function<void()> task, Time t, Duration d) = 0;
};
class JobScheduler : public IJobScheduler {
 public:
  JobScheduler() { scheduler_thread = thread(&JobScheduler::run, this); }
  ~JobScheduler()
  {
    stop();
    scheduler_thread.join();
  }
  void stop()
  {
    is_stopped = true;
    cv.notify_all();
  }
  int schedule(function<void()> task, Time t) override
  {
    int jobId = counter++;
    {
      unique_lock<mutex> lock(mtx);
      pq.push(Job({jobId, task, t}));
    }
    cv.notify_all();  // notify when we add.
    return jobId;
  }
  int recurringSchedule(function<void()> task, Time t, Duration d) override
  {
    int jobId = counter++;
    {
      unique_lock<mutex> lock(mtx);
      pq.push(Job({jobId, task, t, d}));
    }
    cv.notify_all();  // notify when we add.
    return jobId;
  }

 private:
  void run()
  {
    // Have a stop functionality.
    while (!is_stopped) {
      unique_lock<mutex> lock(mtx);
      if (pq.empty()) {
        cv.wait(lock, [this]() {
          return !pq.empty() || is_stopped;
        });  // wait until pq has element
      }
      if (is_stopped) break;
      auto next_run_time = pq.top().when;
      cv.wait_until(lock, next_run_time, [this, &next_run_time]() {
        return is_stopped || pq.top().when < next_run_time;
      });
      if (pq.top().when <= Clock::now()) {
        auto job = pq.top();
        pq.pop();
        lock.unlock();

        thread(job.task).detach();

        if (job.is_recurring) {
          job.when += job.after;
          lock.lock();
          pq.push(job);
          lock.unlock();
        }
      }
    }
  }

 private:
  thread scheduler_thread;
  atomic<int> counter{0};
  mutex mtx;
  priority_queue<Job> pq;
  condition_variable cv;
  atomic<bool> is_stopped{false};
};
int main()
{
  JobScheduler s;
  s.recurringSchedule(
      []() { cout << "recurr 5 sec\n"; }, Clock::now(), chrono::seconds(5));
  s.schedule([]() { cout << "after 3 seconds\n"; },
             Clock::now() + chrono::seconds(3));
  s.schedule([]() { cout << "after 1 seconds\n"; },
             Clock::now() + chrono::seconds(1));
  s.schedule([]() { cout << "after 0 seconds\n"; },
             Clock::now() + chrono::seconds(0));
  this_thread::sleep_for(chrono::seconds(11));
  return 0;
}
