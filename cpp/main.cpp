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
#include "solve.h"
#include "util.h"
#include "symbregmodels.h"

using namespace Laser;

void TryLOO (const TrainingSet_t& allPairs)
{
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

double GetMse (const TrainingSet_t& pairs, const Params_t<ParamsCount>& p)
{
	return std::accumulate (pairs.begin (), pairs.end (), 0.0,
			[&p] (double sum, auto pair)
			{
				return sum + std::pow (residual (pair, p), 2);
			});
}

int main (int argc, char **argv)
{
	if (argc < 2)
	{
		std::cout << "Usage: " << argv [0] << " datafile" << std::endl;
		return 1;
	}

	const std::string infile (argv [1]);
	const auto& pairs = LoadData (infile);

	std::cout << "read " << pairs.size () << " samples: " << std::endl;

	const auto& p = solve<ParamsCount> (pairs,
			residual, residualDer, {{ 0.002, 0.0002, 100 }});
	std::cout << "inferred params: " << dlib::trans (p);
	std::cout << "MSE: " << GetMse (pairs, p) << std::endl << std::endl;

	const auto& fixedP = solve<ParamsCount> (pairs,
			residual, residualDer, varsDer,
			[] (const TrainingSetInstance_t& pair) { return pair.second * 0.02; },
			[] (const TrainingSetInstance_t& pair) { return pair.first (0) < 0.6 ? 0.1 : 0.01; },
			{{ 0.002, 0.0002, 100 }});
	std::cout << "fixed inferred params: " << dlib::trans (fixedP);

	std::cout << "MSE: " << GetMse (pairs, fixedP) << std::endl << std::endl;

	//TryLOO (pairs);
	//TrySVM (pairs);
	//TrySVMPoly (pairs);
	//TrySVMSigmoid (pairs);

	std::cout << "calculating mean/dispersion..." << std::endl;

	/*
	std::vector<double> xVars;
	for (double i = 0; i < 1e-3; i += 1e-4)
		xVars.push_back (i);

	std::vector<double> yVars;
	for (double i = 0; i < 5e-4; i += 2e-5)
		yVars.push_back (i);
	*/
	std::vector<double> xVars
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
	std::vector<double> yVars
	{
		1e-3,
		2e-3,
		5e-3,
		1e-2,
		2e-2,
		5e-2,
	};

	auto symbRegSolver = [] (const TrainingSet_t& pts, double xVar, double yVar)
	{
		const auto multiplier = 32;
		if (std::min (xVar, yVar) < 1e-1)
		{
			xVar *= multiplier;
			yVar *= multiplier;
		}

		return solve<ParamsCount> (pts,
				residual, residualDer, varsDer,
				[yVar] (const TrainingSetInstance_t& pair) { return pair.second * yVar; },
				[xVar] (const TrainingSetInstance_t& pair) { return pair.first (0) * xVar; },
				{{ 0.002, 0.0002, 100 }});
	};
	auto svmSolver = [] (const TrainingSet_t& pts)
	{
		dlib::svr_trainer<dlib::radial_basis_kernel<SampleType_t>> trainer;
		trainer.set_kernel ({ 4e-07 });
		trainer.set_c (0.1);
		trainer.set_epsilon_insensitivity (1e-30);

		std::vector<SampleType_t> samples;
		std::vector<double> targets;
		for (const auto& pair : pts)
		{
			samples.push_back (pair.first);
			targets.push_back (pair.second);
		}

		const auto& df = trainer.train (samples, targets);
		return df.alpha;
	};
	auto results = calcStats (symbRegSolver, xVars, yVars, pairs);

	WriteCoeffs (p, results, infile);
}
