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
#include <dlib/svm.h>
#include "solve.h"
#include "util.h"

const size_t ParamsCount = 3;

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

	if (value != 1 || !count)
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

namespace Series
{
	double residual (const std::pair<SampleType_t, double>& data, const Params_t<ParamsCount>& p)
	{
		double result = 0;
		const auto x = data.first (0);
		for (size_t i = 0; i < ParamsCount; ++i)
			result += p (i) / std::pow (x, 2 * i);
		return result - data.second;
	}

	Params_t<ParamsCount> residualDer (const std::pair<SampleType_t, double>& data, const Params_t<ParamsCount>& p)
	{
		Params_t<ParamsCount> res;
		const auto x = data.first (0);
		for (size_t i = 0; i < ParamsCount; ++i)
			res (i) = 1 / std::pow (x, 2 * i);
		return res;
	}

	namespace
	{
		double subDerivative (double x, int64_t i, double p_i)
		{
			return -2 * i * p_i * std::pow (x, -2 * i - 1);
		}
	}

	SampleType_t varsDer (const std::pair<SampleType_t, double>& data, const Params_t<ParamsCount>& p)
	{
		const auto x = data.first (0);

		double result = 0;
		for (size_t i = 1; i < ParamsCount; ++i)
			result += subDerivative (x, static_cast<int64_t> (i), p (i));

		SampleType_t res;
		res (0) = result;
		return res;
	}
}

namespace Resonance
{
	double residual (const std::pair<SampleType_t, double>& data, const Params_t<ParamsCount>& p)
	{
		const auto x = data.first (0);
		const auto x2 = x * x;

		const double cminus = p (2) - 1.0 / x2;
		const double underRoot = p (0) + p (1) / cminus;
		const double root = underRoot >= 0 ? std::sqrt(underRoot) : 10;

		return root - data.second;
	}

	Params_t<ParamsCount> residualDer (const std::pair<SampleType_t, double>& data, const Params_t<ParamsCount>& p)
	{
		const auto x = data.first (0);
		const auto x2 = x * x;

		const double cminus = p (2) - 1.0 / x2;
		const double underRoot = p (0) + p (1) / cminus;
		const double root = underRoot > 0 ? std::sqrt(underRoot) : 10;

		Params_t<ParamsCount> res;
		res (0) = 1. / (2. * root);
		res (1) = 1. / (2. * root * cminus);
		res (2) = -(p (1) / (2 * root * cminus * cminus));
		return res;
	}
}

using namespace Series;

void TryLOO (const TrainingSet_t& allPairs)
{
	for (size_t i = 0; i < allPairs.size (); ++i)
	{
		std::cout << "omitting point " << i << std::endl;

		auto pairs = allPairs;
		pairs.erase (pairs.begin () + i);

		const auto& p = solve<ParamsCount> (pairs, residual, residualDer);
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

template<typename Kernel>
struct SVMResult
{
	double MSE_;
	dlib::decision_function<Kernel> DF_;
};

template<template<typename T> class Kernel, typename S, typename T, typename... KernelParams>
SVMResult<Kernel<typename S::value_type>> TrySVMSingle (S samples, T targets, double c, KernelParams... params)
{
	dlib::svr_trainer<Kernel<typename S::value_type>> trainer;
	trainer.set_kernel ({ params... });
	trainer.set_c (c);
	trainer.set_epsilon_insensitivity (1e-10);

	const auto& df = trainer.train (samples, targets);

	dlib::randomize_samples (samples, targets);
	return
	{
		dlib::cross_validate_regression_trainer (trainer, samples, targets, 3) (0),
		df
	};
}

template<typename T>
void TrySVM (const TrainingSetBase_t<T>& allPairs)
{
	typedef SampleTypeBase_t<double> sample_t;

	std::vector<sample_t> samples;
	std::vector<T> targets;
	for (const auto& pair : allPairs)
	{
		samples.push_back (pair.first);
		targets.push_back (pair.second);
	}

	struct MinInfo
	{
		double c;
		double gamma;
		double mse;
	} min { 0, 0, 1e6 };
	// c = 10 seems optimal for now
	for (auto c : { 1e-4, 1e-3, 1e-2, 0.1, 1.0, 10.0, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9 })
	{
		for (auto gamma = 1e-6; gamma < 1e-4; gamma += 1e-6)
		{
			const auto result = TrySVMSingle<dlib::radial_basis_kernel> (samples, targets, c, gamma);
			if (result.MSE_ < min.mse)
				min = { c, gamma, result.MSE_ };
		}
	}
	std::cout << "min: " << min.mse << " with c = " << min.c << "; gamma = " << min.gamma << std::endl;
}

template<typename T>
void TrySVMPoly (const TrainingSetBase_t<T>& allPairs)
{
	typedef SampleTypeBase_t<double> sample_t;

	std::vector<sample_t> samples;
	std::vector<T> targets;
	for (const auto& pair : allPairs)
	{
		samples.push_back (pair.first);
		targets.push_back (pair.second);
	}

	struct MinInfo
	{
		double c;
		double gamma;
		double coeff;
		double degree;
		double mse;
	} min { 0, 0, 0, 0, 1e6 };

	for (auto c : { 1e-4, 1e-3, 1e-2, 0.1, 1.0, 10.0, 1e2, 1e3 })
	{
		for (auto gamma : { 0.01, 0.1, 1. })
		{
			for (auto coeff : { 0., 0.01, 0.1, 1., 10. })
			{
				for (auto degree : { 0.001, 0.005, 0.01, 0.05, 0.1, 0.5, 1. })
				{
					const auto result = TrySVMSingle<dlib::polynomial_kernel> (samples, targets, c, gamma, coeff, degree);
					/*
					std::cout << "alpha count: " << result.DF_.alpha.size () << std::endl;
					for (int i = 0; i < result.DF_.alpha.nr (); ++i)
						std::cout << result.DF_.alpha (i) << " ";
					std::cout << std::endl;
					*/

					if (result.MSE_ < min.mse)
						min = { c, gamma, coeff, degree, result.MSE_ };
				}
			}
			std::cout << "done gamma " << gamma << std::endl;
		}
	}

	std::cout << "min: " << min.mse
			<< " with c = " << min.c
			<< "; gamma = " << min.gamma
			<< "; coeff = " << min.coeff
			<< "; degree = " << min.degree
			<< std::endl;
}

template<typename T>
void TrySVMSigmoid (const TrainingSetBase_t<T>& allPairs)
{
	typedef SampleTypeBase_t<double> sample_t;

	std::vector<sample_t> samples;
	std::vector<T> targets;
	for (const auto& pair : allPairs)
	{
		samples.push_back (pair.first);
		targets.push_back (pair.second);
	}

	struct MinInfo
	{
		double c;
		double gamma;
		double coeff;
		double mse;
	} min { 0, 0, 0, 1e6 };

	for (auto c : { 1e-4, 1e-3, 1e-2, 0.1, 1.0, 10.0, 1e2, 1e3 })
	{
		for (auto gamma = 1e-07; gamma < 1e-6; gamma += 5e-08)
		{
			for (auto coeff = -1.0; coeff < -0.5; coeff += 0.01)
			{
				const auto result = TrySVMSingle<dlib::sigmoid_kernel> (samples, targets, c, gamma, coeff);
				/*
				std::cout << "alpha count: " << result.DF_.alpha.size () << std::endl;
				for (int i = 0; i < result.DF_.alpha.nr (); ++i)
					std::cout << result.DF_.alpha (i) << " ";
				std::cout << std::endl;
				*/

				if (result.MSE_ < min.mse)
					min = { c, gamma, coeff, result.MSE_ };
			}
		}
	}

	std::cout << "min: " << min.mse
			<< " with c = " << min.c
			<< "; gamma = " << min.gamma
			<< "; coeff = " << min.coeff
			<< std::endl;
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

	const auto& p = solve<ParamsCount> (pairs, residual, residualDer);
	std::cout << "inferred params: " << dlib::trans (p);

	double sum = 0;
	for (const auto& pair : pairs)
	{
		auto r = residual (pair, p);
		sum += r * r;
	}
	std::cout << "MSE: " << sum / pairs.size () << std::endl << std::endl;

	//TryLOO (pairs);
	//TrySVM (pairs);
	//TrySVMPoly (pairs);
	//TrySVMSigmoid (pairs);

	std::cout << "calculating mean/dispersion..." << std::endl;

	std::vector<double> lVars;
	for (double i = 0; i < 1e-3; i += 1e-4)
		lVars.push_back (i);

	std::vector<double> nVars;
	for (double i = 0; i < 5e-4; i += 2e-5)
		nVars.push_back (i);

	auto symbRegSolver = [] (const TrainingSet_t& pts)
	{
		return solve<ParamsCount> (pts, residual, residualDer);
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
	const auto& myp = svmSolver (pairs);
	auto results = calcStats (svmSolver, lVars, nVars, pairs);

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

	WriteCoeffs (myp, results, infile);
}
