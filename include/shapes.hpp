#pragma once

#include <cmath>
#include <SFML/Graphics/VertexArray.hpp>


sf::VertexArray circArrow(sf::Vector2f center, sf::Color color, float inner, float outer, float pointextra, int num = 50) {
	const float tau = 2 * std::acosf(-1);
	
	sf::VertexArray ret(sf::PrimitiveType::TriangleStrip,num*2+3);

	auto getDir = [&](auto i) {
		float angle = i * tau / (num * 1.3f) - tau / 3;
		return sf::Vector2f{ -std::cos(angle),std::sin(angle) };
	};

	for (int i = 0; i < num; ++i) {
		auto dir = getDir(i);
		ret[i * 2] = { center + dir * inner, color };
		ret[i * 2 + 1] = { center + dir * outer, color };
	}

	auto dir = getDir(num - 1);
	ret[num * 2] = { center + dir * (inner - pointextra), color };
	ret[num * 2 + 1] = { center + dir * (outer + pointextra), color };
	auto extra_dir = sf::Transform().rotate(-90).transformPoint(dir);
	ret[num * 2 + 2] = { center + dir * (inner + outer) * 0.5f + extra_dir*inner*0.7f, color };

	return ret;
}