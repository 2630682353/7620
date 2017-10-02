CUR_PATH :=   ./ft_processor
OBJS 	 += $(patsubst %.cpp, %.o, $(wildcard $(CUR_PATH)/*.cpp)) 
