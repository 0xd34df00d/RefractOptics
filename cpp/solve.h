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

template<typename T> using SampleTypeBase_t = dlib::matrix<T, 1, 1>;
template<typename T> using TrainingSetBase_t = std::vector<std::pair<SampleTypeBase_t<T>, T>>;

typedef SampleTypeBase_t<double> SampleType_t;
typedef TrainingSetBase_t<double> TrainingSet_t;

template<size_t ParamsCount> using Params_t = dlib::matrix<double, ParamsCount, 1>;

template<size_t ParamsCount, typename R, typename D>
Params_t<ParamsCount> solve (const TrainingSet_t& pairs, R res, D der)
{
	Params_t<ParamsCount> p;
	for (size_t i = 0; i < ParamsCount; ++i)
		p (i) = 0;
	dlib::solve_least_squares_lm (dlib::objective_delta_stop_strategy(1e-11),
			res, der, pairs, p, 0.01);
	return p;
}

typedef std::vector<std::vector<double>> StatsVec_t;
typedef std::vector<std::pair<SampleType_t, double>> PairsList_t;
typedef std::vector<dlib::running_stats<double>> RunningStatsList_t;
typedef std::map<double, std::map<double, RunningStatsList_t>> Stats_t;

template<typename Solver>
class StatsKeeper
{
	Solver Solver_;

	double LVar_;
	double NVar_;

	PairsList_t Pairs_;

	StatsVec_t Stats_;
	RunningStatsList_t Running_;

	const bool Relative_ = true;
public:
	StatsKeeper (Solver s, double lVar, double nVar, const PairsList_t& pairs, bool relative = true)
	: Solver_ (s)
	, LVar_ (lVar)
	, NVar_ (nVar)
	, Pairs_ (pairs)
	, Relative_ (relative)
	{
	}

	void TryMore (size_t tries)
	{
		std::random_device seeder;
		std::mt19937_64 generator { seeder () };

		std::normal_distribution<double> lambdaDistr { 0, LVar_ };
		std::normal_distribution<double> nDistr { 0, NVar_ };

		for (size_t i = 0; i < tries; ++i)
		{
			auto localPairs = Pairs_;
			for (auto& pair : localPairs)
			{
				if (LVar_)
				{
					const auto bound = LVar_ * pair.first (0);
					pair.first (0) += Relative_ ?
							std::normal_distribution<double> { 0, bound } (generator) :
							lambdaDistr (generator);
				}
				if (NVar_)
				{
					const auto bound = NVar_ * pair.second;
					pair.second += Relative_ ?
							std::normal_distribution<double> { 0, bound } (generator) :
							nDistr (generator);
				}
			}

			const auto& p = Solver_ (localPairs);

			if (Stats_.size () < p.nr ())
			{
				Stats_.resize (p.nr ());
				Running_.resize (p.nr ());
			}

			for (size_t j = 0; j < p.nr (); ++j)
			{
				const auto val = p (j);
				Stats_ [j].push_back (val);
				Running_ [j].add (val);
			}
		}
	}

	const StatsVec_t& GetPoints () const
	{
		return Stats_;
	}

	const RunningStatsList_t& GetRunning () const
	{
		return Running_;
	}
};

template<typename Solver>
StatsVec_t getStats (double lVar, double nVar, const PairsList_t& pairs, Solver s)
{
	StatsKeeper<Solver> keeper (s, lVar, nVar, pairs);
	keeper.TryMore (5000);
	return keeper.GetPoints ();
}

template<typename Solver>
Stats_t calcStats (Solver s, const std::vector<double>& lVars, const std::vector<double>& nVars,
			const PairsList_t& pairs, size_t threadCount = 0)
{
	std::map<double, std::map<double, std::vector<dlib::running_stats<double>>>> results;

	const double count = lVars.size () * nVars.size ();
	size_t finished = 0;

	if (!threadCount)
		threadCount = std::thread::hardware_concurrency () - 1;

	for (auto lVar : lVars)
	{
		for (auto i = nVars.begin (); i != nVars.end (); )
		{
			std::map<double, std::future<StatsVec_t>> nVar2future;
			for (size_t t = 0; i != nVars.end () && t < threadCount; ++i, ++t)
				nVar2future [*i] = std::async (std::launch::async,
						&getStats<Solver>,
						lVar, *i, pairs, s);

			for (auto& pair : nVar2future)
			{
				const auto& coeffs = pair.second.get ();

				std::vector<dlib::running_stats<double>> stats;
				stats.resize (coeffs.size ());
				for (size_t i = 0; i < coeffs.size (); ++i)
					for (auto coeff : coeffs [i])
						stats [i].add (coeff);
				results [lVar] [pair.first] = stats;
				std::cout << (100 * ++finished / count) << "% done for (" << lVar << "; " << pair.first << ")" << std::endl;
			}
		}
	}
	return results;
}
