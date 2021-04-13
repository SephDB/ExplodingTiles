#pragma once

#include <random>
#include "board.hpp"

class Player {
public:
	virtual bool isMouseControlled() const { return false; }
	virtual void startTurn(const Board& b, int player_num) {}
	//Optional to allow for multi-frame calculations
	virtual std::optional<TriCoord> update() {
		return {};
	}
	virtual ~Player() = default;
};

class MousePlayer : public Player {
	bool isMouseControlled() const override { return true; }
};

class RandomAI : public Player {
public:
	RandomAI(std::random_device& e) : engine(e()) {}
	void startTurn(const Board& b, int player_num) override {
		std::vector<TriCoord> allowed_moves;
		b.iterTiles([&](TriCoord c) {
			if (b[c].player == player_num || b[c].num == 0) allowed_moves.push_back(c);
		});
		chosen = allowed_moves[std::uniform_int_distribution(0,static_cast<int>(allowed_moves.size()-1))(engine)];
	};
	std::optional<TriCoord> update() override { return chosen; }
private:
	std::default_random_engine engine;
	TriCoord chosen{};
};