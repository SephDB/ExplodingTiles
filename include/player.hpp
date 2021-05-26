#pragma once

#include <random>
#include <tuple>
#include <span>
#include <concepts>
#include <optional>
#include <functional>
#include <variant>
#include <SFML/System/Clock.hpp>
#include "board.hpp"

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...)->overloaded<Ts...>;

namespace input_events {
	struct MouseMove {
		TriCoord position;
	};
	struct MouseClick {
		TriCoord position;
	};
	using Event = std::variant<MouseMove, MouseClick>;
}

class Player {
public:
	virtual void startTurn(const Board& b, int player_num) {}
	virtual void onInput(const input_events::Event& e) {}
	virtual TriCoord selected() const { return {}; }
	//Optional to allow for multi-frame calculations
	virtual std::optional<TriCoord> update() {
		return {};
	}
	virtual ~Player() = default;
};

class MousePlayer : public Player {
	TriCoord select{};
	bool clicked_mouse = false;
public:
	void onInput(const input_events::Event& e) override {
		std::visit(overloaded{
			[this](input_events::MouseMove x) {
				select = x.position;
			},
			[this](input_events::MouseClick x) {
				select = x.position;
				clicked_mouse = true;
			},
			[](auto) {}
		}, e);
	}

	TriCoord selected() const override {
		return select;
	}

	std::optional<TriCoord> update() override {
		if (clicked_mouse) {
			clicked_mouse = false;
			return select;
		}
		return {};
	}
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
		TriCoord selected() const override {
			//TODO: return 0 if haven't received result yet
			return chosen;
		}
		std::optional<TriCoord> update() override {
			return chosen;
		}
	};

	class InteractiveAIPlayer : public Player {
		static constexpr float interact_time = 0.3f;
		AIPlayer p;
		sf::Clock timer{};
	public:
		InteractiveAIPlayer(AIPlayer player) : p{ std::move(player) } {}
		void startTurn(const Board& b, int player_num) override {
			p.startTurn(b,player_num);
			timer.restart();
		}
		TriCoord selected() const override {
			return p.selected();
		}
		std::optional<TriCoord> update() override {
			if (timer.getElapsedTime().asSeconds() >= interact_time) {
				return p.update();
			}
			return {};
		}
	};

	template<typename F>
	concept AIFunc = std::convertible_to<F,AIFunction>;

	template<AIFunc... Fs>
	AIFunc auto firstSuccess(Fs... strats) {
		return [=](const Board& b, std::span<TriCoord> moves, int player) {
			std::optional<TriCoord> m{};
			((m = strats(b, moves, player)) || ...);
			return *m;
		};
	}

	AIFunc auto randomAI(std::default_random_engine& random) {
		return [engine = &random](const Board&, std::span<TriCoord> moves, int) {
			return moves[std::uniform_int_distribution(0, static_cast<int>(moves.size() - 1))(*engine)];
		};
	}
}

enum class PlayerType {
	Mouse,
	AIRando
};

std::unique_ptr<Player> toPlayer(PlayerType t) {
	static std::default_random_engine random_engine(std::random_device{}());
	switch (t)
	{
	case PlayerType::Mouse:
		return std::make_unique<MousePlayer>();
		break;
	case PlayerType::AIRando:
		return std::make_unique<AI::InteractiveAIPlayer>(AI::AIPlayer(AI::randomAI(random_engine)));
		break;
	}
	return nullptr;
}
