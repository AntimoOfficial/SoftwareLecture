#pragma once

#include <string>

using std::string;

class Animal{
public:
    virtual string Name() = 0;
    void Live(){
        Eat();
        Drink();
        Climb();
    };
private:
    virtual void Eat() = 0;
    virtual void Drink() = 0;
    virtual void Climb() = 0;
};