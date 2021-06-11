#include <iostream>
#include <algorithm>
#include <concepts>
#include <ranges>
#include "game.hpp"

AI::Filter auto heuristic = AI::maxFitness([](const Board& board, int player, int) {
	if (board.isWon()) return std::numeric_limits<int>::max();

	struct Set {
		size_t parent;
		size_t num_owned = 0; //how many of our own pieces are in this set
		bool threatened = false; //is this set threatened by an enemy explosion next turn?
		size_t num_threatened_by = 0; //Number of enemy pieces threatened by this set
	};

	auto fill = std::views::iota( 0, board.size() * board.size() * 8 ) | std::views::transform( [](size_t i) {return Set{i}; } );
	std::vector<Set> sets(fill.begin(),fill.end());

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

	auto merge = [&sets,&find](size_t a, size_t b) {
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
			if(!any_exploding_neighbor && is_player) count += board[c].num; //Count the piece as a single added thing but not part of any chain
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

int main() {
	std::default_random_engine random_initializer(std::random_device{}());

	BoardWithPlayers game(3);
	game.addPlayer(std::make_unique<AI::AIPlayer>(AI::firstSuccess(
		AI::filtered(heuristic, AI::randomAI(random_initializer)),
		AI::randomAI(random_initializer)
	)));
	game.addPlayer(std::make_unique<AI::AIPlayer>(AI::firstSuccess(
		AI::filtered(AI::maxGain, AI::randomAI(random_initializer)),
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