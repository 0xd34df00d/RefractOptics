#include "util.h"

TrainingSet_t LoadData (const std::string& file)
{
	TrainingSet_t pairs;

	std::ifstream istr (file);
	while (istr)
	{
		double lambda, n;
		char c;
		istr >> lambda >> c >> n;
		if (!istr)
			break;

		SampleType_t sample;
		sample (0) = lambda;

		pairs.push_back ({ sample, n });
	}

	return pairs;
}
