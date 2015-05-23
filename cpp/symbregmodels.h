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

#include "solve.h"

namespace Series
{
	const size_t ParamsCount = 3;

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

namespace Laser
{
	const auto L = 150.0;
	const size_t ParamsCount = 3;

	double alpha0MinusLn (double alpha0, double logr0)
	{
		return alpha0 - logr0 / (2 * L);
	}

	double residual (const std::pair<SampleTypeBase_t<double, 2>, double>& data, const Params_t<ParamsCount>& p)
	{
		const auto r0 = data.first (0);
		const auto logr0 = data.first (1);
		const auto g0 = p (0);
		const auto alpha0 = p (1);
		const auto k = p (2);

		return k * (1 - r0) / (1 + r0) * (g0 / alpha0MinusLn (alpha0, logr0) - 1) - data.second;
	}

	Params_t<ParamsCount> residualDer (const std::pair<SampleTypeBase_t<double, 2>, double>& data, const Params_t<ParamsCount>& p)
	{
		const auto r0 = data.first (0);
		const auto logr0 = data.first (1);
		const auto g0 = p (0);
		const auto alpha0 = p (1);
		const auto k = p (2);

		const auto a0ml = alpha0MinusLn (alpha0, logr0);
		const auto dg0 = k * (1 - r0) / (1 + r0) / a0ml;
		const auto dalpha0 = -g0 * k * (1 - r0) / (1 + r0) / (a0ml * a0ml);
		const auto dk = (1 - r0) / (1 + r0) * (g0 / a0ml - 1);

		Params_t<ParamsCount> res;
		res (0) = dg0;
		res (1) = dalpha0;
		res (2) = dk;
		return res;
	}

	SampleType_t varsDer (const std::pair<SampleTypeBase_t<double, 2>, double>& data, const Params_t<ParamsCount>& p)
	{
		const auto r0 = data.first (0);
		const auto logr0 = data.first (1);
		const auto g0 = p (0);
		const auto alpha0 = p (1);
		const auto k = p (2);

		const auto a0ml = alpha0MinusLn (alpha0, logr0);
		auto result = -2 * (g0 / a0ml - 1) / (1 + r0) / (1 + r0);
		result += g0 * (1 - r0) / (1 + r0) / (2 * L * r0 * a0ml * a0ml);

		SampleType_t res;
		res (0) = result * k;
		return res;
	}

	TrainingSetBase_t<double, 2> preprocess (const TrainingSet_t& srcPts)
	{
		TrainingSetBase_t<double, 2> res;
		for (const auto& srcPt : srcPts)
		{
			double val = srcPt.first (0);
			SampleTypeBase_t<double, 2> pt;
			pt = val, std::log (val);
			res.emplace_back (pt, srcPt.second);
		}
		return res;
	}

	Params_t<ParamsCount> symbRegSolver (double multiplier, const TrainingSet_t& srcPts, double xVar, double yVar)
	{
		xVar *= multiplier;
		yVar *= multiplier;

		return solve<ParamsCount> (preprocess (srcPts),
				residual, residualDer, varsDer,
				[yVar] (const auto& pair) { return pair.second * yVar; },
				[xVar] (const auto& pair) { return pair.first (0) * xVar; },
				{{ 0.002, 0.0002, 100 }});
	}
}

namespace Resonance
{
	const size_t ParamsCount = 3;

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
