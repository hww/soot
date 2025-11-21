#pragma once
#include <cmath>

namespace vm
{
	inline bool compare_float(float f1, float f2) 
	{
		static constexpr auto epsilon = 1.0e-05f;
		if (fabs(f1 - f2) <= epsilon)
			return true;
		return fabs(f1 - f2) <= epsilon * fmax(fabs(f1), fabs(f2));
	}
}
