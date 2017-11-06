#ifndef ___ONE_MEAN___
#define ___ONE_MEAN___

#include <vector>
#include <helib_number.h>

#include "point2d.h"

extern long L;
extern long p;
extern long r;

extern int thread_num;

extern std::vector<Point2D<int> > rawDiscreteData;
extern bool measureAccuracy;

extern Point2D<int> discreteQuery;

void initialize(int argc, char **argv);

#define maxX 500
#define maxY 500

#endif
