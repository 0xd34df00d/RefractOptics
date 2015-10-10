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
