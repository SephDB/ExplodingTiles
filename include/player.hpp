#pragma once

#include <random>
#include <tuple>
#include <span>
#include <concepts>
#include <optional>
#include <functional>
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

namespace AI {

	using AIFunction = std::function<std::optional<TriCoord>(const Board&, std::span<TriCoord>, int)>;

	class AIPlayer : public Player {
		AIFunction f;
		TriCoord chosen{};
	public:
		AIPlayer(AIFunction strat) : f(std::move(strat)) {}
		void startTurn(const Board& b, int player_num) override {
			std::vector<TriCoord> allowed_moves;
			b.iterTiles([&](TriCoord c) {
				if (b[c].player == player_num || b[c].num == 0) allowed_moves.push_back(c);
				return true;
			});
			chosen = *f(b, allowed_moves, player_num);
		}
		std::optional<TriCoord> update() override {
			return chosen;
		}
	};

	template<typename F>
	concept AIFunc = std::convertible_to<F,AIFunction>;

	template<AIFunc... Fs>
	AIFunc auto firstSuccess(Fs... strats) {
		return [=](const Board& b, std::span<TriCoord> moves, int player) {
			std::optional<TriCoord> m{};
			((m = strats.findMove(b, moves, player)) || ...);
			return *m;
		};
	}

	AIFunc auto randomAI(std::random_device& random) {
		return [engine = std::default_random_engine(random())](const Board&, std::span<TriCoord> moves, int) mutable {
			return moves[std::uniform_int_distribution(0, static_cast<int>(moves.size() - 1))(engine)];
		};
	}
}
