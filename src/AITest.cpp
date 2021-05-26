#include <iostream>
#include <algorithm>
#include "game.hpp"

AI::AIFunc auto biggestExplosion = [](const Board& b, std::span<TriCoord> moves, int player) -> std::optional<TriCoord> {
	auto explosionSize = [&](TriCoord m) {
		if (b[m].num == b.allowedPieces(m)) {
			Board test = b;
			test.incTile(m, player);
			int num = 0;
			while (test.needsUpdate() && num < 1000) {
				test.update_step();
				++num;
			}
			return num;
		}
		return 0;
	};

	auto max_explosion = std::ranges::max(moves, {}, explosionSize);

	if(explosionSize(max_explosion) > 0)
		return max_explosion;

	return std::nullopt;

};

template<class Pred, AI::AIFunc Next>
AI::AIFunc auto filterIncludeMoves(Pred pred, Next next) {
	return [=](const Board& b, std::span<TriCoord> moves, int player) -> std::optional<TriCoord> {
		std::vector<TriCoord> filtered_moves;
		std::ranges::copy_if(moves, std::back_inserter(filtered_moves), [&](auto c) {return pred(b, c, player); });
		if (filtered_moves.empty()) return std::nullopt;
		return next(b, filtered_moves, player);
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

int main() {
	std::default_random_engine random_initializer(std::random_device{}());

	BoardWithPlayers game(3);
	game.addPlayer(std::make_unique<AI::AIPlayer>(AI::firstSuccess(
		filterIncludeMoves(explodingFilter, AI::randomAI(random_initializer)),
		filterIncludeMoves(notNextToExploding, AI::randomAI(random_initializer)),
		AI::randomAI(random_initializer)
	)));
	game.addPlayer(std::make_unique<AI::AIPlayer>(AI::firstSuccess(
		biggestExplosion,
		AI::randomAI(random_initializer)
	)));

	int total_1_wins = 0;
	int total_game_steps = 0;
	constexpr int total_games = 1000;
	for (int i = 0; i < total_games; ++i) {
		game.reset();
		while (true) {
			game.update();
			total_game_steps++;
			if (auto win = game.getWinner(); win) {
				total_1_wins += *win;
				break;
			}
		}
		if (i % 100 == 99) {
			std::cout << i + 1 << ": " << i - total_1_wins + 1 << '-' << total_1_wins << '\n';
		}
	}
	std::cout << "Player 0 wins: " << total_games - total_1_wins << '\n';
	std::cout << "Player 1 wins: " << total_1_wins << '\n';
	std::cout << "Total game steps: " << total_game_steps << '\n';
}