#pragma once

#include <random>
#include <tuple>
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



class AIStrat {
public:
	virtual std::optional<TriCoord> findMove(const Board& b, const std::vector<TriCoord>& allowed_moves, int player_num) = 0;
};

class RandomAIStrat final : public AIStrat {
public:
	explicit RandomAIStrat(std::random_device& e) : engine(e()) {}
	std::optional<TriCoord> findMove(const Board& b, const std::vector<TriCoord>& allowed_moves, int player_num) override {
		return allowed_moves[std::uniform_int_distribution(0, static_cast<int>(allowed_moves.size() - 1))(engine)];
	}
private:
	std::default_random_engine engine;
};

template<typename... Strats>
TriCoord findFirstSuccess(const Board& b, const std::vector<TriCoord>& allowed_moves, int player_num, Strats&&... strats) {
	std::optional<TriCoord> m{};
	((m = strats.findMove(b, allowed_moves, player_num)) || ...);
	return *m;
}

template<typename... Strats>
class FirstSuccessAI : public Player {
	std::tuple<Strats...> strats;
	TriCoord chosen{};
public:
	FirstSuccessAI(Strats... s) : strats(std::move(s)...) {}
	void startTurn(const Board& b, int player_num) override {
		std::vector<TriCoord> allowed_moves;
		b.iterTiles([&](TriCoord c) {
			if (b[c].player == player_num || b[c].num == 0) allowed_moves.push_back(c);
			return true;
		});
		chosen = findFirstSuccess(b, allowed_moves, player_num, std::get<Strats>(strats)...);
	};
	std::optional<TriCoord> update() override { return chosen; }
};
