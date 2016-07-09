#pragma once

#include <iammad/diff.h>
#include <iammad/parse.h>
#include <iammad/params.h>
#include <iammad/simplify.h>
#include "defs.h"

namespace detail
{
	using namespace Parse;

	template<char Family, size_t Index, typename Formula>
	struct Initializer : Initializer<Family, Index - 1, Formula>
	{
		template<typename Vec, typename Params>
		Initializer (const Vec& vec, Params& res)
		: Initializer<Family, Index - 1, Formula> { vec, res }
		{
			using Simplify::Simplify_t;
			res (Index - 1) = Simplify_t<VarDerivative_t<Formula, Node<Variable<Family, Index - 1>>>>::Eval (vec);
		}
	};

	template<char Family, typename Formula>
	struct Initializer<Family, 0, Formula>
	{
		template<typename Vec, typename Params>
		Initializer (Vec&, Params&)
		{
		}
	};

	template<typename Model, typename YSigmaGetter, typename XSigmaGetter>
	class WrappedModel
	{
		YSigmaGetter YSigma_;
		XSigmaGetter XSigma_;
	public:
		WrappedModel (YSigmaGetter ysigma, XSigmaGetter xsigma)
		: YSigma_ (ysigma)
		, XSigma_ (xsigma)
		{
		}

		static constexpr size_t ParamsCount = Model::ParamsCount;

		static std::array<DType_t, ParamsCount> initial ()
		{
			return Model::initial ();
		}

		static constexpr size_t BaseIndependentCount = Model::IndependentCount;
		static constexpr size_t WithSigmaCount = BaseIndependentCount + 2;

		using BaseFormula_t = typename Model::Formula_t;
		using BaseVarsDer_t = typename Model::VarsDer_t;
		static constexpr auto f = BaseFormula_t {};

		static constexpr auto y = Var<'Y'>;
		static constexpr auto ysigma = Var<'S'>;
		static constexpr auto xsigma = Var<'s'>;

		using Formula_t = decltype ((f - y) / Sqrt ((ysigma ^ _2) + ((BaseVarsDer_t {} * xsigma) ^ _2)));

		static auto buildParamsFunctor (const std::pair<SampleType_t<WithSigmaCount>, DType_t>& data, const Params_t<ParamsCount>& p)
		{
			const auto baseVec = Model::BindParams (data.first, p);
			const auto paramsVec = Params::BuildFunctor<DType_t> (y, data.second,
					ysigma, data.first (BaseIndependentCount),
					xsigma, data.first (BaseIndependentCount + 1));
			return Params::UniteFunctors (baseVec, paramsVec);
		}

		static DType_t residual (const std::pair<SampleType_t<WithSigmaCount>, DType_t>& data, const Params_t<ParamsCount>& p)
		{
			return Formula_t::Eval (buildParamsFunctor (data, p));
		}

		static Params_t<ParamsCount> residualDer (const std::pair<SampleType_t<WithSigmaCount>, DType_t>& data, const Params_t<ParamsCount>& p)
		{
			const auto& vec = buildParamsFunctor (data, p);

			using Simplify::Simplify_t;

			Params_t<ParamsCount> res;
			Initializer<'w', ParamsCount, Formula_t> { vec, res };
			return res;
		}

		TrainingSet_t<WithSigmaCount> preprocess (const TrainingSet_t<>& srcPts) const
		{
			auto baseRes = Model::preprocess (srcPts);

			TrainingSet_t<WithSigmaCount> res;
			for (const auto& srcPt : baseRes)
			{
				const auto val = srcPt.first;

				SampleType_t<WithSigmaCount> pt;
				for (size_t i = 0; i < BaseIndependentCount; ++i)
					pt (i) = srcPt.first (i);

				pt (BaseIndependentCount) = YSigma_ (srcPt);
				pt (BaseIndependentCount + 1) = XSigma_ (srcPt);

				res.emplace_back (pt, srcPt.second);
			}
			return res;
		}
	};
}

template<typename Model, typename YSigmaGetter, typename XSigmaGetter>
auto WrapModel (YSigmaGetter ysigma, XSigmaGetter xsigma)
{
	return detail::WrappedModel<Model, YSigmaGetter, XSigmaGetter> { ysigma, xsigma };
}
