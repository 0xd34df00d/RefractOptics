#include <fstream>
#include <iostream>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <future>
#include <limits>
#include <dlib/svm.h>

typedef dlib::matrix<double, 1, 1> SampleType_t;

const size_t ParamsCount = 3;
typedef dlib::matrix<double, ParamsCount, 1> Params_t;

namespace Series
{
	double residual (const std::pair<SampleType_t, double>& data, const Params_t& p)
	{
		double result = 0;
		const auto x = data.first (0);
		for (size_t i = 0; i < ParamsCount; ++i)
			result += p (i) / std::pow (x, 2 * i);
		return result - data.second;
	}

	Params_t residualDer (const std::pair<SampleType_t, double>& data, const Params_t& p)
	{
		Params_t res;
		const auto x = data.first (0);
		for (size_t i = 0; i < ParamsCount; ++i)
			res (i) = 1 / std::pow (x, 2 * i);
		return res;
	}
}

namespace Resonance
{
	double residual (const std::pair<SampleType_t, double>& data, const Params_t& p)
	{
		const auto x = data.first (0);
		const auto x2 = x * x;

		const double cminus = p (2) - 1.0 / x2;
		const double underRoot = p (0) + p (1) / cminus;
		const double root = underRoot >= 0 ? std::sqrt(underRoot) : std::numeric_limits<double>::max () / 10;

		return root - data.second;
	}

	Params_t residualDer (const std::pair<SampleType_t, double>& data, const Params_t& p)
	{
		const auto x = data.first (0);
		const auto x2 = x * x;

		const double cminus = p (2) - 1.0 / x2;
		const double underRoot = p (0) + p (1) / cminus;
		const double root = underRoot >= 0 ? std::sqrt(underRoot) : std::numeric_limits<double>::max () / 10;

		Params_t res;
		res (0) = 1. / (2. * root);
		res (1) = 1. / (2. * root * cminus);
		res (2) = -(p (1) / (2 * root * cminus * cminus));
		return res;
	}
}

using namespace Resonance;

Params_t solve (const std::vector<std::pair<SampleType_t, double>>& pairs)
{
	Params_t p;
	for (size_t i = 0; i < ParamsCount; ++i)
		p (i) = 1;
	dlib::solve_least_squares_lm (dlib::objective_delta_stop_strategy(1e-5),
			residual, residualDer, pairs, p);
	return p;
}

typedef std::vector<std::vector<double>> StatsVec_t;

StatsVec_t getStats (double lVar, double nVar,
		const std::vector<std::pair<SampleType_t, double>>& pairs)
{
	std::random_device generator;
	std::normal_distribution<double> lambdaDistr (0, lVar);
	std::normal_distribution<double> nDistr (0, nVar);

	StatsVec_t stats;
	stats.resize (ParamsCount);
	for (size_t i = 0; i < 10000; ++i)
	{
		std::vector<std::pair<SampleType_t, double>> localPairs = pairs;
		for (auto& pair : localPairs)
		{
			pair.first (0) += lambdaDistr (generator);
			pair.second += nDistr (generator);
		}

		const auto& p = solve (localPairs);
		for (size_t j = 0; j < ParamsCount; ++j)
			stats [j].push_back (p (j));
	}
	return stats;
}

std::string format (double value)
{
	std::ostringstream ostr;
	ostr.precision (3);
	if (value > 1)
	{
		ostr << value;
		return ostr.str ();
	}
	if (value < 1e-7)
		return "0";

	int count = 0;
	if (value < 0.001)
		while (value < 1)
		{
			value *= 10;
			++count;
		}

	if (value != 1 | !count)
	{
		ostr << value;
		if (count)
			ostr << " \\cdot ";
	}
	if (count)
	{
		ostr << "10^{-";
		ostr << count;
		ostr << "}";
	}
	return ostr.str ();
}

int main (int argc, char **argv)
{
	if (argc < 2)
	{
		std::cout << "Usage: " << argv [0] << " datafile" << std::endl;
		return 1;
	}

	const std::string infile (argv[1]);

	std::vector<SampleType_t> samples;
	std::vector<double> labels;
	std::vector<std::pair<SampleType_t, double>> pairs;

	std::ifstream istr (infile);
	while (istr)
	{
		double lambda, n;
		char c;
		istr >> lambda >> c >> n;

		SampleType_t sample;
		sample (0) = lambda;

		samples.push_back (sample);
		labels.push_back (n);
		pairs.push_back ({ sample, n });
	}

	std::cout << "read " << samples.size () << " samples: " << std::endl;

	{
		const auto& p = solve (pairs);
		std::cout << "inferred params: " << dlib::trans (p) << std::endl;

		double sum = 0;
		for (const auto& pair : pairs)
		{
			auto r = residual (pair, p);
			sum += r * r;
		}
		std::cout << "MSE: " << sum / pairs.size () << std::endl;
	}

	std::cout << "calculating mean/dispersion..." << std::endl;

	std::vector<double> lVars { 0, 0.1, 0.3, 1, 3 };
	std::vector<double> nVars { 0, 1e-5, 2e-5, 3e-5, 4e-5, 5e-5, 6e-5, 7e-5, 8e-5, 9e-5,
			1e-4, 2e-4, 3e-4, 4e-4, 5e-4, 6e-4, 7e-4, 8e-4, 9e-4, 1e-3 };
	/*
	std::vector<double> nVars { 0, 1e-5, 2e-5, 5e-5, 1e-4, 2e-5, 5e-4, 1e-3, 2e-3, 5e-3,
			7e-3, 0.01, 0.01, 0.03, 0.04, 0.05, 0.06, 0.07, 0.08, 0.09, 0.1 };
			*/

	std::mutex mapMutex;
	std::map<double, std::map<double, std::vector<dlib::running_stats<double>>>> results;
	for (auto lVar : lVars)
	{
		for (auto i = nVars.begin (); i != nVars.end (); )
		{
			std::map<double, std::future<StatsVec_t>> nVar2future;
			for (size_t t = 0; i != nVars.end () && t < std::thread::hardware_concurrency (); ++i, ++t)
				nVar2future [*i] = std::async (std::launch::async, getStats, lVar, *i, pairs);

			for (auto& pair : nVar2future)
			{
				const auto& coeffs = pair.second.get ();
				std::vector<dlib::running_stats<double>> stats;
				stats.resize (coeffs.size ());
				for (size_t i = 0; i < coeffs.size (); ++i)
					for (auto coeff : coeffs [i])
						stats [i].add (coeff);
				results [lVar] [pair.first] = stats;
			}
		}
	}

	/*
	for (size_t i = 0; i < ParamsCount; ++i)
	{
		std::cout << "\\begin{tabular}{| l ";
		for (size_t i = 0; i < nVars.size (); ++i)
			std::cout << "| l ";
		std::cout << "|} \\hline\n";
		for (auto nVar : nVars)
			std::cout << " & $" << format (nVar) << "$";
		std::cout << "\\\\ \\hline\n";

		std::cout.precision (3);

		for (auto lVar : lVars)
		{
			std::cout << format (lVar);
			for (auto nVar : nVars)
			{
				const auto& stats = results [lVar] [nVar];
				std::cout << " & ";
				std::cout << "$\\displaystyle (" << format (stats [i].mean ()) << "; " << format (stats [i].stddev ()) << ")$";
			}
			std::cout << "\\\\ \\hline\n";
		}
		std::cout << "\\end{tabular}" << std::endl << std::endl;
	}
	*/

	for (size_t i = 0; i < ParamsCount; ++i)
	{
		std::stringstream fname;
		fname << infile << "_coeff" << i << ".dat";

		std::ofstream ostr (fname.str ());
		for (auto lVar : lVars)
		{
			for (auto nVar : nVars)
			{
				const auto& stats = results [lVar] [nVar];
				ostr << lVar << " " << nVar << " " << stats [i].stddev () << std::endl;
			}
			ostr << std::endl;
		}
		std::cout << "wrote " << fname.str () << std::endl;
	}
}
