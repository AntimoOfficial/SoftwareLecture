// CPU占用过高
#include "animal.h"

class Tiger: public Animal{
    virtual void Eat() override;
    virtual void Drink() override;
    virtual void Climb() override;
public:
    virtual string Name() override;
};