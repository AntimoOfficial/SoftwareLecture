IDIR = ./include
ODIR = ./obj
SDIR = ./src
BDIR = ./bin
TDIR = ./test
SCRIPT_DIR = ./script

CXX = g++
CXXFLAGS = -I${IDIR} -lgtest -g -pg

# SOURCE使用wildcard函数匹配SRC目录下的.cpp文件
SOURCE = $(wildcard ${SDIR}/*.cpp)			
# OBJECT使用patsubst函数将SOURCE对应的.cpp文件的后缀替换成.o				
OBJECT = $(patsubst %.cpp, ${ODIR}/%.o, $(notdir ${SOURCE}))

TARGET = zoo_boom.out
BIN_TARGET = ${BDIR}/${TARGET}

# make 找到的第一条命令
${BIN_TARGET}: ${OBJECT}
	${CXX} -o $@ $^ ${CXXFLAGS}

# "隐式规则+自动化变量" 来生成 ".o" 目标文件
# $@代表target的名字，$<代表右侧输入的第一个名字
# 该规则展开如下：
# ./obj/foo.o : ./src/foo.c
#    ${CXX} -c ${CXXFLAGS} foo.c -o foo.o
${ODIR}/%.o: ${SDIR}/%.cpp
	${CXX} -c ${CXXFLAGS} $< -o $@ 

.PHONY:clean valgrind perf gtest gprof run debug
clean:  
     #find $(ODIR) -name *.o -exec rm -rf {} \; #这个是find命令，不懂的可以查下资料  
	rm -f ${ODIR}/*.o
	rm -rf ${BDIR}/*.out
	rm -f ${TDIR}/*.out ${TDIR}/*.txt ${TDIR}/*.png ${TDIR}/*.out
	rm -rf gmon.out output.png

valgrind:
	valgrind --tool=memcheck --leak-check=full ./bin/zoo_boom.out

perf:
	perf stat ./bin/zoo_boom.out

# 	gprof bin/zoo_boom gmon.out >report.txt
gprof:
	gprof bin/zoo_boom.out gmon.out |./script/gprof2dot.py -n0 -e0 |dot -Tpng -o output.png

run:
	./${BIN_TARGET}

debug:
	echo ${SOURCE}
	echo ${OBJECT}