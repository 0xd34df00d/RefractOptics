#include "util.h"

TrainingSet_t LoadData (const std::string& file)
{
	TrainingSet_t pairs;

	std::ifstream istr (file);
	while (istr)
	{
		double lambda, n;
		char c;
		istr >> lambda >> c >> n;
		if (!istr)
			break;

		SampleType_t sample;
		sample (0) = lambda;

		pairs.push_back ({ sample, n });
	}

	return pairs;
}

namespace
{
	std::string format (double value)
	{
		std::ostringstream ostr;
		ostr.precision (3);
		if (value > 1)
		{
			ostr << value;
			return ostr.str ();
		}
		if (value < 1e-7)
			return "0";

		int count = 0;
		if (value < 0.001)
			while (value < 1)
			{
				value *= 10;
				++count;
			}

		if (value != 1 || !count)
		{
			ostr << value;
			if (count)
				ostr << " \\cdot ";
		}
		if (count)
		{
			ostr << "10^{-";
			ostr << count;
			ostr << "}";
		}
		return ostr.str ();
	}
}

void WriteTeX (size_t paramsCount, const std::vector<double>& xVars, const std::vector<double>& yVars, Stats_t results)
{
	for (size_t i = 0; i < paramsCount; ++i)
	{
		std::cout << "\\begin{tabular}{| l ";
		for (size_t i = 0; i < yVars.size (); ++i)
			std::cout << "| l ";
		std::cout << "|} \\hline\n";
		for (auto nVar : yVars)
			std::cout << " & $" << format (nVar) << "$";
		std::cout << "\\\\ \\hline\n";

		std::cout.precision (3);

		for (auto lVar : xVars)
		{
			std::cout << format (lVar);
			for (auto nVar : yVars)
			{
				const auto& stats = results [lVar] [nVar];
				std::cout << " & ";
				std::cout << "$\\displaystyle (" << format (stats [i].mean ()) << "; " << format (stats [i].stddev ()) << ")$";
			}
			std::cout << "\\\\ \\hline\n";
		}
		std::cout << "\\end{tabular}" << std::endl << std::endl;
	}
}
