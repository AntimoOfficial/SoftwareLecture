// 内存泄漏
#include "animal.h"

class Mouse: public Animal{
    virtual void Eat() override;
    virtual void Drink() override;
    virtual void Climb() override;
public:
    virtual string Name() override;
};