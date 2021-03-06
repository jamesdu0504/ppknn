#include <iostream>
#include <string.h>

#include <liphe/carray_iterator.h>
#include <liphe/helib_number.h>
#include <liphe/eq.h>
#include <liphe/polynomial.h>
#include <liphe/cmp.h>
#include <liphe/binomial_tournament.h>
#include <liphe/unsigned_word.h>
#include <liphe/average.h>

#include "get_percentile_framework.h"
#include "get_percentile.h"
#include "special_polynomials.h"

typedef UnsignedWord<18, HelibNumber> HelibBits;


template<class Number>
Polynomial<Number> SpecialPolynomials<Number>::square_msd_polynomial;

template<class Number>
Polynomial<Number> SpecialPolynomials<Number>::sqrt_msd_polynomial;

template<class Number>
Polynomial<Number> SpecialPolynomials<Number>::sqrt_polynomial;

template<class Number>
Polynomial<Number> SpecialPolynomials<Number>::is_positive_polynomial;

//template<class Number>
//std::vector<Polynomial<Number> > SpecialPolynomials<Number>::convert_to_bit;



int main(int argc, char**argv) {
	measureAccuracy = false;
	MAX_CANDIDATES = 1;
	RETRIES = 1;

	initialize(argc, argv);

	HelibKeys keys;

	long R = 1;
	long d = 1;
	long c = 2;
	long k = keySize;
	long s = 0;
	long chosen_m = 0;
	Vec<long> gens;
	Vec<long> ords;

	keys.initKeys(s, R, p, r, d, c, k, 64, L, chosen_m, gens, ords);
	HelibNumber::set_global_keys(&keys);

	SpecialPolynomials<HelibNumber>::init_polynomials(p);

//	secure_geo_search<HelibNumber, HelibBits>(rawDiscreteData, discreteQuery);
	secure_knn_classifier<HelibNumber, HelibBits>(rawDiscreteData, rawDataClasses, discreteQuery);
	return 0;
}

