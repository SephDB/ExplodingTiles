#pragma once

#include <vector>
#include <SFML/System/Vector2.hpp>
#include "vectorops.hpp"

template<std::size_t Degree>
struct Bezier {
	//first control point fixed to origin
	std::array<sf::Vector2f, Degree> points;

	sf::Vector2f value(float t) const {
		Bezier<Degree - 1> lowered;
		sf::Vector2f offset = points[0] * t;
		for (std::size_t i = 0; i < Degree - 1; ++i) {
			lowered.points[i] = lerp(points[i], points[i + 1], t) - offset;
		}
		return lowered.value(t) + offset;
	}

	sf::Vector2f tangent(float t) const {
		Bezier<Degree - 1> lowered;
		const float order_f = static_cast<float>(Degree);
		sf::Vector2f offset = points[0] * order_f;
		for (std::size_t i = 0; i < Degree - 1; ++i) {
			lowered.points[i] = (points[i + 1] - points[i]) * order_f - offset;
		}
		return normalized(lowered.value(t) + offset);
	}
};

template<>
struct Bezier<1> {
	std::array<sf::Vector2f, 1> points;

	sf::Vector2f value(float t) const {
		return points[0] * t;
	}
};

using CubeSpline = Bezier<3>;

/**
* Stitches CubeSplines together by their endpoints, properly aligning the tangent values for smooth interpolation
*/
class PolyCubeBezier {
	std::vector<sf::Vector2f> control_points; //currently grouped by three, will expand later

	std::pair<sf::Vector2f,CubeSpline> get(std::size_t n) const {
		CubeSpline ret;
		sf::Vector2f offset{};
		if (n > 0) {
			offset = control_points[n * 3 - 1];
		}
		std::transform(control_points.begin() + n * 3, control_points.begin() + (n + 1) * 3, ret.points.begin(), [&](auto point) {return point - offset; });
		return { offset,ret };
	}

	std::size_t num_curves() const {
		return control_points.size() / 3;
	}

public:
	PolyCubeBezier(CubeSpline s) : control_points(s.points.begin(),s.points.end()) {
	}

	PolyCubeBezier& addSpline(CubeSpline s) {
		auto end_tangent = tangent(1);
		auto start_tangent = s.tangent(0);
		auto cos = dot(end_tangent, start_tangent);
		auto sin = (end_tangent.x * start_tangent.y - end_tangent.y * start_tangent.x);
		//rotates the start tangent of the new spline onto the end tangent of the current end
		sf::Transform rot = sf::Transform(cos,-sin,0,
		                                  sin,cos,0,
		                                  0,0,1);

		sf::Vector2f offset = control_points.back();

		for (const auto& p : s.points) {
			control_points.push_back(rot.transformPoint(p) + offset);
		}
		
		return *this;
	}

	sf::Vector2f value(float t) const {
		int n = static_cast<int>(t * num_curves());
		if (n == num_curves()) n--;

		t = inverseLerp<float>(n, n + 1, t * num_curves());

		auto [offset, spline] = get(n);

		return spline.value(t) + offset;
	}

	sf::Vector2f tangent(float t) const {
		int n = static_cast<int>(t * num_curves());
		if (n == num_curves()) n--;

		t = inverseLerp<float>(n, n + 1, t * num_curves());
		return get(n).second.tangent(t);
	}

};


sf::VertexArray curve_to_strip(const auto& curve, float thickness, std::size_t num_points = 100) {
	sf::VertexArray ret(sf::PrimitiveType::TrianglesStrip, num_points*2);
	for (std::size_t i = 0; i < num_points; ++i) {
		const float t = static_cast<float>(i) / (num_points - 1);
		const auto midpoint = curve.value(t);
		const auto tangent = curve.tangent(t);
		const auto normal = sf::Vector2f{ -tangent.y,tangent.x };
		ret[i * 2] = midpoint + normal * thickness;
		ret[i * 2 + 1] = midpoint - normal * thickness;
	}
	return ret;
}
