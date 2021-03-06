#ifndef ___GET_PERCENTILE_FRAMEWORK___
#define ___GET_PERCENTILE_FRAMEWORK___

#include <sys/resource.h>
#include <sys/types.h>
#include <sys/time.h>
#include <vector>
#include <thread>

#include <time_measurements.h>

#define DONT_USE_SIMD

#include "polynomial.h"
#include "get_percentile.h"
#include "special_polynomials.h"
#include "input_database.h"

extern int maxDistance;


TakeTimes global_timer;



void print_stat_prefix(int i) {
	std::cout << "After ";

	if (i == -1)
		std::cout << "all";
	else
		std::cout << i;

	std::cout << " ";
}

int get_mem() {
	struct rusage rus;
	int res;

	res = getrusage(RUSAGE_SELF, &rus);
	if (res == -1) {
		perror("rusage");
		return 0;
	}

	return rus.ru_maxrss / 1024;
}

int realAvg;
int realAvgSqr;
int n;

void print_stat(int i) {
	print_stat_prefix(i);
	std::cout << global_timer.stats("Everything") << std::endl;

	print_stat_prefix(i);
	std::cout << "Used " << get_mem() << " MegaBytes" << std::endl;
}

template<class NUMBER>
void print_detailed_stats(int step, NUMBER &avgTemp, NUMBER &avgSqrMsdTemp, NUMBER &avgSqrLsdTemp) {
	int avg = avgTemp.to_int();
	int avgSqrLsd = avgSqrLsdTemp.to_int();   // this is x mod p
	int avgSqrMsd = avgSqrMsdTemp.to_int();   // this is x/2^a

	int avgSqr = avgSqrMsd *p + avgSqrLsd;

	print_stat_prefix(step);
	std::cout << std::endl;
	std::cout << "Avg " << avg << " / " << (realAvg / n) << " = " << ((double)avg * n / realAvg) << std::endl;
	std::cout << "AvgSqrLsd = " << avgSqrLsd << std::endl;
	std::cout << "AvgSqrMsd = " << avgSqrMsd << std::endl;
	std::cout << "AvgSqr " << avgSqr << " / " << (realAvgSqr / n) << " = " << ((double)avgSqr * n / realAvgSqr) << std::endl;

}

inline int sqr(int x) { return x*x; }

unsigned int statistic_rate = 10;


template<class Number>
Number fold(Number n) {
#	ifdef DONT_USE_SIMD
	return n;
#	endif

	AddBinomialTournament<Number> ret;

	int end = n.simd_factor();
	int simd_factor = n.simd_factor();
	int batch = 1;
	while (simd_factor != 0) {
		if ((simd_factor & 1) != 0) {
			ret.add_to_tournament( n.rotate_left(end - batch) );
			end -= batch;
		}

		n += n.rotate_left(batch);

		simd_factor >>= 1;
		batch <<= 1;
	}

	return ret.unite_all();
}

int popcnt(int i) {
	i = ((i >> 1) & 0x5555) + (i & 0x5555);
	i = ((i >> 2) & 0x3333) + (i & 0x3333);
	i = ((i >> 4) & 0x0f0f) + (i & 0x0f0f);
	i = ((i >> 8) & 0x00ff) + (i & 0x00ff);
	return i;
}

//template<class Number>
//Number fold_power_2(Number n, int simd_factor) {
//#	ifdef DONT_USE_SIMD
//	return n;
//#	endif
//
//	assert(popcnt(simd_factor) == 1);
//
//	Number ret;
//
//	Number *a = &n;
//	Number *b = &ret;
//
//	int iter = simd_factor;
//	while (iter > 1) {
//		std::vector<long int> mask_left;
//		std::vector<long int> mask_right;
//
//		for (int i = 0; i < simd_factor; ++i) {
//			if ((i / (iter/2)) & 1) {
//				mask_left[i] = 0;
//				mask_rightt[i] = 1;
//			} else
//				mask_left[i] = 1;
//				mask_rightt[i] = 0;
//			}
//		}
//
//		*b = (a->shift_left(iter/2) * Number::from_vector(mask_right)) + (a->shift_right(iter/2) * Number::from_vector(mask_left));
//
//		iter /= 2;
//
//		Number *temp = a;
//		a = b;
//		b = temp;
//	}
//
//	return *b;
//}

template<class Number, class NumberBits>
void multithreaded_average(Distances<Number, NumberBits> &_x, std::function<int(int)> f_m, Number &output) {
	std::thread *thread = new std::thread[thread_num];
	AverageLiphe<Number, Number, CompareNative<Number>, NoConversion<Number> > avg;
	std::mutex mutex;

	avg.set_n(_x.size());
	avg.set_max_sample(maxDistance);
	avg.compute_resample_constant(0.1, 0.05);

	if ((bool)f_m)
		avg.set_f_m( f_m );


	for (int thread_i = 0; thread_i < thread_num; ++thread_i) {
		thread[thread_i] = std::thread( [thread_i, &avg, &_x, &mutex](){
			typename Distances<Number, NumberBits>::Iterator input = _x.begin();
			input += thread_i;

			while (input != _x.end()) {
				Number xi = *input;
#				ifdef USE_SIMD
				avg.add_simd(xi, &mutex);
#				else
				avg.add(xi, &mutex);
#				endif

				++input;
			}
		} );
	}

	output = 0;
	for (int thread_i = 0; thread_i < thread_num; ++thread_i) {
		thread[thread_i].join();
	}
	output = avg.getAverage();

	delete[] thread;
}

template<class Number, class NumberBits>
void multithreaded_averages(Distances<Number, NumberBits> &_x, Number &avg, Number &sqrMsd, Number &sqrLsd) {
	AverageLiphe<Number, Number, ComparePoly<Number>, NoConversion<Number> > avgAvg;
	AverageLiphe<Number, Number, ComparePoly<Number>, NoConversion<Number> > avgAvgSqrMsd;
	AverageLiphe<Number, Number, ComparePoly<Number>, NoConversion<Number> > avgAvgSqrLsd;

	std::mutex mutexAvg;
	std::mutex mutexAvgSqrMsd;
	std::mutex mutexAvgSqrLsd;

	avgAvg.set_n(_x.size());
	avgAvg.set_max_sample(avg.p());
	avgAvg.compute_resample_constant();

	avgAvgSqrMsd.set_n(_x.size());
	avgAvgSqrMsd.set_max_sample(sqrMsd.p());
	avgAvgSqrMsd.set_f_m( [](int m)->int{ return ::sqrt(m*p); } );
	avgAvgSqrMsd.compute_resample_constant();

	avgAvgSqrLsd.set_n(_x.size());
	avgAvgSqrLsd.set_max_sample(sqrLsd.p());
	avgAvgSqrLsd.set_f_m( [](int m)->int{ return ::sqrt(m); } );
	avgAvgSqrLsd.compute_resample_constant();


	ThreadPool threads;

	{
		AutoTakeTimes tt("populating cache");

		typename Distances<Number, NumberBits>::Iterator input = _x.begin();
		input.set_thread_pool(&threads);

		// populate the cache
		while (input != _x.end()) {
			while (!threads.has_free_cpu())
				threads.process_jobs(1);

			std::shared_ptr<Number> xi(new Number(*input));

			++input;
		}
		threads.process_jobs();
	}


	{
		AutoTakeTimes tt("computing averages");

		typename Distances<Number, NumberBits>::Iterator input = _x.begin();
		while (input != _x.end()) {
			while (!threads.has_free_cpu())
				threads.process_jobs(1);

			std::shared_ptr<Number> xi(new Number(*input));

			avgAvg.add(xi, &mutexAvg, &threads);
			avgAvgSqrMsd.add(xi, &mutexAvgSqrMsd, &threads);
			avgAvgSqrLsd.add(xi, &mutexAvgSqrLsd, &threads);

			++input;
		}
		threads.process_jobs();
	}


	{
		AutoTakeTimes tt("getting averages");

		avg = avgAvg.getAverage();
		sqrMsd = avgAvgSqrMsd.getAverage();
		sqrLsd = avgAvgSqrLsd.getAverage();
	}
}


#define MULTI_THREADED

inline float sqr(float x) { return x*x; }

void print_histogram(const std::vector<Point2D<int> > &sites, const std::vector<int> &classes, const Point2D<int> &query) {
	int avg = 0;
	for (auto i = sites.begin(); i != sites.end(); ++i) {
		int dist = abs(i->x - query.x) + abs(i->y - query.y);
		avg += dist;
	}
	avg /= sites.size();

	int sigma = 0;
	for (auto i = sites.begin(); i != sites.end(); ++i) {
		int dist = abs(i->x - query.x) + abs(i->y - query.y);
		sigma += (dist - avg) * (dist - avg);
	}
	sigma /= sites.size();
	sigma = sqrt(sigma);

	std::cout << "avg = " << avg << std::endl;
	std::cout << "sigma = " << sigma << std::endl;

	std::cout << "Histogram:" << std::endl;
	std::vector<int> distribution_test(20);
	std::vector<int> distribution_test_zero(20);
	std::vector<int> distribution_test_one(20);

	for (auto i = distribution_test.begin(); i != distribution_test.end(); ++i)
		*i = 0;
	distribution_test_zero = distribution_test;
	distribution_test_one = distribution_test;

	int i_site = 0;
	for (auto i = sites.begin(); i != sites.end(); ++i, ++i_site) {
		int dist = abs(i->x - query.x) + abs(i->y - query.y);
		int bucket = (dist - avg) / ((sigma+2)/3) + 10;

		if (bucket > 18)
			bucket = 18;
		if (bucket < 0)
			bucket = 0;

		++distribution_test[bucket];
		if (classes[i_site] == 0)
			++distribution_test_zero[bucket];
		else
			++distribution_test_one[bucket];
	}

	for (unsigned int bucket = 0; bucket < distribution_test.size(); ++bucket) {
		std::cout << ((bucket - 10) * ((sigma+2)/3) + avg)  << ": " << distribution_test[bucket] <<
		" = " << distribution_test_zero[bucket] << " + " << distribution_test_one[bucket] << std::endl;
	}
}

int real_knn_classifier(const std::vector<Point2D<int> > &sites, const std::vector<int> &classes, const Point2D<int> &query) {
	print_histogram(sites, classes, query);

	std::vector<float> distances;
	for (auto i_sites = sites.begin(); i_sites != sites.end(); ++i_sites) {
		float dist = sqr((*i_sites).x - query.x) + sqr((*i_sites).y - query.y);
		distances.push_back(dist);
	}
	std::sort(distances.begin(), distances.end());

	float threshold = distances[distances.size() * 0.02];

	int classOne = 0;
	int classZero = 0;

	for (unsigned int i_sites = 0; i_sites < sites.size(); ++i_sites) {
		float dist = sqr(sites[i_sites].x - query.x) + sqr(sites[i_sites].y - query.y);
		if (dist < threshold) {
			if (classes[i_sites] == 0)
				++classZero;
			else if (classes[i_sites] == 1)
				++classOne;
			else {
				std::cerr << "Error: classes should be 0s of 1s\n";
				exit(1);
			}
		}
	}

	std::cout << "real count of class 0: " << classZero << std::endl;
	std::cout << "real count of class 1: " << classOne << std::endl;

	return (classZero > classOne) ? 0 : 1;
}


bool OK = true;

template<class Number, class NumberBits>
void secure_knn_classifier_gaussian(const std::vector<Point2D<int> > &sites, const std::vector<int> &classes, const Point2D<int> &query, std::vector<int> &classZeroCountVector, std::vector<int> &classOneCountVector) {

	std::cout << "Starting classifier KNN" << std::endl;

	Distances<Number, NumberBits> distances(sites, classes, query);

	Number avgValue;
	Number avgSqrValue;

	n = sites.size();

	////////////////////////////////////
	// Start computing average and average of squares
	////////////////////////////////////

	Number avgEnc;
	Number avgSqrMsdEnc;
	Number avgSqrLsdEnc;

	{
		AutoTakeTimes tt("computing averages");

		global_timer.start();
		multithreaded_averages<Number, NumberBits>(distances, avgEnc, avgSqrMsdEnc, avgSqrLsdEnc);
		std::cout << global_timer.end("computing averages");
	}


	////////////////////////////////////
	// End computing average and average of squares
	////////////////////////////////////



	////////////////////////////////////
	// start computing threshold
	////////////////////////////////////

	std::vector<Number> thresholdCandidates;
	{
		ThreadPool threads;

		global_timer.start();

		Number avgLsdEnc = avgEnc * avgEnc;
		Number avgMsdEnc;
		{
			AutoTakeTimes tt("computing avg sqaure");
			SpecialPolynomials<Number>::square_msd_polynomial.compute(avgMsdEnc, avgEnc, &threads);
		}

		Number sigma;
		{
			AutoTakeTimes tt("computing sigma");
			sigma = SpecialPolynomials<Number>::sqrt_msd_polynomial.compute( avgSqrMsdEnc - avgMsdEnc, &threads );
			sigma += SpecialPolynomials<Number>::sqrt_polynomial.compute( avgSqrLsdEnc - avgLsdEnc, &threads );
		}

		int inv2 = power_mod(2, phi(Number::get_global_ring_size()) - 1, Number::get_global_ring_size());
		std::cout << "2^{-1} mod " << Number::get_global_ring_size() << " = " << inv2 << std::endl;

		{
			{
				Number threshold = avgEnc;
				thresholdCandidates.push_back(threshold);
			}
			{
				Number threshold = avgEnc - sigma;
				thresholdCandidates.push_back(threshold);
			}
			{
				Number threshold = avgEnc - sigma*2;
				thresholdCandidates.push_back(threshold);
			}
			{
				Number threshold = avgEnc - sigma*inv2;
				thresholdCandidates.push_back(threshold);
			}
			{
				Number threshold = avgEnc - (sigma+1)*inv2;
				thresholdCandidates.push_back(threshold);
			}

		}


		std::cout << global_timer.end("compute threashold candidates");




		// debugging messages

		int realAvg = 0;
		int realAvgSqr = 0;
		for (auto i = sites.begin(); i != sites.end(); ++i) {
			int dist = abs((*i).x - query.x) + abs((*i).y - query.y);
			realAvg += dist;
			realAvgSqr += dist*dist;
		}
		realAvg /= sites.size();
		realAvgSqr /= sites.size();

		std::cout << "avg = " << avgEnc.to_int()  << "         (real = " << realAvg << ")" << std::endl;

		std::cout << "avg^2 = " << (avgMsdEnc.to_int()*p + avgLsdEnc.to_int()) << "        (real = " << (realAvg*realAvg) << ")" << std::endl;
		std::cout << "avg^2 / p = " << avgMsdEnc.to_int() <<  "            (real = " << (realAvg*realAvg / p) << ")" << std::endl;
		std::cout << "avg^2 % p = " << avgLsdEnc.to_int() << "             (real = " << (realAvg*realAvg % p) << ")" << std::endl;

		std::cout << "avgSqr = " << (avgSqrMsdEnc.to_int()*p + avgSqrLsdEnc.to_int()) << "          (real = " << realAvgSqr << ")" << std::endl;
		std::cout << "avgSqr / p = " << avgSqrMsdEnc.to_int() << "        (real = " << (realAvgSqr / p) << ")" << std::endl;
		std::cout << "avgSqr % p = " <<  avgSqrLsdEnc.to_int() << "        (real = " << (realAvgSqr % p) << ")" << std::endl;


		std::cout << "sigma = sqrt_msd(" << (avgSqrMsdEnc - avgMsdEnc).to_int() << ") + " << "sqrt_lsd(" << (avgSqrLsdEnc - avgLsdEnc).to_int() << ")" << std::endl;
		std::cout << "sigma = " << sigma.to_int() << "            (real = " << ::sqrt(-realAvg*realAvg + realAvgSqr) << ")" << std::endl;
		for (unsigned int i_candidate = 0; i_candidate < thresholdCandidates.size(); ++i_candidate)
			std::cout << "Threshold candidate " << i_candidate << " = " << thresholdCandidates[i_candidate].to_int() << std::endl;

	}

	////////////////////////////////////
	// end computing threshold
	////////////////////////////////////



	////////////////////////////////////
	// start counting classes
	////////////////////////////////////


	std::vector<Number> classOneCountEnc(thresholdCandidates.size());
	std::vector<Number> classZeroCountEnc(thresholdCandidates.size());

	for (unsigned int i_candidate = 0; i_candidate < thresholdCandidates.size(); ++i_candidate) {
		classOneCountEnc[i_candidate] = 0;
		classZeroCountEnc[i_candidate] = 0;
	}

	auto dist = distances.begin();
	while (dist != distances.end()) {
		Number xi = dist.getDistances();
		std::vector<long int> classZero = dist.getClass(0);
		std::vector<long int> classOne = dist.getClass(1);

		{
			for (unsigned int i_candidate = 0; i_candidate < thresholdCandidates.size(); ++i_candidate) {
				global_timer.start();

				Number knnEnc = ComparePoly<Number>(xi) < thresholdCandidates[i_candidate];
				Number knnClassOneEnc = knnEnc * classOne;
				Number knnClassZeroEnc = knnEnc * classZero;

				classOneCountEnc[i_candidate] += knnClassOneEnc;
				classZeroCountEnc[i_candidate] += knnClassZeroEnc;
				std::cout << global_timer.end("count classes for one candidate for one batch of points");

				// debugging
				std::cout << "The KNN indicator vector for candidate " << thresholdCandidates[i_candidate].to_int() << std::endl;
				std::vector<long int> knn = knnEnc.to_vector();
				std::vector<long int> knnClassZero = knnClassZeroEnc.to_vector();
				std::vector<long int> knnClassOne = knnClassOneEnc.to_vector();
				std::vector<long int> plaintextDistances = dist.getPlaintextDistances();
				std::vector<long int> plaintextClasses = dist.getPlaintextClasses();
				for (unsigned int i = 0; (i < knn.size()) && (i < sites.size()); ++i) {
					std::cout << dist.loc() << ", " << i << ") " << "x = " << knn[i] << "   dist = " << plaintextDistances[i] << "    class = " << plaintextClasses[i]
						<< " class zero: " << knnClassZero[i] <<  " class one: " << knnClassOne[i] << std::endl;
					if ((knnClassZero[i] != 0) && (knnClassZero[i] != 1))
						OK = false;
					if ((knnClassOne[i] != 0) && (knnClassOne[i] != 1))
						OK = false;
				}
			}
		}

		++dist;
	}

//	std::cout << "depth = mul " << classOneCountEnc0.mul_depth() << " add " << classOneCountEnc0.add_depth() << std::endl;

	if (!OK) {
		std::cout << "test is wrong" << std::endl;
		exit(1);
	}

	classZeroCountVector.resize(thresholdCandidates.size());
	classOneCountVector.resize(thresholdCandidates.size());
	for (unsigned int i_candidate = 0; i_candidate < thresholdCandidates.size(); ++i_candidate) {
		long int classOneCount = classOneCountEnc[i_candidate].to_int();
		long int classZeroCount = classZeroCountEnc[i_candidate].to_int();

		std::cout << "candidate " << i_candidate << " secure count of class 0: " << classZeroCount << std::endl;
		std::cout << "candidate " << i_candidate << " secure count of class 1: " << classOneCount << std::endl;
		classZeroCountVector[i_candidate] = classZeroCount;
		classOneCountVector[i_candidate] = classOneCount;
	}


//	int secureKnnClassifier = (classZeroCount > classOneCount) ? 0 : 1;



	print_stat(-1);
//	return secureKnnClassifier;
}


int avgIterations = 0;

template<class Number, class NumberBits>
int secure_knn_classifier(const std::vector<Point2D<int> > &sites, const std::vector<int> &classes, const Point2D<int> &query) {
	std::vector<int> classZeroCount;
	std::vector<int> classOneCount;

	for (int i = 0; i < 5; ++i) {
		++avgIterations;
		classZeroCount.resize(0);
		classOneCount.resize(0);
		secure_knn_classifier_gaussian<Number, NumberBits>(sites, classes, query, classZeroCount, classOneCount);

		for (unsigned int sigmaFactor = 0; sigmaFactor < classOneCount.size(); ++sigmaFactor) {
			int one = classOneCount[sigmaFactor];
			int zero = classZeroCount[sigmaFactor];
			std::cout << "Iteration " << i << " sigmaFactor " << sigmaFactor << " one=" << one << "  zero=" << zero << std::endl;

			float threshold = 0.05 * sites.size();

			if (one + zero < 0.5 * threshold) {
				std::cout << "Too little neighbors" << std::endl;
				continue;
			}
			// Deal with the case where we have a small count of 1 but huge amount of 0
			if (one + zero > 2 * threshold) {
				if (one < threshold) {
					std::cout << "enough neighbors, and 1 count is very small. Classifying as 0" << std::endl;
					return 0;
				}
				if (zero < threshold) {
					std::cout << "enough neighbors, and 0 count is very small. Classifying as 1" << std::endl;
					return 1;
				}
				std::cout << "Too many neighbors" << std::endl;
				continue;
			}
			std::cout << "enough neighbors. Classifying by majority" << std::endl;
			return (one > zero) ? 1 : 0;
		}
	}

	return -1;
}

template<class Number, class NumberBits>
void test_secure_knn_classifier(const std::vector<Point2D<int> > &sites, const std::vector<int> &classes) {
	int match = 0;
	int mismatch = 0;
	int secClassificationFailed = 0;

	int secureCorrect = 0;
	int secureIncorrect = 0;

	int realCorrect = 0;
	int realIncorrect = 0;

	for (unsigned int i_query = 0; i_query < sites.size(); ++i_query) {

		std::vector<Point2D<int> > sub_sites;
		std::vector<int> sub_classes;
		for (unsigned int i_copy = 0; i_copy < sites.size(); ++i_copy) {
			if (i_copy != i_query) {
				sub_classes.push_back(classes[i_copy]);
				sub_sites.push_back(sites[i_copy]);
			}
		}

		int secKnnClass = secure_knn_classifier<Number, NumberBits>(sub_sites, classes, sites[i_query]);
		int realKnnClass = real_knn_classifier(sub_sites, sub_classes, sites[i_query]);

		std::cout << "test is OK";

		std::cout << "Secure KNN classifier classified as: " << secKnnClass << std::endl;
		std::cout << "Original KNN classifier classified as: " << realKnnClass << std::endl;

		if (secKnnClass == -1) {
			++secClassificationFailed;
			std::cout << "SecKnn failed to classify" << std::endl;
		} else {
			if (realKnnClass == secKnnClass) {
				++match;
			} else {
				std::cout << "real KNN and secure KNN mismatch" << std::endl;
				++mismatch;
			}
			if (realKnnClass == classes[i_query]) { ++realCorrect; } else { ++realIncorrect; }
			if (secKnnClass == classes[i_query]) { ++secureCorrect; } else { ++secureIncorrect; }
		}

		if (match + mismatch > 0) {
			std::cout << "OUTPUT [1]: " << "matched: " << match << " out of " << (match+mismatch) << " = " << ((int)100*match/(match+mismatch)) << "%" << std::endl;
			std::cout << "OUTPUT [2]: " << "correct secure: " << secureCorrect << " out of " << (secureCorrect+secureIncorrect) << " = " << ((int)100*secureCorrect/(secureCorrect+secureIncorrect)) << "%" << std::endl;
			std::cout << "OUTPUT [3]: " << "correct real: " << realCorrect << " out of " << (realCorrect+realIncorrect) << " = " << ((int)100*realCorrect/(realCorrect+realIncorrect)) << "%" << std::endl;
		}
		std::cout << "OUTPUT [4]: " << "classification failed: " << secClassificationFailed << std::endl;

		std::cout << "ITERATIONS: average iterations needed: " << (avgIterations / (i_query+1)) << std::endl;
	}

}


#endif
