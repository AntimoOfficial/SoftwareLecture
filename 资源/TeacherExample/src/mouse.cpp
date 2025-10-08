// 内存泄漏
#include "mouse.h"
#include <iostream>

using std::cout;
using std::endl;

string Mouse::Name(){
    return "Mouse";
}

void Mouse::Eat(){
    cout<< Name()<< " eat"<<endl;
}

void Mouse::Climb(){
    cout<< Name()<< " climb"<<endl;
}

void Mouse::Drink(){
    cout<< Name()<< " drink"<<endl;
    for(int i = 0; i < 10; i++){
        int* leak = new int;
    }
}