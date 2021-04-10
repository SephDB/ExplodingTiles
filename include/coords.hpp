#pragma once

#include <SFML/System/Vector2.hpp>
#include <SFML/System/Vector3.hpp>
#include <array>

struct TriCoord {
	TriCoord(sf::Vector3i bary, int hex_size) : x(bary.x), y(bary.y), R(bary.x + bary.y + bary.z == hex_size * 3 - 2) {}
	TriCoord(int x, int y, bool R) : x(x), y(y), R(R) {}
	int x, y;
	bool R;

	std::array<TriCoord, 3> neighbors() {
		int offset = R ? 1 : -1;
		return { { {x,y,!R},{x + offset,y,!R},{x,y + offset,!R} } };
	}

	sf::Vector2f tri_center() {
		return { x + (1 + R) / 3.f, y + (1 + R) / 3.f };
	}

	sf::Vector3i bary(int hex_size) {
		return { x, y, hex_size * 3 - 1 - x - y - R };
	}
};