#pragma once

#include <SFML/System/Vector2.hpp>
#include <SFML/System/Vector3.hpp>
#include <array>

struct TriCoord {
	TriCoord() = default;
	TriCoord(sf::Vector3i bary, int hex_size) : x(bary.x), y(bary.y), R(bary.x + bary.y + bary.z == hex_size * 3 - 2) {}
	TriCoord(int x, int y, bool R) : x(x), y(y), R(R) {}
	int x, y;
	bool R;

	std::array<TriCoord, 3> neighbors() const {
		int offset = R ? 1 : -1;
		return { { {x,y,!R},{x + offset,y,!R},{x,y + offset,!R} } };
	}

	sf::Vector3f tri_center(int hex_size) const {
		float a = (x + (1 + R) / 3.f) / (hex_size * 3);
		float b = (y + (1 + R) / 3.f) / (hex_size * 3);

		return { a, b, 1 - a - b };
	}

	sf::Vector3i bary(int hex_size) const {
		return { x, y, hex_size * 3 - 1 - x - y - R };
	}
};