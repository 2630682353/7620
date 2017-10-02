CUR_PATH :=   ./util
OBJS 	 += $(patsubst %.cpp, %.o, $(wildcard $(CUR_PATH)/*.cpp))