#pragma once

#include <vector>
#include <SFML/System/Vector2.hpp>
#include "vectorops.hpp"

struct CubeSpline {
	//first point is at 0,0
	std::array<sf::Vector2f, 3> control_points;

	sf::Vector2f value(float t) const {
		auto inter = [t](auto a, auto b) {return lerp(a, b, t); };
		std::array<sf::Vector2f, 3> first_order = {
			inter(sf::Vector2f(0,0),control_points[0]),
			inter(control_points[0],control_points[1]),
			inter(control_points[1],control_points[2])
		};
		auto second_a = inter(first_order[0], first_order[1]);
		auto second_b = inter(first_order[1], first_order[2]);
		return inter(second_a, second_b);
	}

	sf::Vector2f tangent(float t) const {
		std::array<sf::Vector2f, 3> quad_spline_derivative = {
			3.f * control_points[0],
			3.f * (control_points[1] - control_points[0]),
			3.f * (control_points[2] - control_points[1])
		};
		auto inter = [t](auto a, auto b) {return lerp(a, b, t); };
		auto second_a = inter(quad_spline_derivative[0], quad_spline_derivative[1]);
		auto second_b = inter(quad_spline_derivative[1], quad_spline_derivative[2]);
		return normalized(inter(second_a, second_b));
	}
};

/**
* Stitches CubeSplines together by their endpoints, properly aligning the tangent values for smooth interpolation
*/
class PolyCubeBezier {
	std::vector<CubeSpline> splines;
	std::vector<sf::Transform> alignment;
public:
	PolyCubeBezier(CubeSpline s) : splines{ s }, alignment{ sf::Transform() } {
	}

	PolyCubeBezier& addSpline(CubeSpline s) {
		auto end_tangent = splines.back().tangent(1);
		auto start_tangent = s.tangent(0);
		auto cos = dot(end_tangent, start_tangent);
		auto sin = (end_tangent.x * start_tangent.y - end_tangent.y * start_tangent.x);
		//rotates the start tangent of the new spline onto the end tangent of the current end
		sf::Transform rot = sf::Transform(cos,-sin,0,
		                                  sin,cos,0,
		                                  0,0,1);
		sf::Transform current = alignment.back();
		current.translate(alignment.back().transformPoint(splines.back().value(1))).combine(rot);
		splines.push_back(s);
		alignment.push_back(current);
		
		return *this;
	}

	sf::Vector2f value(float t) const {
		int n = static_cast<int>(t * splines.size());
		if (n == splines.size()) n--;

		float t2 = inverseLerp<float>(n,n+1,t*splines.size());

		return alignment[n].transformPoint(splines[n].value(t2));
	}

	sf::Vector2f tangent(float t) const {
		int n = static_cast<int>(t * splines.size());
		if (n == splines.size()) n--;
		auto translation = alignment[n].transformPoint(sf::Vector2f(0,0));

		t = inverseLerp<float>(n, n + 1, t*splines.size());
		return alignment[n].transformPoint(splines[n].tangent(t)) - translation;
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
