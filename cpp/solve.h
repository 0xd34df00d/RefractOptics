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

#pragma once

#include <vector>
#include <thread>
#include <future>
#include <dlib/svm.h>

typedef dlib::matrix<double, 1, 1> SampleType_t;

template<size_t ParamsCount> using Params_t = dlib::matrix<double, ParamsCount, 1>;

template<size_t ParamsCount, typename R, typename D>
Params_t<ParamsCount> solve (const std::vector<std::pair<SampleType_t, double>>& pairs, R res, D der)
{
	Params_t<ParamsCount> p;
	for (size_t i = 0; i < ParamsCount; ++i)
		p (i) = 0;
	dlib::solve_least_squares_lm (dlib::objective_delta_stop_strategy(1e-7),
			res, der, pairs, p, 10);
	return p;
}

typedef std::vector<std::vector<double>> StatsVec_t;

template<size_t ParamsCount, typename R, typename D>
StatsVec_t getStats (double lVar, double nVar,
		const std::vector<std::pair<SampleType_t, double>>& pairs, R res, D der)
{
	std::random_device generator;
	std::normal_distribution<double> lambdaDistr (0, lVar);
	std::normal_distribution<double> nDistr (0, nVar);

	StatsVec_t stats;
	stats.resize (ParamsCount);
	for (size_t i = 0; i < 5000; ++i)
	{
		std::vector<std::pair<SampleType_t, double>> localPairs = pairs;
		for (auto& pair : localPairs)
		{
			if (lVar)
				pair.first (0) += lambdaDistr (generator);
			if (nVar)
				pair.second += nDistr (generator);
		}

		const auto& p = solve<ParamsCount> (localPairs, res, der);
		for (size_t j = 0; j < ParamsCount; ++j)
			stats [j].push_back (p (j));
	}
	return stats;
}

template<size_t ParamsCount, typename R, typename D>
std::map<double, std::map<double, std::vector<dlib::running_stats<double>>>>
	calcStats (const std::vector<double>& lVars, const std::vector<double>& nVars,
			std::vector<std::pair<SampleType_t, double>> pairs, R res, D der)
{
	std::map<double, std::map<double, std::vector<dlib::running_stats<double>>>> results;
	for (auto lVar : lVars)
	{
		for (auto i = nVars.begin (); i != nVars.end (); )
		{
			std::map<double, std::future<StatsVec_t>> nVar2future;
			for (size_t t = 0; i != nVars.end () && t < std::thread::hardware_concurrency (); ++i, ++t)
				nVar2future [*i] = std::async (std::launch::async,
						getStats<ParamsCount, R, D>,
						lVar, *i, pairs, res, der);

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
	return results;
}