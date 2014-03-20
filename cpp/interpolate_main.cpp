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

#include "interpolator.h"
#include <boost/lexical_cast.hpp>
#include <boost/multiprecision/gmp.hpp>
#include <boost/math/special_functions/powm1.hpp>
#include <vector>
#include <gmpxx.h>
#include <gmp.h>
#include "util.h"

template<>
struct DoubleTraits<mpf_class>
{
	static double ToDouble (const mpf_class& c) { return c.get_d (); }

	static mpf_class Pow (const mpf_class& c1, size_t c2)
	{
		mpf_class res;
		mpf_pow_ui (res.get_mpf_t (), c1.get_mpf_t (), c2);
		return res;
	}
};

template<typename T>
struct DoubleTraits<boost::multiprecision::number<T>>
{
	static double ToDouble (const boost::multiprecision::number<T>& num) { return num.template convert_to<double> (); }

	static boost::multiprecision::number<T> Pow (const boost::multiprecision::number<T>& num, size_t c)
	{
		return boost::math::powm1 (num, c) + 1;
	}
};

namespace
{
	template<typename T>
	std::vector<double> VecToDouble (const std::vector<T>& vec)
	{
		std::vector<double> result;
		for (const auto& item : vec)
			result.push_back (DoubleTraits<T>::ToDouble (item));
		return result;
	}
}

template<typename T>
bool Test (const std::vector<T>& coeffs)
{
	std::cout << "*** testing\t\t";
	PrintCoeffs (std::cout, VecToDouble (coeffs)) << "...\t\t\t";

	TrainingSetBase_t<T> set;
	auto add = [&set] (T x, T y)
	{
		SampleTypeBase_t<T> t;
		t (0) = x;
		set.push_back ({ t, y });
	};

	for (size_t i = 0; i < coeffs.size (); ++i)
		add (i, Value (coeffs, static_cast<T> (i)));

	Interpolator<T> ip { set };
	const auto& res = ip.GetResult ();
	for (size_t i = 0; i < coeffs.size (); ++i)
	{
		if (std::abs (DoubleTraits<T>::ToDouble (coeffs [i] - res [i])) / DoubleTraits<T>::ToDouble (coeffs [i] ? coeffs [i] : 1) <= 1e-2)
			continue;

		std::cout << "[ Fail ]" << std::endl;
		std::cout << "\tExpected: ";
		PrintCoeffs (std::cout, VecToDouble (coeffs)) << std::endl;
		std::cout << "\tGot:      ";
		PrintCoeffs (std::cout, VecToDouble (res)) << std::endl;

		return false;
	}

	std::cout << "[ OK ]" << std::endl;

	return true;
}

template<typename T>
void PerfTest (const std::vector<T>& coeffs)
{
	TrainingSetBase_t<T> set;
	auto add = [&set] (T x, T y)
	{
		SampleTypeBase_t<T> t;
		t (0) = x;
		set.push_back ({ t, y });
	};

	for (size_t i = 0; i < coeffs.size (); ++i)
		add (i, Value (coeffs, static_cast<T> (i)));

	const auto& begin = std::chrono::high_resolution_clock::now ();

	const size_t count = 1000;
	for (size_t i = 0; i < 1000; ++i)
	{
		Interpolator<T> ip { set };
		const auto& res = ip.GetResult ();
	}

	const auto& end = std::chrono::high_resolution_clock::now ();

	std::cout << count << " iterations took "
			<< std::chrono::duration_cast<std::chrono::milliseconds> (end - begin).count ()
			<< " ms" << std::endl;
}

typedef long double Type;

int main (int argc, char **argv)
{
	Test<Type> ({ 1 });
	Test<Type> ({ 1, 0 });
	Test<Type> ({ 1, 0, 0 });
	Test<Type> ({ 3, 2, 4 });
	Test<Type> ({ 1, 0, 0, 0 });
	Test<Type> ({ 1, 0, 0, 10 });
	Test<Type> ({ 2, 3, 2, 0 });
	Test<Type> ({ 2, 3, 2, 10 });
	Test<Type> ({ 2, -3, 2, -10 });

	if (argc < 2)
	{
		std::cout << "Usage: " << argv [0] << " datafile [threadCount]" << std::endl;
		return 1;
	}

	size_t threadCount = 0;
	if (argc > 2)
		threadCount = boost::lexical_cast<size_t> (argv [2]);

	const std::string infile (argv [1]);
	const auto& pairs = LoadData (infile);

	std::vector<double> lVars;
	for (double i = 0; i < 1e-3; i += 1e-4)
		lVars.push_back (i);
	for (double i = 1e-3; i < 1e-2; i += 1e-3)
		lVars.push_back (i);

	std::vector<double> nVars;
	for (double i = 0; i < 1e-4; i += 1e-5)
		nVars.push_back (i);
	for (double i = 1e-4; i < 1e-3; i += 1e-4)
		nVars.push_back (i);

	const Interpolator<Type> srcInterp { pairs };
	std::cout << "Derived polynome:" << std::endl;
	PrintCoeffs (std::cout, VecToDouble (srcInterp.GetResult ())) << std::endl;

	auto results = calcStats ([] (const TrainingSet_t& pts)
				{ return Interpolator<Type> { pts }.GetResultMat (); },
			lVars, nVars, pairs, threadCount);

	WriteCoeffs (srcInterp.GetResultMat (), results, infile);

	/*
	std::vector<Type> multi { };
	for (int i = 0; i < 17; ++i)
	{
		multi.push_back (600 + 10 * i);
		if (multi.size () >= 3)
		{
			Test (multi);
			PerfTest (multi);
		}
	}
	*/

	return 0;
}
