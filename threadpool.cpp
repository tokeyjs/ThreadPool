#include"threadpool.h"

const int TASK_MAX_THRESHHOLD = 1024; //任务队列最大数量
const int THREAD_MAX_THRESHHOLD = 10; //线程的最大数量
const int THREAD_MAX_IDLE_TIME = 60; //单位：秒

//线程池构造
ThreadPool::ThreadPool():
    initThreadSize_(4), 
    taskSize_(0),
    curThreadSize_(0),
    threadSizeThreshHold_(THREAD_MAX_THRESHHOLD),
    taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD),
    poolMode_(PoolMode::MODE_FIXED),
    isPoolRunning_(false),
    idleThreadSize_(0)
    {}

//线程池析构
ThreadPool::~ThreadPool(){
    isPoolRunning_ = false;

    std::unique_lock<std::mutex> loc(taskQueMutex_);
    notEmpty_.notify_all();
    exit_.wait(loc, [&]()->bool{
        return curThreadSize_==0;
    });

    std::cout<<"threadpool exit !"<<std::endl;

}

//设置线程池的工作模式
void ThreadPool::setPoolMode(PoolMode mode){
    //线程池已经启动就禁止更改一些属性
    if(isPoolRunning_)
        return;

    poolMode_ = mode;
}

//开启线程池
void ThreadPool::start(int initThreadSize){
    //设置状态
    isPoolRunning_ = true;

    //设置初始化线程数量
    initThreadSize_ = initThreadSize;
    curThreadSize_ = initThreadSize;
    
    for(int i = 0;i < initThreadSize_; i++){
        //创建线程对象, 把线程函数给到thread线程对象
        std::unique_ptr<Thread> thre(new Thread(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1)));

        int threadid = thre->getId();
        threads_.emplace(threadid, std::move(thre));

        std::cout<<"create new thread: ["<<threadid<<"]"<<std::endl;

        // threads_.emplace_back(std::move(thre));
    }

    //启动所有线程
    for(int i = 0;i < initThreadSize_; i++){
        threads_[i]->start();
        //空闲线程数增加
        idleThreadSize_++;
    }

}

//设置任务队列上限阈值
void ThreadPool::setTaskQueMaxThreshHold_(int nums){
    //线程池已经启动就禁止更改一些属性
    if(isPoolRunning_)
        return;

    taskQueMaxThreshHold_ = nums;
}


//给线程池提交任务   用户提交任务给线程池
Result ThreadPool::submitTask(std::shared_ptr<Task> sp){

    //加锁
    std::unique_lock<std::mutex> loc(taskQueMutex_);

    //任务队列已满
    //用户提交任务，最长不能阻塞超过1s，否则判定提交任务失败返回
    if(!notFull_.wait_for(loc, std::chrono::seconds(1),
        [&]()->bool{
        return taskQue_.size() < taskQueMaxThreshHold_;
    })){
        //表示等待一秒钟条件依然不能满足
        std::cerr<<"task queue is full, submit task fail."<<std::endl;
        // std::cout<<"submit task successed"<<std::endl;

        return Result(sp, false);
    }

    //加入到任务队列
    taskQue_.emplace(sp);

    std::cout<<"submit task successed"<<std::endl;

    taskSize_++;
    //唤醒因为任务队列空而等待的线程
    notEmpty_.notify_all();


    // cached模式 根据任务数量与空闲线程数量判断是否需要创建线程
    if(poolMode_ == PoolMode::MODE_CACHED 
    && taskSize_ > idleThreadSize_ 
    && curThreadSize_ < threadSizeThreshHold_){
        //创建新线程
        //创建线程对象, 把线程函数给到thread线程对象
        std::unique_ptr<Thread> thre(new Thread(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1)));
        int threadid = thre->getId();
        threads_.emplace(threadid, std::move(thre));
        curThreadSize_++;

        std::cout<<"create new thread: ["<<threadid<<"]"<<std::endl;

        threads_[threadid]->start();
        //空闲线程数增加
        idleThreadSize_++;
    }



    return Result(sp);
}


//线程函数(所有线程执行的线程函数)   线程池的所有线程从任务队列获取任务
void ThreadPool::threadFunc(int threadid){
    // std::cout<<"opo"<<std::endl;
    auto lastTime = std::chrono::high_resolution_clock().now();

    while(isPoolRunning_){
        std::shared_ptr<Task> task;
        {
            //获取锁
            std::unique_lock<std::mutex> loc(taskQueMutex_);

            //cached模式下有可能已经创建很多线程，但是空闲时间超过60s，
            //应该把多余线程回收（超过initThreadSize_数量的线程要回收）
            //当前时间 - 上一次线程执行的时间 >60s
            if(poolMode_ == PoolMode::MODE_CACHED && curThreadSize_ > initThreadSize_){

                //每一秒返回一次isPoolRunning_&&
                while(taskQue_.size() == 0){
                    
                    //线程池结束
                    if(!isPoolRunning_){
                        threads_.erase(threadid);

                        std::cout<<"thread : ["<<threadid<<"] exit"<<std::endl;
                        curThreadSize_--;
                        idleThreadSize_--;
                        exit_.notify_all();
                        return;
                    }
                    

                    std::cv_status ret = notEmpty_.wait_for(loc, std::chrono::seconds(1));
                    if(ret == std::cv_status::timeout){
                        //超时返回， 获取当前时间
                        auto nowTime = std::chrono::high_resolution_clock().now(); 
                        auto dur = std::chrono::duration_cast<std::chrono::seconds>(nowTime - lastTime);

                        if(dur.count() >= THREAD_MAX_IDLE_TIME){
                            //开始回收当前线程
                            //把线程对象从列表容器中删除(通过 threadid)
                            threads_.erase(threadid);
                            curThreadSize_--;
                            idleThreadSize_--;
                            std::cout<<"thread: ["<<threadid<<"] is reclaimed"<<std::endl;

                            return;

                        }

                    }
                }



            }else{
                // //线程回收时这里死锁了
                // notEmpty_.wait(loc, [&]()->bool{
                //     // return taskQue_.size()>0 ; //如果线程池要销毁了，任务不会再大于0，会一直阻塞（死锁）
                //     return taskQue_.size()>0 || !isPoolRunning_; //如果线程池要销毁，就让他不阻塞，出去，（解决死锁）
                // });

                while(taskQue_.size()==0){
                    //线程池结束
                    if(!isPoolRunning_){
                        threads_.erase(threadid);

                        std::cout<<"thread : ["<<threadid<<"] exit"<<std::endl;
                        curThreadSize_--;
                        idleThreadSize_--;
                        exit_.notify_all();
                        return;
                    }

                    notEmpty_.wait(loc);
                }
                
            }


            // //线程池结束
            // if(!isPoolRunning_){
            //     threads_.erase(threadid);

            //     std::cout<<"thread : ["<<threadid<<"] exit"<<std::endl;
            //     curThreadSize_--;
            //     idleThreadSize_--;
            //     exit_.notify_all();
            //     return;
            // }

            //空闲线程数增加
            idleThreadSize_--;

            //取出
            task = taskQue_.front();
            taskQue_.pop();
            taskSize_--;

            //如果还有任务， 通知其他线程取任务
            if(!taskQue_.empty()){
                notEmpty_.notify_all();
            }

            //唤醒因为队列满而等待的线程
            notFull_.notify_all();
        } //加作用域是为了锁的释放(通过unique_lock调用析构)

        std::cout<<"thread: ["<<threadid<<"] get a task"<<std::endl;

        if(task!=nullptr){
            //执行任务
            // task->run();
            task->exec();
        }
        std::cout<<"thread: ["<<threadid<<"] complete a task"<<std::endl;
        //空闲线程数增加
        idleThreadSize_++;
        //更新线程执行任务的时间
        lastTime = std::chrono::high_resolution_clock().now();
    }
    
    //线程池关闭需要回收线程资源
    std::cout<<"thread : ["<<threadid<<"] exit"<<std::endl;
    curThreadSize_--;
    idleThreadSize_--;
    exit_.notify_all();

    

}

/////////////// Task方法实现


void Task::exec(){
    if(result_ != nullptr){
        result_->setVal(run());
    }
    
}

void Task::setResult(Result *result){
    result_ = result;
}

Task::Task():result_(nullptr){}





///////////// Thread   线程方法
int Thread::generateId_ = 0;

int Thread::getId() const{
    return threadId_;
}
//线程构造
Thread::Thread(ThreadFunc _func):
    func_(_func),
    threadId_(generateId_++)
    {

}
//线程析构
Thread::~Thread(){

}

//启动线程
void Thread::start(){

    //创建一个线程函数为func_
    std::thread t(func_, threadId_);
    //设置分离线程
    t.detach();
}




///////---------------Result
Result::Result(std::shared_ptr<Task> task, bool isVaild):
    task_(task), isVaild_(isVaild){
        task_->setResult(this);
    }

//get方法，[用户调用]这个方法获取task的返回值
Any Result::get(){
    if(!isVaild_){
        //返回值无效
        return "";
    }else{
        //申请获取
        sem_.wait(); //task任务没有执行完会阻塞用户线程
        return std::move(any_);
    }
}


// setVal方法，获取任务执行完的返回值  [task调用]
void Result::setVal(Any any){
    this->any_ = std::move(any);
    sem_.post();//已经获取的任务的返回值，增加信号量资源
}
