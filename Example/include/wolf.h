// 大内存占用
#include "animal.h"
#include <vector>

using std::vector;

struct BigMem{
    char bigMem[1024];
};


class Wolf: public Animal{
public:
    virtual string Name() override;
private:
    virtual void Eat() override;
    virtual void Drink() override;
    virtual void Climb() override;
private:
    vector<BigMem> memVec;
};