#pragma once

#include <SFML/System/Vector2.hpp>

sf::Vector2f normalized(sf::Vector2f v) {
	return v / std::sqrt(v.x*v.x + v.y*v.y);
}

float dot(sf::Vector2f a, sf::Vector2f b) {
	return a.x * b.x + a.y * b.y;
}

template<typename Vec>
Vec lerp(Vec a, Vec b, float val) {
	return a + (b - a) * val;
}

template<typename Vec>
float inverseLerp(Vec a, Vec b, Vec val) {
	return (val - a) / (b - a);
}
