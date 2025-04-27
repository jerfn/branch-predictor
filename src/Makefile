CXX		=	g++
CXXFLAGS	=	-g -O3 -Wall -std=c++17

all:		predict

predict:	predict.cc trace.cc predictor.h branch.h trace.h my_predictor.h
		$(CXX) $(CXXFLAGS) -o predict predict.cc trace.cc

clean:
		rm -f predict
