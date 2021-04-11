#pragma once

#include <vector>
#include <numeric>
#include <algorithm>
#include "coords.hpp"

struct TileState {
	int player = -1;
	int num = 0;
};

class Board {
	std::vector<TileState> _state;
	std::vector<TriCoord> _exploding;
	int _size;
public:
	Board() = default;
	Board(int size) : _state(size* size * 8), _size(size) {}

	bool in_bounds(TriCoord c) const {
		auto b = c.bary(_size);
		auto [min, max] = std::minmax({ b.x,b.y,b.z });
		return min >= 0 && max < _size * 2;
	}

	bool is_edge(TriCoord c) const {
		auto neighbors = c.neighbors();
		return !std::all_of(neighbors.begin(), neighbors.end(), [this](auto c) {return in_bounds(c); });
	}

	int allowed_pieces(TriCoord c) const {
		return 2 - is_edge(c);
	}

	bool needs_update() const {
		return !_exploding.empty();
	}

	int size() const {
		return _size;
	}

	TileState operator[](TriCoord c) const {
		return _state[c.x * 2 + c.y * _size * 4 + c.R];
	}

	bool incTile(TriCoord c, int player) {
		if (!in_bounds(c))
			return false;

		TileState& s = _state[c.x * 2 + c.y * _size * 4 + c.R];

		if (s.player >= 0 && s.player != player) return false;

		s.player = player;
		s.num++;
		if (s.num > allowed_pieces(c)) _exploding = { c };
		return true;
	}

};