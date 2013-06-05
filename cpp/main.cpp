#include <fstream>
#include <iostream>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <future>
#include <dlib/svm.h>

typedef dlib::matrix<double, 1, 1> SampleType_t;

const size_t PowLength = 3;
typedef dlib::matrix<double, PowLength, 1> Params_t;

double residual (const std::pair<SampleType_t, double>& data, const Params_t& p)
{
	double result = 0;
	const auto x = data.first (0);
	for (size_t i = 0; i < PowLength; ++i)
		result += p (i) / std::pow (x, 2 * i);
	return result - data.second;
}

Params_t residualDer (const std::pair<SampleType_t, double>& data, const Params_t& p)
{
	Params_t res;
	const auto x = data.first (0);
	for (size_t i = 0; i < PowLength; ++i)
		res (i) = 1 / std::pow (x, 2 * i);
	return res;
}

Params_t solve (const std::vector<std::pair<SampleType_t, double>>& pairs)
{
	Params_t p;
	for (size_t i = 0; i < PowLength; ++i)
		p (i) = 0;
	dlib::solve_least_squares_lm (dlib::objective_delta_stop_strategy(1e-5),
			residual, residualDer, pairs, p);
	return p;
}

typedef std::vector<dlib::running_stats<double>> StatsVec_t;

StatsVec_t getStats (double lVar, double nVar,
		const std::vector<std::pair<SampleType_t, double>>& pairs)
{
	std::random_device generator;
	std::normal_distribution<double> lambdaDistr (0, lVar);
	std::normal_distribution<double> nDistr (0, nVar);

	StatsVec_t stats;
	stats.resize (PowLength);
	for (size_t i = 0; i < 10000; ++i)
	{
		std::vector<std::pair<SampleType_t, double>> localPairs = pairs;
		for (auto& pair : localPairs)
		{
			pair.first (0) += lambdaDistr (generator);
			pair.second += nDistr (generator);
		}

		const auto& p = solve (localPairs);
		for (size_t j = 0; j < PowLength; ++j)
			stats [j].add (p (j));
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

int main ()
{
	std::vector<SampleType_t> samples;
	std::vector<double> labels;
	std::vector<std::pair<SampleType_t, double>> pairs;

	std::ifstream istr("data/p1.txt");
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

	/*
	{
		typedef dlib::radial_basis_kernel<SampleType_t> Kernel_t;

		dlib::svr_trainer<Kernel_t> trainer;
		trainer.set_kernel (Kernel_t (2e-6));
		trainer.set_c (1);
		trainer.set_epsilon_insensitivity (1e-9);

		auto df = trainer.train (samples, labels);
		std::cout << "alpha: " << df.alpha << "; b: " << df.b << "; svs: " << df.basis_vectors.size() << std::endl;

		dlib::randomize_samples (samples, labels);
		std::cout << "MSE and R-squared " << dlib::cross_validate_regression_trainer (trainer, samples, labels, 2) << std::endl;
	}
	*/

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

	std::vector<double> lVars { 0, 0.01, 0.1, 1, 10 };
	std::vector<double> nVars { 0, 1e-5, 2e-5, 5e-5, 1e-4, 2e-4, 5e-4, 1e-3, 2e-3, 5e-3, 7e-3, 1e-2, 2e-2, 3e-2, 4e-2, 5e-2, 6e-2, 7e-2, 8e-2, 9e-2, 0.1 };

	std::mutex mapMutex;
	std::map<double, std::map<double, std::vector<dlib::running_stats<double>>>> results;
	for (auto lVar : lVars)
	{
		std::map<double, std::future<StatsVec_t>> nVar2future;
		for (auto nVar : nVars)
			nVar2future [nVar] = std::async (std::launch::async, getStats, lVar, nVar, pairs);

		for (auto nVar : nVars)
			results [lVar] [nVar] = nVar2future [nVar].get ();
	}

	/*
	for (size_t i = 0; i < PowLength; ++i)
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

	for (size_t i = 0; i < PowLength; ++i)
	{
		std::stringstream fname;
		fname << "coeff" << i << ".dat";

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
