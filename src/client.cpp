#include<iostream>
#include<thread>
#include<mutex>

std::mutex _mutex;

void f(int n) {
    std::unique_lock<std::mutex> lock(_mutex);
    for(int i=0; i<n; ++i) {
        std::cout<<"Thread "<<std::this_thread::get_id()<<" ";
        std::cout<<i<<"\n";
    }
}
int main() {
    int n = 10;
    std::thread t(f, n);
    f(n);
    t.join();
    return 0;
}