#include "rational.h"
#include <libavutil/mathematics.h>


void mplRational_from_AVRational(mplRational *rat, AVRational avrat) {
	rat->num = avrat.num;
	rat->den = avrat.den;
}

void mplRational_reduce(mplRational *rat) {
	int64_t gcd = av_gcd(rat->num, rat->den);
	rat->num /= gcd;
	rat->den /= gcd;
}

double mplRational_d(const mplRational *rat) {
	return (double)rat->num / rat->den;
}
