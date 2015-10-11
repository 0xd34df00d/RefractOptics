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

#include <random>
#include "defs.h"
#include "threadpool.h"

DType_t clamp (DType_t from, DType_t to, DType_t x, DType_t xDev)
{
	if (x + xDev <= to && x + xDev >= from)
		return x + xDev;

	if (x + xDev > to)
	{
		if (x - xDev >= from)
			return x - xDev;

		return 2 * x >= to - from ? to : from;
	}
	else
	{
		if (x - xDev >= to)
			return x - xDev;

		return 2 * x >= to - from ? to : from;
	}
}

template<
		typename Model,
		typename YSigmaGetterT,
		typename XSigmasGetterT
	>
TrainingSet_t<> genSample (size_t size, DType_t from, DType_t to,
		const YSigmaGetterT& ySigma,
		const XSigmasGetterT& xSigma,
		const Params_t<Model::ParamsCount>& params)
{
	std::mt19937_64 generator { std::random_device {} () };

	std::uniform_real_distribution<DType_t> rawXDistr { from, to };

	TrainingSet_t<> result;

	for (size_t i = 0; i < size; ++i)
	{
		const auto rawX = rawXDistr (generator);

		const double xArr [] = { rawX };
		const SampleType_t<> pseudoSample { xArr };

		auto preprocessed = Model::preprocess ({ { pseudoSample, 0 } });
		const auto rawY = Model::residual (preprocessed [0], params);

		const TrainingSetInstance_t<> pair { pseudoSample, rawY };

		const auto yDev = std::normal_distribution<DType_t> { 0, ySigma (pair) } (generator);
		const auto xDev = std::normal_distribution<DType_t> { 0, xSigma (pair) } (generator);

		SampleType_t<> sample;
		sample (0) = rawX + xDev;

		result.push_back ({ sample, rawY + yDev });
	}

	return result;
}

template<size_t ParamsCount>
struct SingleCompareResult
{
	Params_t<ParamsCount> m_classicalParams;
	Params_t<ParamsCount> m_modifiedParams;

	SingleCompareResult ()
	{
		for (size_t i = 0; i < ParamsCount; ++i)
		{
			m_classicalParams (i) = 0;
			m_modifiedParams (i) = 0;
		}
	}

	SingleCompareResult (const Params_t<ParamsCount>& c, const Params_t<ParamsCount>& m)
	: m_classicalParams { c }
	, m_modifiedParams { m }
	{
	}

	SingleCompareResult& operator+= (const SingleCompareResult& other)
	{
		m_classicalParams += other.m_classicalParams;
		m_modifiedParams += other.m_modifiedParams;
		return *this;
	}

	SingleCompareResult operator- () const
	{
		return { -m_classicalParams, -m_modifiedParams };
	}

	SingleCompareResult& operator-= (const SingleCompareResult& other)
	{
		return (*this) += (-other);
	}

	friend SingleCompareResult operator+ (SingleCompareResult left, const SingleCompareResult& right)
	{
		left += right;
		return left;
	}

	friend SingleCompareResult operator- (SingleCompareResult left, const SingleCompareResult& right)
	{
		left -= right;
		return left;
	}
};

template<
		typename Model,
		typename YSigmaGetterT,
		typename XSigmasGetterT
	>
SingleCompareResult<Model::ParamsCount> compareFunctionals (size_t size, DType_t from, DType_t to,
		const YSigmaGetterT& ySigma,
		const XSigmasGetterT& xSigma,
		const Params_t<Model::ParamsCount>& params,
		double multiplier)
{
	const auto& trainingSet = genSample<Model> (size, from, to, ySigma, xSigma, params);

	const auto& preprocessed = Model::preprocess (trainingSet);

	const auto& classicP = solve<Model::ParamsCount> (preprocessed,
			Model::residual, Model::residualDer, Model::initial ());
	const auto& fixedP = solve<Model::ParamsCount> (preprocessed,
			Model::residual, Model::residualDer, Model::varsDer,
			ySigma,
			xSigma,
			Model::initial (),
			multiplier);

	return { classicP, fixedP };
}

template<
		typename Model,
		typename YSigmaGetterT,
		typename XSigmasGetterT
	>
auto compareFunctionals (size_t sizeFrom, size_t sizeTo,
		int repetitions,
		DType_t pointFrom, DType_t pointTo,
		const YSigmaGetterT& ySigma,
		const XSigmasGetterT& xSigma,
		const Params_t<Model::ParamsCount>& params,
		double multiplier)
{
	using SingleResult_t = SingleCompareResult<Model::ParamsCount>;
	std::vector<SingleResult_t> result;
	result.resize (sizeTo - sizeFrom + 1);

	const SingleResult_t reference { params, params };

	{
		ThreadPool pool;
		std::mutex outMutex;
		for (auto size = sizeFrom; size < sizeTo; ++size)
			pool << [&, size]
				{
					{
						std::lock_guard<std::mutex> lock { outMutex };
						std::cout << "\tdoing " << size << std::endl;
					}
					SingleResult_t subres;
					for (size_t i = 0; i < repetitions; ++i)
						subres += compareFunctionals<Model> (size, pointFrom, pointTo, ySigma, xSigma, params, multiplier) - reference;

					subres.m_classicalParams /= repetitions;
					subres.m_modifiedParams /= repetitions;

					result [size - sizeFrom] = subres;
				};
	}

	return result;
}
