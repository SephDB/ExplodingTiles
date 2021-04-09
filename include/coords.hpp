#pragma once

#include <array>

struct TriCoord {
	int x, y;
	bool R;
};

std::array<TriCoord, 3> neighbors(TriCoord c) {
	int offset = c.R ? 1 : -1;
	return { { {c.x,c.y,!c.R},{c.x + offset,c.y,!c.R},{c.x,c.y + offset,!c.R} } };
}

template<typename Vec>
Vec to_bary(TriCoord c) {
	return Vec((c.x + 0.33f + c.R * 0.33f), (c.y + 0.33f + c.R * 0.33f));
}
