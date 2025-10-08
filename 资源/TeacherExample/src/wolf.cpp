// 大内存占用
#include "wolf.h"
#include <iostream>

using std::cout;
using std::endl;

string Wolf::Name(){
    return "Wolf";
}

void Wolf::Eat(){
    cout<<Name()<<" eat"<<endl;
}

void Wolf::Climb(){
    cout<<Name()<<" climb"<<endl;
    long long KB = 1024; 
    long long MB = KB * 1024;
    long long GB = MB * 1024;

    for(int i = memVec.size(); i < 1024; i++){
        memVec.push_back(BigMem());
    }
}

void Wolf::Drink(){
    cout<<Name()<<" drink"<<endl;
}