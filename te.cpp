#include"threadpool.h"
#include<iostream>
//线程池测试代码

class MyTask:public Task{
public:
    MyTask(int s_, int d_):
    s(s_), d(d_){

    }
    Any run(){
        // std::cout<<"thread: "<<std::this_thread::get_id()<<" running..."<<std::endl;
        long long sum  =0 ;
        for(int i=s; i<d; i++){
            sum+=i;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
        // std::cout<<"thread: "<<std::this_thread::get_id()<<" end"<<std::endl;
        return sum;
    }
    int s;
    int d;
};


int main(){

    

        ThreadPool pool;
    // pool.setPoolMode(PoolMode::MODE_CACHED);
    pool.start(2);

    std::shared_ptr<Task> task(new MyTask(1, 10000));

    pool.submitTask(task);
    pool.submitTask(task);
    pool.submitTask(task);
    pool.submitTask(task);
    pool.submitTask(task);
    pool.submitTask(task);
    Result res = pool.submitTask(task);
    long long sum = res.get().cast_<long long>();
    std::cout<<"====="<<sum<<"\n";
    
    pool.submitTask(task);
    pool.submitTask(task);
    pool.submitTask(task);
    pool.submitTask(task);
    pool.submitTask(task);
    pool.submitTask(task);
    
    
    


    // getchar();
    system("pause");
    return 0;
}
