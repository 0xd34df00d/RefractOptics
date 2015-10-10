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
#include <dlib/optimization.h>
#include <dlib/statistics.h>
#include "defs.h"

template<size_t ParamsCount, typename R, typename D, typename TS>
Params_t<ParamsCount> solve (const TS& pairs, R res, D paramsDer, const std::array<DType_t, ParamsCount>& initial)
{
	Params_t<ParamsCount> p;
	for (auto i = 0u; i < ParamsCount; ++i)
		p (i) = initial [i];
	dlib::solve_least_squares_lm (dlib::gradient_norm_stop_strategy { 1e-18, 500 },
			res, paramsDer, pairs, p, 1e4);
	return p;
}

template<size_t ParamsCount,
		typename TS,
		typename ResidualT,
		typename ParamsDerivativeT,
		typename VariablesDerivativeT,
		typename YSigmaGetterT,
		typename XSigmasGetterT>
Params_t<ParamsCount> solve (const TS& pairs,
		ResidualT res, ParamsDerivativeT paramsDer, VariablesDerivativeT varsDer,
		YSigmaGetterT ySigma, XSigmasGetterT xSigmas,
		const std::array<DType_t, ParamsCount>& initial,
		double multiplier)
{
	Params_t<ParamsCount> p;
	for (auto i = 0u; i < ParamsCount; ++i)
		p (i) = initial [i];

	const auto diff = 1e-18;

	for (int i = 0; i < 500; ++i)
	{
		const auto prevP = p;

#if 1
		auto wrappedRes = [&] (const auto& data, const auto& p) -> double
		{
			const auto srcVal = res (data, p);

			const auto& derivatives = varsDer (data, p);
			const DType_t denom = std::pow (ySigma (data), 2) + std::pow (xSigmas (data) * derivatives, 2);

			return srcVal / std::sqrt (multiplier * multiplier * denom);
		};
#else
		const auto& wrappedRes = res;
#endif

		dlib::solve_least_squares_lm (dlib::gradient_norm_stop_strategy { diff, 2 },
				wrappedRes, paramsDer, pairs, p, 1e6);

		if (dlib::length (prevP - p) < diff)
			break;
	}
	return p;
}

typedef std::vector<std::vector<DType_t>> StatsVec_t;
typedef std::vector<std::pair<SampleType_t<>, DType_t>> PairsList_t;
typedef std::vector<dlib::running_stats<DType_t>> RunningStatsList_t;
typedef std::map<DType_t, std::map<DType_t, RunningStatsList_t>> Stats_t;

namespace detail
{
	template<typename Solver>
	auto MakeSolverWrapper (Solver s, std::result_of_t<Solver (PairsList_t, DType_t, DType_t)>* = nullptr)
	{
		return s;
	}

	template<typename Solver>
	auto MakeSolverWrapper (Solver s, std::result_of_t<Solver (PairsList_t)>* = nullptr)
	{
		return [s] (const PairsList_t& pairs, double, double) { return s (pairs); };
	}
}

template<typename Solver>
class StatsKeeper
{
	decltype (detail::MakeSolverWrapper (std::declval<Solver> ())) Solver_;

	DType_t LVar_;
	DType_t NVar_;

	PairsList_t Pairs_;

	StatsVec_t Stats_;
	RunningStatsList_t Running_;

	const bool Relative_ = true;
public:
	StatsKeeper (Solver s, DType_t lVar, DType_t nVar, const PairsList_t& pairs, bool relative = true)
	: Solver_ (detail::MakeSolverWrapper (s))
	, LVar_ (lVar)
	, NVar_ (nVar)
	, Pairs_ (pairs)
	, Relative_ (relative)
	{
	}

	void TryMore (size_t tries)
	{
		std::mt19937_64 generator { std::random_device {} () };

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

			const auto& p = Solver_ (localPairs, LVar_, NVar_);

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
StatsVec_t getStats (DType_t lVar, DType_t nVar, const PairsList_t& pairs, Solver s)
{
	StatsKeeper<Solver> keeper (s, lVar, nVar, pairs);
	keeper.TryMore (20000);
	return keeper.GetPoints ();
}

template<typename Solver>
Stats_t calcStats (Solver s, const std::vector<DType_t>& lVars, const std::vector<DType_t>& nVars,
			const PairsList_t& pairs, size_t threadCount = 0)
{
	std::map<DType_t, std::map<DType_t, std::vector<dlib::running_stats<DType_t>>>> results;

	const double count = lVars.size () * nVars.size ();
	size_t finished = 0;

	if (!threadCount)
		threadCount = std::thread::hardware_concurrency () - 1;

	std::vector<std::pair<DType_t, DType_t>> combs;
	for (auto lVar : lVars)
		for (auto nVar : nVars)
			combs.emplace_back (lVar, nVar);

	for (auto i = combs.begin (); i != combs.end (); )
	{
		std::map<std::pair<DType_t, DType_t>, std::future<StatsVec_t>> vars2future;
		for (size_t t = 0; i != combs.end () && t < threadCount; ++i, ++t)
			vars2future [*i] = std::async (std::launch::async,
					&getStats<Solver>,
					i->first, i->second, pairs, s);

		for (auto& pair : vars2future)
		{
			const auto& coeffs = pair.second.get ();

			const auto lVar = pair.first.first;
			const auto nVar = pair.first.second;

			std::vector<dlib::running_stats<DType_t>> stats;
			stats.resize (coeffs.size ());
			for (size_t i = 0; i < coeffs.size (); ++i)
				for (auto coeff : coeffs [i])
					stats [i].add (coeff);
			results [lVar] [nVar] = stats;
			std::cout << (100 * ++finished / count) << "% done for (" << lVar << "; " << nVar << ")" << std::endl;
		}
	}

	/*
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
	*/
	return results;
}
