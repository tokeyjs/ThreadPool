#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<iostream>
#include<mutex>
#include<condition_variable>
#include<atomic>
#include<vector>
#include<queue>
#include<memory>
#include<functional>
#include<thread>
#include<chrono>
#include<unordered_map>

//Any类型  接收任意数据类型
class Any{
public:
    Any() = default;
    ~Any() = default;
    Any(const Any&) = delete;
    Any& operator=(const Any&) = delete;
    Any(Any &&) = default;
    Any& operator=(Any&&) = default;

    template<typename T>
    Any(T data): base_(std::make_unique<Derive<T>>(data)){}

    //这个方法把Any对象里存储的data提取
    template<typename T>
    T cast_(){
        
        //将基类指针转成派生类指针从他里面取出data
        Derive<T> *pd = dynamic_cast<Derive<T>*>(base_.get());
        if(pd == nullptr)
            throw "type is unmatch!";

        return pd->data_;
    }

private:
    //基类类型
    class Base{
    public:
        virtual ~Base() = default;
    };

    //派生类
    template<typename T>
    class Derive: public Base{
    public:
        Derive(T data) : data_(data){}

        T data_;
    };
std::unique_ptr<Base> base_;
};


//semaphore实现信号量类
class Semaphore{
public:
    Semaphore(int limit = 0):limit_(limit) {}

    //申请资源
    void wait(){

        //获取锁
        std::unique_lock<std::mutex> loc(mutex_);

        conda.wait(loc, [&]()->bool{
            return limit_ > 0;
        });

        limit_--;

    }
    //释放资源
    void post(){
        std::unique_lock<std::mutex> loc(mutex_);

        limit_++;

        conda.notify_all();

    }
private:
    int limit_;
    std::mutex mutex_;
    std::condition_variable conda;

};


//实现接收提交到线程池的task任务执行完后的返回值类型Result
class Task;

class Result{
public:
    Result(std::shared_ptr<Task> task, bool isVaild = true);
    ~Result() = default;

    // setVal方法，获取任务执行完的返回值
    void setVal(Any any);
    //get方法，用户调用这个方法获取task的返回值
    Any get();

private:
    Any any_; //存储任务的返回值
    Semaphore sem_; //线程通信信号量

    std::shared_ptr<Task> task_; //指向对应获取返回值的对象
    std::atomic_bool isVaild_; //返回值是否有效
};

//任务抽象基类
//用户可以自定义任务类型，从Task继承，重写run方法
class Task{
public:
    Task();
    ~Task() = default;

    void setResult(Result *result);
    void exec(); 
    virtual Any run() = 0;
private:
    Result *result_;
};

//线程池支持的模式
enum class PoolMode{
    MODE_FIXED, //固定数量的线程
    MODE_CACHED, //线程数量可以动态增长
};


//线程类型
class Thread{
public:
    using ThreadFunc = std::function<void(int)>;

    //线程构造
    Thread(ThreadFunc func);
    //线程析构
    ~Thread();

    //启动线程
    void start();

    //获取线程id
    int getId() const;
private:
    //线程执行函数
    ThreadFunc func_;
    static int generateId_;
    int threadId_; //线程id
};

//线程池
class ThreadPool{
public:
    ThreadPool();
    ~ThreadPool();

    //设置线程池的工作模式
    void setPoolMode(PoolMode mode);

    //开启线程池
    void start(int initThreadSize = std::thread::hardware_concurrency());

    //设置任务队列上限阈值
    void setTaskQueMaxThreshHold_(int nums);

    //给线程池提交任务
    Result submitTask(std::shared_ptr<Task> sp);

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    //线程函数
    void threadFunc(int threadid);

private:

    std::unordered_map<int, std::unique_ptr<Thread>> threads_; //线程map
    // std::vector<std::unique_ptr<Thread>> threads_; //线程列表

    int initThreadSize_; //初始线程数量
    std::atomic_int curThreadSize_; //记录当前线程池里线程的总数量
    std::atomic_int idleThreadSize_; //表示空闲线程数量
    int threadSizeThreshHold_; //线程数量上限

    std::queue<std::shared_ptr<Task>> taskQue_; //任务队列
    std::atomic_int taskSize_;  //任务数量
    int taskQueMaxThreshHold_; //任务的数量上限

    std::mutex taskQueMutex_; //保证任务队列的线程安全
    std::condition_variable notFull_; //表示任务队列不满
    std::condition_variable notEmpty_; //表示任务队列不空
    std::condition_variable exit_; //线程退出

    PoolMode poolMode_; //线程池工作模式

    std::atomic_bool isPoolRunning_; //表示当前线程池的启动状态
    


};




#endif