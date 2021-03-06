#pragma once

#include <random>
#include <tuple>
#include <span>
#include <concepts>
#include <optional>
#include <functional>
#include <variant>
#include <ranges>
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
	
	template<typename F>
	concept AIFunc = std::is_invocable_r_v<std::optional<TriCoord>,F, const Board&, std::span<TriCoord>, int>;
	
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

	

	AIFunc auto firstSuccess(AIFunc auto... strats) {
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

	template<typename F>
	concept Filter = std::is_invocable_r_v<std::vector<TriCoord>, F, const Board&, std::span<TriCoord>, int /*player*/>;

	AIFunc auto filtered(Filter auto filter, AI::AIFunc auto next) {
		return [=](const Board& b, std::span<TriCoord> moves, int player)->std::optional<TriCoord> {
			auto filtered = filter(b, moves, player);
			if (filtered.empty()) return {};
			return next(b, filtered, player);
		};
	}

	Filter auto operator|(Filter auto a, Filter auto b) {
		return [=](const Board& board, std::span<TriCoord> moves, int player) {
			auto filtered = a(board, moves, player);
			return b(board, filtered, player);
		};
	}

	template<typename F>
	concept Fitness = std::is_invocable_r_v<int, F, const Board&, int /*player*/, int /*num_updates*/>;

	Filter auto maxFitness(Fitness auto fitness) {
		return [=](const Board& b, std::span<TriCoord> moves, int player) {
			auto fitnessEvaluator = [&](TriCoord m) {
				Board test = b;
				test.incTile(m, player);
				int num = 0;
				while (test.needsUpdate() && !test.isWon()) {
					test.update_step();
					++num;
				}

				return fitness(test, player, num);
			};

			int max = std::numeric_limits<int>::min();
			std::vector<TriCoord> out;
			for (auto& c : moves) {
				auto val = fitnessEvaluator(c);
				if (val > max) {
					out.clear();
					max = val;
				}
				if (val == max) {
					out.push_back(c);
				}
			}
			return out;
		};
	}

	Filter auto filterIncludeMoves(auto pred) {
		return [=](const Board& b, std::span<TriCoord> moves, int player) {
			std::vector<TriCoord> filtered_moves;
			std::ranges::copy_if(moves, std::back_inserter(filtered_moves), [&](auto c) {return pred(b, c, player); });
			return filtered_moves;
		};
	}

	bool explodingFilter(const Board& b, TriCoord c, int) {
		return b[c].num == b.allowedPieces(c);
	}

	bool notNextToExploding(const Board& b, TriCoord c, int player) {
		return std::ranges::none_of(c.neighbors(), [&](TriCoord loc) {
			return b.inBounds(loc) && b[loc].player != player && b.allowedPieces(loc) == b[loc].num;
			});
	}

	Filter auto biggestExplosion = filterIncludeMoves(explodingFilter) | maxFitness([](auto&, int, int num) {return num; });

	Filter auto maxGain = maxFitness([](auto& board, int player, int) {
		return board.playerTotals()[player];
	});

	Filter auto heuristic = maxFitness([](const Board& board, int player, int) {
		if (board.isWon()) return std::numeric_limits<int>::max();

		int count = 0;
		board.iterTiles([&](TriCoord c) {
			if (board[c].player == player) {
				count += board[c].num;
				std::array<bool, 3> critical;
				std::ranges::transform(c.neighbors(), critical.begin(), [&](auto n) {return board.inBounds(n) && board[n].player != player && board[n].num == board.allowedPieces(n); });
				if (std::ranges::any_of(critical, std::identity{})) {
					//this tile can easily get taken over by the other player's next move
					count -= 5 + (board[c].num == board.allowedPieces(c)) * 3;
				}
				else if (std::ranges::none_of(critical, std::identity{})) {
					//own a non-threatened tile
					count += 3;
					if (board[c].num == board.allowedPieces(c)) {
						//2 for edge tile, 1 for regular tile
						count += (3 - board[c].num);
						//amount of pieces directly threatened
						count += std::ranges::count_if(c.neighbors(), [&](auto n) {return board.at(n).num; });
					}
				}
			}
			return true;
			});
		return count;
	});

	Filter auto chains_heuristic = maxFitness([](const Board& board, int player, int) {
		if (board.isWon()) return std::numeric_limits<int>::max();

		struct Set {
			size_t parent;
			size_t num_owned = 0; //how many of our own pieces are in this set
			bool threatened = false; //is this set threatened by an enemy explosion next turn?
			size_t num_threatened_by = 0; //Number of enemy pieces threatened by this set
		};

		auto fill = std::views::iota(0, board.size() * board.size() * 8) | std::views::transform([](size_t i) {return Set{ i }; });
		std::vector<Set> sets(fill.begin(), fill.end());

		auto coord_to_set = [&board](TriCoord c) -> size_t {
			return c.x * 2 + c.y * board.size() * 4 + c.R;
		};

		auto parent = [&sets](size_t x) -> size_t& {
			return sets[x].parent;
		};

		auto find = [&parent](size_t x) {
			while (parent(x) != x) {
				parent(x) = parent(parent(x));
				x = parent(x);
			}
			return x;
		};

		auto merge = [&sets, &find](size_t a, size_t b) {
			a = find(a);
			b = find(b);
			if (a == b) return;
			sets[b].parent = a;
			sets[a].num_owned += sets[b].num_owned;
			sets[a].threatened |= sets[b].threatened;
			sets[a].num_threatened_by += sets[b].num_threatened_by;
		};

		int count = 0;
		board.iterTiles([&](TriCoord c) {
			if (board[c].num == board.allowedPieces(c)) {
				size_t s = coord_to_set(c);
				if (board[c].player == player) sets[s].num_owned = board[c].num;
				else {
					sets[s].threatened = true;
				}

				for (auto& neighbor : c.neighbors()) {
					if (not board.inBounds(neighbor)) continue;

					auto neighbor_set = coord_to_set(neighbor);
					if (neighbor_set < s) {
						merge(neighbor_set, s);
					}
				}
			}
			else if (board[c].num > 0) {
				bool any_exploding_neighbor = false;
				bool is_player = board[c].player == player;
				for (auto& neighbor : c.neighbors()) {
					if (not board.inBounds(neighbor) || board[neighbor].num != board.allowedPieces(neighbor)) continue;
					any_exploding_neighbor = true;
					if (!is_player) {
						size_t neighbor_set = find(coord_to_set(neighbor));
						sets[neighbor_set].num_threatened_by += board[c].num;
					}
				}
				if (!any_exploding_neighbor && is_player) count += board[c].num; //Count the piece as a single added thing but not part of any chain
			}
			return true;
			});

		auto all_sets = std::views::iota(size_t{ 0 }, sets.size())
			| std::views::filter([&](size_t s) {return sets[s].parent == s; });

		for (size_t s : all_sets) {
			auto& set = sets[s];
			count += set.threatened ? set.num_owned * -7 : set.num_owned * 3 + set.num_threatened_by * 2;
		}

		return count;
	});
}

enum class PlayerType {
	Mouse,
	AIRando,
	AIGreedy,
	AISmart
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
	case PlayerType::AIGreedy:
		return std::make_unique<AI::InteractiveAIPlayer>(AI::AIPlayer(
				AI::filtered(AI::maxGain, AI::randomAI(random_engine))
		));
		break;
	case PlayerType::AISmart:
		return std::make_unique<AI::InteractiveAIPlayer>(AI::AIPlayer(
				AI::filtered(AI::chains_heuristic, AI::randomAI(random_engine))
		));
		break;
	}
	return nullptr;
}
