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

#include "solve.h"
#include <algorithm>

std::ostream& PrintCoeffs (std::ostream&, const std::vector<double>&);

namespace
{
	template<typename T, typename Iter>
	T GenSet (size_t size, Iter begin, Iter end)
	{
		if (!size || std::distance (begin, end) < size)
			return 1;

		if (size == 1)
			return std::accumulate (begin, end, T { 0 });

		T result {};

		for (auto i = begin; i != end - size + 1; ++i)
			result += *i * GenSet<T> (size - 1, i + 1, end);

		return result;
	}

	template<typename T>
	std::vector<T> GetLNumeratorCoeffs (const TrainingSetBase_t<T>& points, size_t idx)
	{
		std::vector<T> xs;
		for (size_t i = 0; i < points.size (); ++i)
			if (i != idx)
				xs.push_back (- points [i].first (0));

		std::vector<T> result;
		for (size_t i = 0; i < points.size (); ++i)
			result.push_back (GenSet<T> (i, xs.cbegin (), xs.cend ()));
		return result;
	}
}

template<typename T>
struct DoubleTraits
{
	static T Pow (const T& t1, size_t t2) { return std::pow (t1, t2); }
	static double ToDouble (const T& t) { return static_cast<double> (t); }
};

template<typename T>
class Interpolator
{
	std::vector<T> Result_;
public:
	template<typename U>
	Interpolator (const TrainingSetBase_t<U>& points)
	: Result_ (points.size ())
	{
		for (size_t i = 0; i < points.size (); ++i)
		{
			const T xi { points [i].first (0) };
			const T yi { points [i].second };

			T denom { 1 };
			for (size_t j = 0; j < points.size (); ++j)
				if (j != i)
					denom *= xi - points [j].first (0);

			const auto& coeffs = GetLNumeratorCoeffs (points, i);
			for (size_t i = 0; i < Result_.size (); ++i)
				Result_ [i] += coeffs [i] / denom * yi;
		}
	}

	std::vector<T> GetResult () const
	{
		return Result_;
	}

	dlib::matrix<double, 0, 1> GetResultMat () const
	{
		const auto& doublesVec = GetResult ();

		dlib::matrix<double, 0, 1> result;
		result.set_size (doublesVec.size ());

		for (size_t i = 0; i < doublesVec.size (); ++i)
			result (i) = DoubleTraits<T>::ToDouble (doublesVec [i]);

		return result;
	}
};
