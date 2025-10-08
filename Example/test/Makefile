IDIR = ../include
ODIR = ../obj
SDIR = ../src
BDIR = ../bin
TDIR = ../test
SCRIPT_DIR = ./script

CXX = g++
CXXFLAGS = -I${IDIR} -lgtest -lpthread -g -pg

# SOURCE使用wildcard函数匹配SRC目录下的.cpp文件
SOURCE = $(wildcard $(SDIR)/*.cpp)			
# OBJECT使用patsubst函数将SOURCE对应的.cpp文件的后缀替换成.o				
OBJECT = $(patsubst %.cpp, ${ODIR}/%.o, $(notdir ${SOURCE}))
# 删除原来程序的main
TEST_OBJECT = $(patsubst ${ODIR}/main.o, ${ODIR}/test.o, ${OBJECT})

gtest: ${TEST_OBJECT}
	${CXX} -o gtest.out $^ ${CXXFLAGS}

$(ODIR)/%.o: $(SDIR)/%.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@ 

$(ODIR)/%.o: $(TDIR)/%.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@ 

.PHONY:clean debug
clean:  
	rm -f $(ODIR)/*.o
	rm -rf $(BDIR)/*.out
	rm -f $(TDIR)/*.out $(TDIR)/*.txt $(TDIR)/*.png $(TDIR)/*.out
	rm -rf gmon.out

debug:
	echo ${SOURCE}
	echo ${OBJECT}
	echo ${TEST_OBJECT}


