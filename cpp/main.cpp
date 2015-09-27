/**********************************************************************
 * Regression and stability estimation.
 * Copyright (C) 2013  Georg Rudoy
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************/

#include <fstream>
#include <iostream>
#include <cstdlib>
#include <limits>
#include <boost/program_options.hpp>
#include "solve.h"
#include "util.h"
#include "symbregmodels.h"

using namespace Laser;

void tryLOO (const TrainingSet_t<>& srcPairs)
{
	auto allPairs = preprocess (srcPairs);
	for (size_t i = 0; i < allPairs.size (); ++i)
	{
		std::cout << "omitting point " << i << std::endl;

		auto pairs = allPairs;
		pairs.erase (pairs.begin () + i);

		const auto& p = solve<ParamsCount> (pairs, residual, residualDer, {{ 0, 0, 0 }});
		std::cout << "inferred params: " << dlib::trans (p);

		double sum = 0;
		for (const auto& pair : pairs)
		{
			auto r = residual (pair, p);
			sum += r * r;
		}
		std::cout << "MSE: " << sum / pairs.size () << std::endl << std::endl;;
	}
}

double getMse (const TrainingSet_t<>& srcPairs, const Params_t<ParamsCount>& p)
{
	const auto& pairs = preprocess (srcPairs);
	return std::accumulate (pairs.begin (), pairs.end (), 0.0,
			[&p] (double sum, auto pair)
			{
				return sum + std::pow (residual (pair, p), 2);
			});
}

template<long rc>
std::ostream& printVec (std::ostream& ostr, const dlib::matrix<DType_t, rc, 1>& vec)
{
	for (long i = 0; i < rc; ++i)
	{
		if (i)
			ostr << " ";

		ostr << vec (i);
	}

	return ostr;
}

void calculateConvergence (const TrainingSet_t<>& pairs,
		const boost::program_options::variables_map& vm)
{
	const auto& preprocessed = preprocess (pairs);

	const auto& classicP = solve<ParamsCount> (preprocessed,
			residual, residualDer, Initial);

	const auto start = vm.count ("conv-start") ? vm ["conv-start"].as<double> () : 1;
	const auto end = vm.count ("conv-end") ? vm ["conv-end"].as<double> () : 10;
	const auto step = vm.count ("conv-step") ? vm ["conv-step"].as<double> () : 0.01;

	std::ofstream ostr { vm.count ("output-file") ? vm ["output-file"].as<std::string> () : "convergence.txt" };
	for (double i = start; i < end; i += step)
	{
		const auto& fixedP = solve<ParamsCount> (preprocessed,
				residual, residualDer, varsDer,
				[i] (const auto& pair) { return 0.02 * i * pair.second; },
				[] (const auto& pair) { return pair.first (0) < 0.6 ? 0.1 : 0.01; },
				Initial);

		ostr << i << " ";
		printVec (ostr, classicP);
		ostr << " ";
		printVec (ostr, fixedP);
		ostr << "\n";
	}
}

Params_t<ParamsCount> symbRegSolver (DType_t multiplier, const TrainingSet_t<>& srcPts, DType_t xVar, DType_t yVar)
{
	xVar *= multiplier;
	yVar *= multiplier;

	return solve<ParamsCount> (preprocess (srcPts),
			residual, residualDer, varsDer,
			[yVar] (const auto& pair) { return pair.second * yVar; },
			[xVar] (const auto& pair) { return pair.first (0) * xVar; },
			Initial);
}

double svmSolver (const TrainingSet_t<>& pts)
{
	dlib::svr_trainer<dlib::radial_basis_kernel<SampleType_t<>>> trainer;
	trainer.set_kernel ({ 4e-07 });
	trainer.set_c (0.1);
	trainer.set_epsilon_insensitivity (1e-30);

	std::vector<SampleType_t<>> samples;
	std::vector<DType_t> targets;
	for (const auto& pair : pts)
	{
		samples.push_back (pair.first);
		targets.push_back (pair.second);
	}

	const auto& df = trainer.train (samples, targets);
	return df.alpha;
}

boost::program_options::variables_map parseOptions (int argc, char **argv)
{
	namespace po = boost::program_options;

	po::options_description desc { "Allowed options" };
	desc.add_options ()
		("help", "show help")
		("input-file", po::value<std::string> (), "input data file")
		("output-file", po::value<std::string> (), "output data file")
		("convergence", po::value<bool> (), "calculate convergence")
		("conv-start", po::value<double> (), "convergence start")
		("conv-end", po::value<double> (), "convergence end")
		("conv-step", po::value<double> (), "convergence step")
		("multiplier", po::value<int> (), "set multiplier");

	po::positional_options_description p;
	p.add ("input-file", -1);

	po::variables_map vm;
	po::store (po::command_line_parser (argc, argv).options (desc).positional (p).run (), vm);
	po::notify (vm);

	if (vm.count ("help"))
	{
		std::cout << "Usage:\n" << desc << std::endl;
		return {};
	}

	if (!vm.count ("input-file"))
	{
		std::cout << "Usage:\n" << desc << std::endl;
		throw std::runtime_error { "no input file is set!" };
	}

	return vm;
}

int main (int argc, char **argv)
{
	const auto& vm = parseOptions (argc, argv);
	if (vm.empty ())
		return 1;

	const auto& infile = vm ["input-file"].as<std::string> ();
	const auto& pairs = LoadData (infile);

	std::cout << "read " << pairs.size () << " samples: " << std::endl;

	const auto& p = solve<ParamsCount> (preprocess (pairs),
			residual, residualDer, Initial);
	std::cout << "inferred params: " << dlib::trans (p);
	std::cout << "MSE: " << getMse (pairs, p) << std::endl << std::endl;

	const auto& fixedP = solve<ParamsCount> (preprocess (pairs),
			residual, residualDer, varsDer,
			[] (const auto& pair) { return pair.second * 0.02; },
			[] (const auto& pair) { return pair.first (0) < 0.6 ? 0.1 : 0.01; },
			Initial);
	std::cout << "fixed inferred params: " << dlib::trans (fixedP);

	std::cout << "MSE: " << getMse (pairs, fixedP) << std::endl << std::endl;

	/*
	std::vector<double> xVars;
	for (double i = 0; i < 1e-3; i += 1e-4)
		xVars.push_back (i);

	std::vector<double> yVars;
	for (double i = 0; i < 5e-4; i += 2e-5)
		yVars.push_back (i);
	*/
	std::vector<DType_t> xVars
	{
		1e-3,
		2e-3,
		5e-3,
		1e-2,
		2e-2,
		/*
		5e-2,
		1e-1,
		*/
	};
	std::vector<DType_t> yVars
	{
		1e-3,
		2e-3,
		5e-3,
		1e-2,
		2e-2,
		5e-2,
	};

	if (vm.count ("convergence") && vm ["convergence"].as<bool> ())
	{
		std::cout << "calculating convergence..." << std::endl;
		calculateConvergence (pairs, vm);
		return 0;
	}

	std::cout << "calculating mean/dispersion..." << std::endl;

	const auto multiplier = vm.count ("multiplier") ? vm ["multiplier"].as<int> () : 1;
	using namespace std::placeholders;
	auto results = calcStats (std::bind (symbRegSolver, multiplier, _1, _2, _3), xVars, yVars, pairs);

	WriteCoeffs (p, results, infile);
}
