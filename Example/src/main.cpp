#include "mouse.h"
#include "tiger.h"
#include "wolf.h"
#include <vector>

using std::vector;

// 采用多态来构造动物的集合--动物园
vector<Animal*> zoo;

// 初始化全局变量
void init(){
    Animal* animal = new Mouse();
    zoo.push_back(animal);
    animal = new Tiger();
    zoo.push_back(animal);
    animal = new Wolf();
    zoo.push_back(animal);
}

int main(){
    // 开始性能炸弹
    init();
    for(int i = 0; i < 100; i++)
        for(auto animal : zoo)
            animal->Live();
    return 0;
}