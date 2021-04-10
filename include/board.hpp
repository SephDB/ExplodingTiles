#pragma once

#include <vector>
#include <numeric>
#include <algorithm>
#include "coords.hpp"

struct TileState {
	int player = -1;
	enum class State {
		NONE,
		ONE,
		TWO,
		EXPLODE
	} state = State::NONE;
};

class Board {
	std::vector<TileState> _state;
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

	bool needs_update() const {
		return std::any_of(_state.begin(), _state.end(), [](auto a) {return a.state == TileState::State::EXPLODE; });
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
		switch (s.state)
		{
		case TileState::State::NONE:
			s.state = TileState::State::ONE;
			break;
		case TileState::State::ONE:
			if (is_edge(c)) {
				s.state = TileState::State::EXPLODE;
			}
			else
			{
				s.state = TileState::State::TWO;
			}
			break;
		case TileState::State::TWO:
			s.state = TileState::State::EXPLODE;
			break;
		default:
			break;
		}
		return true;
	}

};