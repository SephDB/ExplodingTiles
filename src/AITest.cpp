#include <iostream>
#include <algorithm>
#include <concepts>
#include "game.hpp"

int main() {
	std::default_random_engine random_initializer(std::random_device{}());

	BoardWithPlayers game(3);
	game.addPlayer(std::make_unique<AI::AIPlayer>(AI::firstSuccess(
		AI::filtered(AI::heuristic, AI::randomAI(random_initializer)),
		AI::randomAI(random_initializer)
	)));
	game.addPlayer(std::make_unique<AI::AIPlayer>(AI::firstSuccess(
		AI::filtered(AI::heuristic, AI::randomAI(random_initializer)),
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