// 长循环，占用计算资源
#include "tiger.h"
#include <iostream>

using std::cout;
using std::endl;

string Tiger::Name(){
    return "Tiger";
}

void Tiger::Eat(){
    cout<< Name()<< " eat"<<endl;
    // long long loop = 10000000000;
    long long loop = 1000000;
    long long nothing = 0;
    for(long long i = 0; i < loop; i++){
        nothing++;
    }
}

void Tiger::Climb(){
    cout<< Name()<< " climb"<<endl;
}

void Tiger::Drink(){
    cout<< Name()<< " drink"<<endl;
}