#pragma once

#include "solve.h"

TrainingSet_t LoadData (const std::string& file);

template<typename Params>
void WriteCoeffs (const Params& p, const Stats_t& results, const std::string& infile)
{
	for (size_t i = 0; i < p.size (); ++i)
	{
		std::stringstream fname;
		fname << infile << "_coeff" << i << ".dat";

		std::ofstream ostr (fname.str ());
		for (auto lIt = results.begin (); lIt != results.end (); ++lIt)
		{
			const auto lVar = lIt->first;

			for (auto nIt = lIt->second.begin (); nIt != lIt->second.end (); ++ nIt)
			{
				const auto nVar = nIt->first;
				const auto& stats = nIt->second;
				ostr << lVar * 1000 << " " << nVar * 1000 << " " << stats [i].stddev () / (std::abs (p (i)) + 1e-12) * 1000  << std::endl;
			}
			ostr << std::endl;
		}
		std::cout << "wrote " << fname.str () << std::endl;
	}
}

void WriteTeX (size_t paramsCount, const std::vector<double>& xVars, const std::vector<double>& yVars, Stats_t stats);
