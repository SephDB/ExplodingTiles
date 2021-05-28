#include <iostream>
#include <algorithm>
#include <concepts>
#include "game.hpp"

template<typename F>
concept Filter = std::is_invocable_r_v<std::vector<TriCoord>, F, const Board&, std::span<TriCoord>, int /*player*/>;

AI::AIFunc auto filtered(Filter auto filter, AI::AIFunc auto next) {
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

			return fitness(test,player,num);
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

Filter auto maxGain = filterIncludeMoves(explodingFilter) | maxFitness([](auto& board, int player, int) {
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
			} else if (std::ranges::none_of(critical, std::identity{})) {
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

int main() {
	std::default_random_engine random_initializer(std::random_device{}());

	BoardWithPlayers game(3);
	game.addPlayer(std::make_unique<AI::AIPlayer>(AI::firstSuccess(
		filtered(heuristic, AI::randomAI(random_initializer)),
		AI::randomAI(random_initializer)
	)));
	game.addPlayer(std::make_unique<AI::AIPlayer>(AI::firstSuccess(
		filtered(heuristic, AI::randomAI(random_initializer)),
		AI::randomAI(random_initializer)
	)));

	std::vector<int> wins(game.getPlayerCount());
	int total_game_steps = 0;
	constexpr int total_games = 1000;
	for (int i = 0; i < total_games; ++i) {
		game.reset();
		while (true) {
			game.update();
			total_game_steps++;
			if (auto win = game.getWinner(); win) {
				wins[*win]++;
				break;
			}
		}
		if (i % 100 == 99) {
			std::cout << i + 1 << ": ";
			for(auto w : wins) {
				std::cout << w << ' ';
			}
			std::cout << '\n';
		}
	}
	std::cout << "Total game steps: " << total_game_steps << '\n';
}