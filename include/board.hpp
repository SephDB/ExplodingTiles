#pragma once

#include <vector>
#include <numeric>
#include <algorithm>
#include <span>
#include <optional>
#include "coords.hpp"

struct TileState {
	int player = -1;
	int num = 0;
};

class Board {
	std::vector<TileState> _state;
	std::vector<TriCoord> _exploding;
	std::vector<int> _totals;
	int _size;

	TileState& get(TriCoord c) {
		return _state[c.x * 2 + c.y * _size * 4 + c.R];
	}

public:
	Board() = default;
	Board(int size) : _state(size* size * 8), _size(size) {}

	std::span<const int> playerTotals() const { return _totals; }

	std::optional<int> isWon() const {
		if (std::ranges::count_if(_totals, [](auto e) {return e != 0; }) == 1) {
			auto winner = std::ranges::find_if(_totals, [](auto e) {return e > 1; });
			if (winner != _totals.end()) {
				return winner - _totals.begin();
			}
		}
		return {};
	}

	bool inBounds(TriCoord c) const {
		auto b = c.bary(_size);
		auto [min, max] = std::minmax({ b.x,b.y,b.z });
		return min >= 0 && max < _size * 2;
	}

	bool isEdge(TriCoord c) const {
		auto b = c.bary(_size);
		auto [min, max] = std::minmax({ b.x,b.y,b.z });
		//Up edge triangles have a coord of 0, down edge triangles have a max coord
		return c.R ? (max == _size * 2 - 1) : (min == 0);
	}

	int allowedPieces(TriCoord c) const {
		return 2 - isEdge(c);
	}

	bool needsUpdate() const {
		return !_exploding.empty();
	}

	int size() const {
		return _size;
	}

	TileState operator[](TriCoord c) const {
		return _state[c.x * 2 + c.y * _size * 4 + c.R];
	}

	TileState at(TriCoord c) const {
		if (inBounds(c)) return (*this)[c];
		return {};
	}

	bool incTile(TriCoord c, int player, bool replace = false) {
		if (!inBounds(c))
			return false;

		TileState& s = get(c);

		if (!replace && s.player >= 0 && s.player != player) return false;

		if (std::size_t(player) >= _totals.size()) _totals.resize(player+1,0);

		if(!replace) _totals[player]++; //Only add one to the total if it was a move done by the player instead of an explosion

		if (s.player != player && s.player >= 0) {
			_totals[s.player] -= s.num;
			_totals[player] += s.num;
		}
		s.player = player;
		s.num++;
		if (s.num > allowedPieces(c)) _exploding.push_back(c);
		return true;
	}

	void update_step() {
		std::vector<TriCoord> old_exploding;
		std::swap(old_exploding, _exploding);

		for (auto& c : old_exploding) {
			TileState& s = get(c);
			if (s.num <= allowedPieces(c)) continue;
			for (auto& n : c.neighbors()) {
				s.num -= incTile(n, s.player, true);
			}
			if (s.num == 0) s.player = -1;
		}
	}

	template<typename F>
	void iterTiles(F f) const {
		for (int x = 0; x < _size*2; ++x) {
			for (int y = 0; y < _size*2; ++y) {
				if (inBounds({ x,y,false }))
					if (!f(TriCoord{ x,y,false })) return;
				if (inBounds({ x,y,true }))
					if (!f(TriCoord{ x,y,true })) return;
			}
		}
	}
};