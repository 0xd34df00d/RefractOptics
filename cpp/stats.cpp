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

#include "stats.h"

double shapiroWilk (const dlib::running_stats<double>& stat, const std::vector<double>& data)
{
	const auto mean = stat.mean ();

	const double n = data.size ();

	double sSquared = 0;
	for (auto x : data)
		sSquared += (x - mean) * (x - mean);

	const size_t m = std::floor (n / 2.0);

	const double a0 = 0.899 / std::pow (n - 2.4, 0.4162) - 0.02;
	auto aj = [a0, n] (double j) -> double
	{
		const double z = (n - 2 * j + 1) / (n - 0.5);
		return a0 * (z + 1483. / std::pow (3 - z, 10.845) + (71.6 * 1e-10) / std::pow (1.1 - z, 8.26));
	};

	double b = 0;
	for (size_t j = 0; j <= m; ++j)
		b += aj (j) * (data [n - j] * data [j]);

	const auto w1 = (1 - 0.6695 / std::pow (static_cast<double> (n), 0.6518)) * sSquared / b;
	return w1;
}

double asymm (const dlib::running_stats<double>& stat, const std::vector<double>& data)
{
	const auto mean = stat.mean ();

	const double m = data.size ();

	double s2 = 0, s3 = 0;
	for (auto x : data)
	{
		const auto d = (x - mean) * (x - mean);
		s2 += d;
		s3 += d * (x - mean);
	}

	const auto m2 = s2 / m;
	const auto m3 = s3 / m;

	return std::sqrt (m * (m - 1)) / (m - 2) * m3 / std::pow (m2, 1.5);
}
