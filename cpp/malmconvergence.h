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
#include <boost/asio/io_service.hpp>
#include "defs.h"

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

		result.push_back ({ sample, yDev });
	}

	return result;
}

template<size_t ParamsCount>
struct SingleCompareResult
{
	Params_t<ParamsCount> m_classicalParams;
	Params_t<ParamsCount> m_modifiedParams;

	SingleCompareResult& operator+= (const SingleCompareResult& other)
	{
		m_classicalParams += other.m_classicalParams;
		m_modifiedParams += other.m_modifiedParams;
		return *this;
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

class ThreadPool
{
	boost::asio::io_service IO_;
	boost::asio::io_service::work Work_ { IO_ };

	const size_t Count_;
public:
	ThreadPool (size_t count = 0)
	: Count_ { count ? count : std::thread::hardware_concurrency () }
	{
	}

	~ThreadPool ()
	{
		std::vector<std::thread> threads;
		for (size_t i = 0; i < Count_; ++i)
			threads.emplace_back ([this] { IO_.run (); });

		for (auto& thread : threads)
			thread.join ();
	}

	template<typename F>
	ThreadPool& operator<< (const F& f)
	{
		IO_.post (f);
		return *this;
	}
};

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
	std::vector<SingleCompareResult<Model::ParamsCount>> result;
	result.resize (sizeTo - sizeFrom + 1);

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
					SingleCompareResult<Model::ParamsCount> subres;
					for (size_t i = 0; i < repetitions; ++i)
						subres += compareFunctionals<Model> (size, pointFrom, pointTo, ySigma, xSigma, params, multiplier);

					subres.m_classicalParams -= repetitions * params;
					subres.m_modifiedParams -= repetitions * params;

					subres.m_classicalParams /= repetitions;
					subres.m_modifiedParams /= repetitions;

					result [size - sizeFrom] = subres;
				};
	}

	return result;
}
